/*
 * Copyright (C) 2010 Avaya, certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 */
package org.sipfoundry.openfire.plugin.presence;

import java.io.NotActiveException;
import java.util.Collection;
import java.util.HashSet;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import org.apache.log4j.Logger;
import org.jivesoftware.openfire.XMPPServer;
import org.jivesoftware.openfire.user.User;
import org.jivesoftware.openfire.user.UserManager;
import org.xmpp.packet.JID;

public class XmlRpcPresenceProvider extends XmlRpcProvider {

    private static String myHandle = "of:" + Math.abs(new Random().nextLong());

    public static final String SERVICE_PATH = "/plugins/sipx-openfire-presence/status";
    public static final String SERVICE_NAME = "status";
    public static final String SERVER = "presenceServer";

    private final Logger log = Logger.getLogger(XmlRpcPresenceProvider.class);

    public XmlRpcPresenceProvider() {

    }

    /*
     * It returns the xmpp username being given user's address
     */
    public static Map<String, Object> getXmppUsername(String emailAddress) {
        UserManager userManager = XMPPServer.getInstance().getUserManager();

        Set<String> searchFields = new HashSet<String>();
        searchFields.add(EMAIL_FIELD);
        Collection<User> users = userManager.findUsers(searchFields, emailAddress);

        if (users == null || users.size() == 0) {
            Map<String, Object> errorMap = createErrorMap(ErrorCode.SIP_ID_NOT_FOUND,
                    "No user matches given email address");
            return errorMap;
        }
        Map<String, Object> retval = createSuccessMap();
        User user = users.iterator().next();
        retval.put(SIP_ID, user.getUsername());

        return retval;
    }

    public Map<String, Object> getPresenceState(String xmppUsername) {
        try {
            String jid = appendDomain(xmppUsername);
            log.debug("GetPresenceState " + jid);
            assertPlugInReady();
            Map<String, Object> retval = createSuccessMap();
            UnifiedPresence unifiedPresence = PresenceUnifier.getInstance().getUnifiedPresence(xmppUsername);
            retval.put(XMPP_PRESENCE, unifiedPresence.getXmppPresence().toString());
            return retval;
        } catch (IllegalArgumentException e) {
            log.error("Invalid enum value ", e);
            return createErrorMap(ErrorCode.INVALID_XMPP_PRESENCE_VALUE, e.getMessage());
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        } catch (Exception e) {
            log.error("User Not Found", e);
            return createErrorMap(ErrorCode.USER_NOT_FOUND, e.getMessage());
        }
    }

    public Map<String, Object> setPresenceState(String xmppUsername, String xmppPresenceAsString) {
        try {
            String jid = appendDomain(xmppUsername);
            log.debug("setPresenceState" + jid + " XMPP presence " + xmppPresenceAsString);
            assertPlugInReady();
            Map<String, Object> retval = createSuccessMap();
            UnifiedPresence.XmppPresence xmppPresence = UnifiedPresence.XmppPresence.valueOf(xmppPresenceAsString);
            PresenceUnifier.getInstance().setUnifiedPresence(xmppUsername, xmppPresence);
            getPlugin().setPresenceState(jid, xmppPresence);
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        } catch (Exception ex) {
            log.error("User Not Found", ex);
            return createErrorMap(ErrorCode.USER_NOT_FOUND, ex.getMessage());
        }
    }

    public Map<String, Object> setPresenceStatus(String id, String status) {
        try {
            String jid = appendDomain(id);
            log.debug("setPresenceStatus " + jid + " status = " + status);
            assertPlugInReady();
            Map<String, Object> retval = createSuccessMap();
            getPlugin().setPresenceStatus(jid, status);
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        } catch (Exception ex) {
            log.error("User Not Found", ex);
            return createErrorMap(ErrorCode.USER_NOT_FOUND, ex.getMessage());
        }
    }

    public Map<String, Object> getPresenceStatus(String id) {
        try {
            String jid = appendDomain(id);
            log.debug("getPresenceStatus " + jid);
            assertPlugInReady();
            Map<String, Object> retval = createSuccessMap();
            String presenceStatus = getPlugin().getPresenceStatus(jid);
            if (presenceStatus != null) {
                retval.put(CUSTOM_PRESENCE_MESSAGE, presenceStatus);
            }
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        } catch (Exception ex) {
            log.error("UserNotFound ", ex);
            return createErrorMap(ErrorCode.USER_NOT_FOUND, ex.getMessage());
        }
    }

    /**
     * @param sipId SIP identity for which presence info is requested
     *
     * @return a Map containing the following information:
     *
     *         RESPONSE If response reports a failure, a map with the following three elements is
     *         returned: "status-code" // value == "ERROR" "faultCode" // code 1 means SIP
     *         identity not found "faultString" // plaintext explaination of failure
     *
     *         If response reports a success, a map with the following seven elements is returned:
     *         "status-code" // value == "OK "sip-resource-id" // queried SIP identity (user part
     *         only) "jabber-id" // associated jabber id "telephony-presence" // string
     *         representing telephony presence. Can be "IDLE", "BUSY" or "UNDERMINED"
     *         "xmpp-presence" // string representing XMPP presence. Can be "AVAILABLE",
     *         "OFFLINE", "EXTENDED_AWAY", "AWAY", "BUSY" and "CHAT" "unified-presence" // string
     *         representing unified XMPP presence which is a merge of the telephony and XMPP
     *         presences. Can be: // "not-available", "available-for-phone", "available-for-chat"
     *         and "available-for-both" "custom-presence-message" // string representing
     *         user-supplied cusomt message linked to presence state. Can be an empty string
     *
     */
    public Map<String, Object> getUnifiedPresenceInfo(String sipId) {
        try {
            log.debug("getUnifiedPresenceInfo " + sipId);
            assertPlugInReady();
            getPlugin();
            String xmppUser = SipXOpenfirePlugin.getXmppId(sipId);
            if (xmppUser == null) {
                Map<String, Object> errorMap = createErrorMap(ErrorCode.SIP_ID_NOT_FOUND,
                        "SIP ID Not found in database.");
                return errorMap;
            }
            log.debug("xmppUser = " + xmppUser);
            Map<String, Object> retval = createSuccessMap();
            JID jid = XMPPServer.getInstance().createJID(xmppUser, null);
            log.debug("xmppUser jid = " + jid);
            UnifiedPresence unifiedPresence = PresenceUnifier.getInstance().getUnifiedPresence(jid.getNode());

            retval.put(XMPP_PRESENCE, unifiedPresence.getXmppPresence().toString());
            retval.put(JABBER_ID, jid.toString());
            retval.put(SIP_ID, sipId);
            retval.put(UNIFIED_PRESENCE, unifiedPresence.getUnifiedPresence());
            retval.put(CUSTOM_PRESENCE_MESSAGE, unifiedPresence.getXmppStatusMessageWithSipState());
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        } catch (Exception ex) {
            log.error("Processing error", ex);
            return createErrorMap(ErrorCode.PROCESSING_ERROR, ex.getMessage());
        }

    }

    public Map<String, Object> registerPresenceMonitor(String protocol, String serverUrl) {
        try {
            log.debug("registerPresenceMonitor " + protocol + " " + serverUrl);
            assertPlugInReady();
            PresenceUnifier.getInstance().addUnifiedPresenceChangeListener(protocol, serverUrl);
            Map<String, Object> retval = createSuccessMap();
            retval.put(INSTANCE_HANDLE, myHandle);
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        }
    }

    public Map<String, Object> ping(String originatorName) {
        try {
            log.debug("ping received from " + originatorName);
            assertPlugInReady();
            Map<String, Object> retval = createSuccessMap();
            retval.put(INSTANCE_HANDLE, myHandle);
            return retval;
        } catch (NotActiveException e) {
            log.error("plug-in not active error: ", e);
            return createErrorMap(ErrorCode.NOT_READY, e.getMessage());
        }
    }

    private static void assertPlugInReady() throws NotActiveException {
        if (getPlugin().isInitialized() == false) {
            throw new NotActiveException("sipXopenfire not yet initialized");
        }
    }

}
