/*
 * Copyright (C) 2010 Avaya, certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 */
package org.sipfoundry.siptester;

import org.apache.commons.digester.Digester;
import org.xml.sax.InputSource;

public class TesterConfigParser {

    public static String TESTER_CONFIG = "tester-config";

    /**
     * Add the digester rules.
     * 
     * @param digester
     */
    private static void addRules(Digester digester) {
        digester.addObjectCreate(TESTER_CONFIG, SipTesterConfig.class);
        digester.addCallMethod(String.format("%s/%s", TESTER_CONFIG, "tester-ip-address"),
                "setTesterIpAddress",0);
        digester.addCallMethod(String.format("%s/%s", TESTER_CONFIG, "log-level"),
        "setLogLevel",0);
        digester.addCallMethod(String.format("%s/%s", TESTER_CONFIG, "tester-base-port"),
                "setBasePort",0);
        digester.addCallMethod(String.format("%s/%s", TESTER_CONFIG, "tester-rtp-base-port"),
                "setRtpBasePort",0);
       
    }

    public SipTesterConfig parse(String url) {
        Digester digester = new Digester();
        addRules(digester);
        InputSource inputSource = new InputSource(url);
        try {
            digester.parse(inputSource);
        } catch (Exception e) {
            throw new SipTesterException(e);
        }
        return (SipTesterConfig) digester.getRoot();
    }

}
