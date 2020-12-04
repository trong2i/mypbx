/*
 *  Copyright (C) 2010 Avaya, certain elements licensed under a Contributor Agreement.
 *  Contributors retain copyright to elements licensed under a Contributor Agreement.
 *  Licensed to the User under the LGPL license.
 *
 */
package org.sipfoundry.sipxbridge;

import gov.nist.javax.sip.ClientTransactionExt;
import gov.nist.javax.sip.TlsSecurityPolicy;
import gov.nist.javax.sip.stack.SIPTransaction;

import java.text.ParseException;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import javax.net.ssl.SSLPeerUnverifiedException;
import javax.sip.address.Hop;
import javax.sip.address.SipURI;
import javax.sip.header.RouteHeader;

import org.apache.log4j.Logger;
import org.apache.xmlrpc.XmlRpcException;
import org.sipfoundry.commons.siprouter.FindSipServer;

public class SipxTlsSecurityPolicy implements TlsSecurityPolicy {

    private static final Logger logger = Logger.getLogger(SipxTlsSecurityPolicy.class);

    /**
     * Enforce any application-specific security policy for TLS clients.
     * Called when establishing an outgoing TLS connection.
     * @param transaction -- the transaction context for the connection
     * @throws SecurityException -- if the connection should be rejected
     */
    @Override
    public void enforceTlsPolicy(ClientTransactionExt transaction) throws SecurityException
    {
       // accept only certificates that match the intended destination
        if ( logger.isDebugEnabled() ) logger.debug("SipxTlsSecurityPolicy::enforceTlsPolicy");
        List<String> certIdentities = null;
        try {
            certIdentities = ((SIPTransaction)transaction).extractCertIdentities();
        } catch (SSLPeerUnverifiedException e1) {
            logger.warn("TLS peer unverified");
            e1.printStackTrace();
        }
        if ((certIdentities == null) || certIdentities.isEmpty()) {
            logger.warn("Could not find any identities in the TLS certificate");
        }
        else {
            if ( logger.isDebugEnabled() ) logger.debug("found identities: " + certIdentities);
            // Policy enforcement: now use the set of SIP domain identities gathered from
            // the certificate to make authorization decisions.
            // Validate that one of the identities in the certificate matches the request domain.

            // All calls will have a Route header containing the IP address (resolved address)
            // so we will do a lookup on all identities, and then verify that the Route header
            // matches one of the resulting addresses.
            RouteHeader route = (RouteHeader)transaction.getRequest().getHeader(RouteHeader.NAME);
            String peerDomain;
            String peerIpAddress = "";

            // SIP Domain from the request URI
            peerDomain = ((SipURI) transaction.getRequest().getRequestURI()).getHost();
            String expectedIpAddress = ((SipURI) route.getAddress().getURI()).getHost();
            if ( logger.isDebugEnabled() ) logger.debug("SIP domain from reqUri is " + peerDomain +
                ", expected IP from route is " + expectedIpAddress);

            Boolean foundPeerIdentity = false;
            OUTERMOST: for (String identity : certIdentities) {
                try {
                    Collection<Hop> hops = new FindSipServer(logger).findSipServers(
                            ProtocolObjects.addressFactory.createSipURI("", identity));
                    Hop hop;
                    for (Iterator<Hop> it = hops.iterator(); it.hasNext();) {
                        hop = it.next();
                        peerIpAddress = (hop == null ? "" : hop.getHost());
                        logger.info("looking up certificate identity " + identity + ": found " + peerIpAddress);
                        if (expectedIpAddress.equals(peerIpAddress)) {
                            foundPeerIdentity = true;
                            break OUTERMOST;
                        }
                    }

                } catch (ParseException e) {
                    logger.warn("could not create URI from " + identity);
                }

            }
            if (!foundPeerIdentity) {
                logger.error("TLS certificate for connection to " + peerDomain + "(" + expectedIpAddress + ") " +
                        " does not match: " + certIdentities);
                StringBuilder err = new StringBuilder(peerDomain);
                for (String identity : certIdentities) {
                    err.append(' ').append(identity);
                }
                Gateway.raiseAlarm(Gateway.TLS_CERTIFICATE_MISMATCH_ALARM_ID, err);
                throw new SecurityException("Certificate identity does not match requested domain");
            }
        }
    }

}
