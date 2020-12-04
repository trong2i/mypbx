/**
 *
 *
 * Copyright (c) 2012 eZuce, Inc. All rights reserved.
 * Contributed to SIPfoundry under a Contributor Agreement
 *
 * This software is free software; you can redistribute it and/or modify it under
 * the terms of the Affero General Public License (AGPL) as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 */
package org.sipfoundry.openfire.sqa;

import static org.apache.commons.lang.StringUtils.substringBefore;
import ietf.params.xml.ns.dialog_info.DialogInfo;

import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import javax.xml.bind.JAXBContext;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.StringUtils;
import org.jivesoftware.openfire.XMPPServer;
import org.sipfoundry.commons.userdb.User;
import org.sipfoundry.commons.userdb.ValidUsers;
import org.sipfoundry.commons.util.UnfortunateLackOfSpringSupportFactory;
import org.sipfoundry.sqaclient.SQAEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.xmpp.packet.JID;
import org.xmpp.packet.Presence;


public class SqaEventHandler implements Runnable {
    private final JAXBContext m_context;
    private final SQAEvent m_event;
    private final XMPPServer m_server = XMPPServer.getInstance();
    private static final Logger logger = LoggerFactory.getLogger(SqaEventHandler.class);
    ValidUsers m_users = UnfortunateLackOfSpringSupportFactory.getValidUsers();
    Map<String, SipPresenceBean> m_presenceCache;
    Map<String, List<String>> m_callMap;

    public SqaEventHandler(SQAEvent event, JAXBContext context, Map<String, SipPresenceBean> presenceCache, Map<String, List<String>> callMap) {
        m_event = event;
        m_context = context;
        m_presenceCache = presenceCache;
        m_callMap = callMap;
    }

    @Override
    public void run() {
        String data = m_event.getData();
        StringReader reader = new StringReader(data);
        logger.debug("Dialog Event Data: "+data);
        try {
            //Remove any unwanted trailing characters
            data = substringBefore(data, "</dialog-info>").concat("</dialog-info>\n");
            DialogInfo dialogInfo = (DialogInfo) m_context.createUnmarshaller().unmarshal(new StringReader(data));
            logger.debug("dialog-info: "+dialogInfo.toString());

            SipEventBean bean = new SipEventBean(dialogInfo);
            String observerId = bean.getObserverId();
            User observerUser = m_users.getUser(observerId);
            if (observerUser == null || !observerUser.isImEnabled()) {
                logger.debug("Observer user is null or is not im enabled - presence is not routed");
                return;
            }
            String sipId = bean.getCallerId();
            String targetSipId = bean.getCalleeId();
            if (StringUtils.equals(sipId, targetSipId)) {
                logger.debug("Do not handle events when sipId is EQUAL with targetSipId: " + sipId + " = " + targetSipId);
                return;
            }
            logger.debug("Target SIP Id (callee) "+targetSipId);
            User user = m_users.getUser(sipId);
            User targetUser = targetSipId != null ? m_users.getUser(targetSipId) : null;
            String targetJid = targetUser != null ? targetUser.getJid() : null;
            String userJid = user != null ? user.getJid() : null;
            User observerPartyUser = targetUser;
            String observerPartyId = targetSipId;

            if (!StringUtils.equals(sipId, observerId)) {
                observerPartyUser = user;
                observerPartyId = sipId;
            }
            boolean trying = bean.isTrying();
            boolean ringing = bean.isRinging();
            boolean confirmed = bean.isConfirmed();
            boolean terminated = bean.isTerminated();
            JID observerJID = m_server.createJID(observerUser.getJid(), null);
            if (observerJID == null) {
                logger.debug("Observer JID is null - presence is not routed");
                return;
            }
            logger.debug("Initiator: " + userJid);
            logger.debug("Recipient: " + targetJid);
            logger.debug("Observer status: trying: " + trying + " confirmed: " + confirmed + " terminated: " + terminated + " ringing: " + ringing);
            logger.debug("Observer JID (IM Entity that sent the message): " + observerJID.getNode());
            org.jivesoftware.openfire.user.User ofObserverUser = m_server.getUserManager().getUser(observerJID.getNode());
            Presence presence = m_server.getPresenceManager().getPresence(ofObserverUser);
            if (presence == null) {
                logger.debug("User is OFFLINE  -- cannot set presence state");
                return;
            }
            if (confirmed) {
                String confirmedDialogId = bean.getConfirmedDialogId();
                logger.debug("Received confirmed state for dialogId: " + confirmedDialogId);
                SipPresenceBean previousPresenceBean = m_presenceCache.get(observerJID.getNode());
                logger.debug("Previous confirmed presence bean with key: " + observerJID.getNode() + " is: " + previousPresenceBean);
                //if cache is not cleared, than this is a new incomming call, do not broadcast on the call status again
                if (previousPresenceBean == null) {
                    logger.debug("ObserverPartyUser " + observerPartyUser + " observer party id " + observerPartyId);
                    String presenceMessage = Utils.generateXmppStatusMessageWithSipState(observerUser, observerPartyUser, presence, observerPartyId);
                    //presence status is about to be changed - save current presence
                    if (presenceMessage != null) {
                        //save current presence and broadcast -on the phone- to roster
                        m_presenceCache.put(observerJID.getNode(), new SipPresenceBean(presence.getStatus(), observerPartyId));
                        presence.setStatus(presenceMessage);
                        addDialogCall(confirmedDialogId);
                        logger.debug("Broadcast 'On The Phone' presence for confirmed dialogId: " + confirmedDialogId);
                        ofObserverUser.getRoster().broadcastPresence(presence);
                    }
                }
            } else if (terminated) {
                String terminatedDialogId = bean.getTerminatedDialogId();
                logger.debug("Received terminated state for dialogId: " + terminatedDialogId);
                SipPresenceBean previousPresenceBean = m_presenceCache.get(observerJID.getNode());
                logger.debug("Previous terminated presence bean with key: " + observerJID.getNode() + " is: " + previousPresenceBean);
                if(previousPresenceBean != null && removeDialogCall(terminatedDialogId)) {
                    //if on the phone and call terminated, broadcast previous presence, clear cache
                    presence.setStatus(previousPresenceBean.getStatusMessage());
                    logger.debug("Broadcast REMOVE 'On The Phone' presence for terminated dialogId: " + terminatedDialogId);
                    ofObserverUser.getRoster().broadcastPresence(presence);
                    m_presenceCache.remove(observerJID.getNode());
                }
            }
        } catch (Exception e) {
            logger.error("Cannot parse packet: data ", e);
        } finally {
            IOUtils.closeQuietly(reader);
        }
    }

    private void addDialogCall(String dialogId) {
        List<String> queue = m_callMap.get(dialogId);
        logger.debug("Add in queue for dialogId: " + dialogId + " queue: " +queue);
        if (queue == null) {
            queue = new ArrayList<String>();
            m_callMap.put(dialogId, queue);
        }
        queue.add("confirmed");
    }

    // returns true if there is no confirmed state present in the queue
    private boolean removeDialogCall(String dialogId) {
        List<String> queue = m_callMap.get(dialogId);
        logger.debug("Remove from queue for dialogId: " + dialogId + " queue: " + queue);
        //pair is true if the terminated state has a previous confirmed state pair for the same dialog id
        boolean pair = false;
        if (queue != null && !queue.isEmpty()) {
            queue.remove(queue.size() - 1);
            pair = true;
        }
        if (queue == null || queue.isEmpty()) {
            m_callMap.remove(dialogId);
            return pair;
        }
        return false;
    }
}

