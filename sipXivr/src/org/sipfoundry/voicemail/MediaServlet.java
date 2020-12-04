/**
 *
 *
 * Copyright (c) 2011 eZuce, Inc. All rights reserved.
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
package org.sipfoundry.voicemail;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.StringUtils;
import org.apache.log4j.Logger;
import org.sipfoundry.commons.ivr.MimeType;
import org.sipfoundry.commons.userdb.User;
import org.sipfoundry.commons.userdb.ValidUsers;
import org.sipfoundry.sipxivr.SipxIvrConfiguration;
import org.sipfoundry.sipxivr.rest.SipxIvrServletHandler;
import org.sipfoundry.voicemail.mailbox.MailboxManager;
import org.sipfoundry.voicemail.mailbox.VmMessage;

public class MediaServlet extends HttpServlet {
    private static final long serialVersionUID = 1L;
    private static final String METHOD_GET = "GET";
    static final Logger LOG = Logger.getLogger("org.sipfoundry.sipxivr");

    @Override
    public void doPut(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException {
        doIt(request, response);
    }

    @Override
    public void doGet(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException {
        doIt(request, response);
    }

    @Override
    public void doDelete(HttpServletRequest request, HttpServletResponse response) throws ServletException,
            IOException {
        doIt(request, response);
    }

    public void doIt(HttpServletRequest request, HttpServletResponse response) throws ServletException, IOException {
        ValidUsers validUsers = (ValidUsers) request.getAttribute(SipxIvrServletHandler.VALID_USERS_ATTR);
        MailboxManager mailboxManager = (MailboxManager) request.getAttribute(SipxIvrServletHandler.MAILBOX_MANAGER);
        SipxIvrConfiguration ivrConfig = (SipxIvrConfiguration) request
                .getAttribute(SipxIvrServletHandler.IVR_CONFIG_ATTR);
        String method = request.getMethod().toUpperCase();

        String pathInfo = request.getPathInfo();
        String[] subDirs = pathInfo.split("/");
        if (subDirs.length < 3) {
            response.sendError(404); // path not found
            return;
        }

        // The first element is empty (the leading slash)
        // The second element is the mailbox
        String mailboxString = subDirs[1];
        // The third element is the "context" (either mwi, message)
        String dir = subDirs[2];
        String messageId = subDirs[3];

        User user = validUsers.getUser(mailboxString);
        // only superadmin and mailbox owner can access this service
        // TODO allow all admin user to access it
        if (user != null && ServletUtil.isForbidden(request, user.getUserName(), request.getLocalPort(), ivrConfig.getHttpPort())) {
            response.sendError(403); // Send 403 Forbidden
            return;
        }

        if (user != null) {
            if (method.equals(METHOD_GET)) {
                VmMessage message = mailboxManager.getVmMessage(user.getUserName(), messageId, true);
                File audioFile = message.getAudioFile();
                response.setHeader("Expires", "0");
                response.setHeader("Cache-Control", "must-revalidate, post-check=0, pre-check=0");
                response.setHeader("Pragma", "public");
                response.setHeader("Content-Disposition", "attachment; filename=\"" + audioFile.getName() + "\"");
                String mimeType = MimeType.getMimeByFormat(message.getDescriptor().getAudioFormat());
                if (StringUtils.isNotBlank(mimeType)) {
                    response.setHeader("Content-type", MimeType.getMimeByFormat(message.getDescriptor().getAudioFormat()));
                }

                OutputStream responseOutputStream = null;
                InputStream stream = null;
                try {
                    responseOutputStream = response.getOutputStream();
                    stream = new FileInputStream(audioFile);
                    IOUtils.copy(stream, responseOutputStream);
                } finally {
                    IOUtils.closeQuietly(stream);
                    IOUtils.closeQuietly(responseOutputStream);
                    message.cleanup();
                }
            } else {
                response.sendError(405);
            }
        }

    }
}
