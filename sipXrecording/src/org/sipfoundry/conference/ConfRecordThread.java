/*
 *
 *
 * Copyright (C) 2009 Pingtel Corp., certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 *
 */
package org.sipfoundry.conference;

import java.io.File;
import java.io.IOException;
import java.util.Date;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.apache.log4j.Logger;
import org.sipfoundry.commons.confdb.Conference;
import org.sipfoundry.commons.freeswitch.ConfBasicThread;
import org.sipfoundry.commons.freeswitch.ConferenceMember;
import org.sipfoundry.commons.freeswitch.ConferenceTask;
import org.sipfoundry.commons.freeswitch.FreeSwitchEvent;
import org.sipfoundry.commons.hz.HzConfEvent;
import org.sipfoundry.commons.hz.HzConstants;
import org.sipfoundry.commons.hz.HzPublisherTask;
import org.sipfoundry.commons.userdb.User;
import org.sipfoundry.commons.util.UnfortunateLackOfSpringSupportFactory;
import org.sipfoundry.sipxrecording.RecordCommand;
import org.sipfoundry.sipxrecording.RecordingConfiguration;


public class ConfRecordThread extends ConfBasicThread {
    static final Logger LOG = Logger.getLogger("org.sipfoundry.sipxrecording");

    static String sourceName = "/tmp/freeswitch/recordings";
    static String destName = System.getProperty("var.dir") + "/mediaserver/data/recordings";

    //TODO add localization support - port this project to spring maybe
    private static final String PARTICIPANT_ENTERED = "entered your conference as participant";
    private static final String PARTICIPANT_LEFT = "left your conference at";

    private ConferenceContextImpl m_conferenceContext;

    private ExecutorService m_executorService = Executors.newSingleThreadExecutor();

    public ConfRecordThread(RecordingConfiguration recordingConfig) {
        // Check that the freeswitch initial recording directory exists
        File sourceDir = new File(sourceName);
        if (!sourceDir.exists()) {
           sourceDir.mkdirs();
        }

        // Create the distribution directory if it doesn't already exist.
        File destDir = new File(destName);
        if (!destDir.exists()) {
            try {
                Process process = Runtime.getRuntime().exec(new String[] {"ln", "-s", sourceName, destName});
                process.waitFor();
                process.destroy();
            } catch (IOException e) {
                LOG.error("ConfRecordThread::IOException error ", e);
            } catch (InterruptedException e) {
                LOG.error("ConfRecordThread::InterruptedException error ", e);
            }
        }
        setConfConfiguration(recordingConfig);
    }

    private void AuditWavMp3Files(File dir) {
        // If any WAV/mp3 file on the directory is more than 5 minutes old then delete it.
        File[] children = dir.listFiles();
        if (children != null) {
            for (int i=0; i<children.length; i++) {
                File child = children[i];
                String filename = child.getName();
                if (filename.endsWith(".wav") || filename.endsWith(".mp3")) {
                   if ((System.currentTimeMillis() - child.lastModified())/1000 > 5*60) {
                       child.delete();
                       LOG.debug("ConfRecordThread::Audit deleting " + filename);
                   }
                }
            }
        }
    }

    @Override
    public void ProcessConfStart(FreeSwitchEvent event, ConferenceTask conf) {
        String confName = event.getEventValue("conference-name");
        User owner = UnfortunateLackOfSpringSupportFactory.getValidUsers().
            getUserByConferenceName(confName);
        if(owner != null) {
            conf.setOwner(owner);
        }
        Conference conference = m_conferenceContext.getConference(confName);
        if (conference != null && conference.isAutoRecord()) {
            String uniqueId = event.getEventValue("Unique-ID");
            String fileName = new String(confName + "_" + uniqueId + "." + m_conferenceContext.getAudioFormat());
            LOG.debug("ConfRecordThread::Creating conference recording " + fileName);
            String confCmd = "record " + sourceName + "/" + fileName;
            // Send the start recording command to Freeswitch
            RecordCommand recordcmd = new RecordCommand(getCmdSocket(), confName, confCmd);
            recordcmd.go();
            // Store the file name
            conf.setFileName(fileName);
        }
    }

    @Override
    public void ProcessConfUserAdd(ConferenceTask conf, ConferenceMember member) {
        User owner = conf.getOwner();
        if(owner != null && owner.getConfEntryIM()) {
            Date date = new Date();
            String instantMsg = member.memberName() + " (" + member.memberNumber() + ") " +
                PARTICIPANT_ENTERED + " [" + member.memberIndex() + "] at " + date.toString();
            try {
                if (owner.getConfEntryIM()) {
                    m_executorService.submit(new HzPublisherTask(
                        new HzConfEvent(member.memberNumber(), owner.getUserName(), instantMsg, HzConfEvent.ConfType.ENTER_CONFERENCE),
                        HzConstants.CONF_TOPIC));
                }
            } catch (Exception ex) {
                LOG.error("User conference enter::sendIM failed", ex);
            }
        }
    }

    public void ProcessConfUserDel(ConferenceTask conf, ConferenceMember member) {

        User owner = conf.getOwner();

        if(owner != null && owner.getConfExitIM()) {
            Date date = new Date();
            String instantMsg = member.memberName() + " (" + member.memberNumber() + ") " +
                PARTICIPANT_LEFT + " " + date.toString();
            try {
                if (owner.getConfExitIM()) {
                    if (owner.getConfEntryIM()) {
                        m_executorService.submit(new HzPublisherTask(
                            new HzConfEvent(member.memberNumber(), owner.getUserName(), instantMsg, HzConfEvent.ConfType.EXIT_CONFERENCE),
                            HzConstants.CONF_TOPIC));
                    }
                }
            } catch (Exception ex) {
                LOG.error("User conference exit::sendIM failed", ex);
            }
        }
    }

    @Override
    public void ProcessConfEnd(FreeSwitchEvent event, ConferenceTask conf) {
        final String confName = event.getEventValue("conference-name");
        Conference conference = m_conferenceContext.getConference(confName);
        String [] ivrUris =m_conferenceContext.getIvrUris();
        if (conference != null && conference.isAutoRecord()) {
            String fileName = conf.getFileName();
            LOG.debug("ConfRecordThread::Finished conference recording of " + fileName);

            try {
                // Let FS finish any recording it may be doing
                Thread.sleep(1000);
            } catch (InterruptedException e) {
            }

            AuditWavMp3Files(new File(destName));
            m_conferenceContext.notifyIvr(ivrUris, fileName, conference, false);
        }
        //verify if there is a temporary file recording given user action
        //the file, if any, has the same name as the conference
        //This is achieved on a separated thread to encourage current recording thread to die immediately
        //once the recording stopped. This shouldn't wait for the file to be copied in user mailbox
        Runnable r = new Runnable() {
            @Override
            public void run() {
                if (m_conferenceContext.isRecordingInProgress(confName)) {
                    m_conferenceContext.saveInMailboxSynch(confName);
                }
            }
        };
        Thread t = new Thread(r);
        t.start();
    }

    public void setConferenceContext(ConferenceContextImpl conferenceContext) {
        m_conferenceContext = conferenceContext;
    }
}
