/*
 *
 *
 * Copyright (C) 2008-2009 Pingtel Corp., certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 *
 */
package org.sipfoundry.sipximbot;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Properties;

import org.apache.log4j.Logger;
import org.apache.log4j.PropertyConfigurator;
import org.sipfoundry.commons.freeswitch.FreeSwitchConfigurationInterface;
import org.sipfoundry.commons.log4j.SipFoundryLayout;
import org.sipfoundry.commons.util.DomainConfiguration;

/**
 * Holds the configuration data needed for sipXimbot.
 *
 */
public class ImbotConfiguration implements FreeSwitchConfigurationInterface {

	private static final Logger LOG = Logger.getLogger("org.sipfoundry.sipximbot");
    private String m_logFile; // The file to log into
    private String m_docDirectory; // File path to DOC Directory (usually /usr/share/www/doc)
    private String m_configDirectory;
    private String m_sipxchangeDomainName; // The domain name of this system
    private String m_realm;
    private String m_3pccSecureUrl; // The url of the third party call controller via HTTPS
    private String m_configUrl;
    private String m_cdrSecureURL;
    private String m_openfireHost; // The host name where the Openfire service runs
    private int    m_openfireXmlRpcPort; // The port number to use for XML-RPC Openfire requests
    private String m_myAsstAcct;
    private String m_myAsstPswd;
    private String m_voicemailRootUrl;

    private static ImbotConfiguration s_current;
    private static File s_propertiesFile;
    private static long s_lastModified;

    private static String s_sharedSecret = null;

    private ImbotConfiguration() {
    }

    public static ImbotConfiguration get() {
        return update(true);
    }

    public static ImbotConfiguration getTest() {
        return update(false);
    }

    /**
     * Load new Configuration object if the underlying properties files have changed since the last
     * time
     *
     * @return
     */
    private static ImbotConfiguration update(boolean load) {
        if (s_current == null || s_propertiesFile != null && s_propertiesFile.lastModified() != s_lastModified) {
            s_current = new ImbotConfiguration();
            if (load) {
                s_current.properties();
            }
        }
        return s_current;
    }

    void properties() {
        // Configure log4j
        String m_configDirectory = System.getProperty("conf.dir");
        PropertyConfigurator.configureAndWatch(m_configDirectory+"/sipximbot/log4j.properties",
                SipFoundryLayout.LOG4J_MONITOR_FILE_DELAY);

        if (m_configDirectory == null) {
            System.err.println("Cannot get System Property conf.dir!  Check jvm argument -Dconf.dir=") ;
            System.exit(1);
        }

        // Setup SSL properties so we can talk to HTTPS servers
        String keyStore = System.getProperty("javax.net.ssl.keyStore");
        if (keyStore == null) {
            // Take an educated guess as to where it should be
            keyStore = m_configDirectory+"/ssl/ssl.keystore";
            System.setProperty("javax.net.ssl.keyStore", keyStore);
            System.setProperty("javax.net.ssl.keyStorePassword", "changeit"); // Real security!
 //           System.setProperty("java.protocol.handler.pkgs", "com.sun.net.ssl.internal.www.protocol");
 //           System.setProperty("javax.net.debug", "ssl");
        }
        String trustStore = System.getProperty("javax.net.ssl.trustStore");
        if (trustStore == null) {
            // Take an educated guess as to where it should be
            trustStore = m_configDirectory+"/ssl/authorities.jks";
            System.setProperty("javax.net.ssl.trustStore", trustStore);
            System.setProperty("javax.net.ssl.trustStoreType", "JKS");
            System.setProperty("javax.net.ssl.trustStorePassword", "changeit"); // Real security!
        }

        String name = "sipximbot.properties";
        FileInputStream inStream;
        Properties props = null;
        try {
        	s_propertiesFile = new File(m_configDirectory + "/" + name);
            s_lastModified = s_propertiesFile.lastModified();
            inStream = new FileInputStream(s_propertiesFile);
            props = new Properties();
            props.load(inStream);
            inStream.close();
        } catch (FileNotFoundException e) {
            e.printStackTrace(System.err);
            System.exit(1);
        } catch (IOException e) {
            e.printStackTrace(System.err);
            System.exit(1);
        }

        String prop = null;
        try {
            m_logFile = props.getProperty(prop = "log.file");

            m_docDirectory = props.getProperty(prop = "imbot.docDirectory") ;
            m_sipxchangeDomainName = props.getProperty(prop = "imbot.sipxchangeDomainName");
            m_realm = props.getProperty(prop ="imbot.realm");

            m_3pccSecureUrl = props.getProperty(prop = "imbot.3pccSecureUrl") + "/callcontroller/";
            m_cdrSecureURL  = props.getProperty(prop = "imbot.callHistoryUrl");

            m_openfireHost = props.getProperty(prop = "imbot.openfireHost");
            m_openfireXmlRpcPort = Integer.parseInt(props.getProperty(prop = "imbot.openfireXmlRpcPort"));

            m_myAsstAcct =  props.getProperty(prop = "imbot.paUserName");
            m_myAsstPswd =  props.getProperty(prop = "imbot.paPassword");
            m_configUrl = props.getProperty(prop = "imbot.configUrl");
            m_voicemailRootUrl = props.getProperty("imbot.voicemailRootUrl");

        } catch (Exception e) {
            System.err.println("Problem understanding property " + prop);
            e.printStackTrace(System.err);
            System.exit(1);
        }
    }

    public static String getSharedSecret() {
        if(s_sharedSecret == null) {
            DomainConfiguration config = new DomainConfiguration(System.getProperty("conf.dir") + "/domain-config");
            s_sharedSecret = config.getSharedSecret();
        }
        return s_sharedSecret;
    }


    @Override
    public String getLogLevel() {
        return Logger.getRootLogger().getLevel().toString();
    }

    @Override
    public String getLogFile() {
        return m_logFile;
    }

    @Override
    public String getDocDirectory() {
        return m_docDirectory;
    }

    @Override
    public String getSipxchangeDomainName() {
        return m_sipxchangeDomainName;
    }

    @Override
    public String getRealm() {
        return m_realm;
    }

    @Override
    public void setRealm(String realm) {
        m_realm = realm;
    }

    public String get3pccSecureUrl() {
        return m_3pccSecureUrl;
    }

    public String getcdrSecureUrl() {
        return m_cdrSecureURL;
    }

    public void set3pccSecureUrl(String url) {
        m_3pccSecureUrl = url;
    }

    public String getOpenfireHost() {
        return m_openfireHost;
    }

    public String getMyAsstAcct() {
        return m_myAsstAcct;
    }

    public String getMyAsstPswd() {
        return m_myAsstPswd;
    }

    public String getConfigUrl() {
        return m_configUrl;
    }


    public void setOpenfireHost(String host) {
        m_openfireHost = host;
    }

    public int getOpenfireXmlRpcPort() {
        return m_openfireXmlRpcPort;
    }

    public void setOpenfireXmlRpcPort(int port) {
        m_openfireXmlRpcPort = port;
    }

    public String getVoicemailRootUrl() {
        return m_voicemailRootUrl;
    }

	@Override
	public Logger getLogger() {
		return LOG;
	}

    @Override
    public int getEventSocketPort() {
        return -1;
    }

    public String getConfigDirectory() {
        return m_configDirectory;
    }
}
