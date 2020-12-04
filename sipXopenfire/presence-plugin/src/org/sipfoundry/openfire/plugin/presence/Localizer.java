/*
 * Copyright (C) 2010 Avaya, certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 */
package org.sipfoundry.openfire.plugin.presence;

import java.util.Locale;
import java.util.MissingResourceException;
import java.util.ResourceBundle;

import org.apache.log4j.Logger;

public class Localizer {
    private static Logger log = Logger.getLogger(Localizer.class);
    private ResourceBundle m_bundle;

    Localizer(String localeString, ClassLoader classloader) {

        Locale locale;

        if (localeString == null) {
            // just in case .. shouldn't happen though
            locale = Locale.ENGLISH;
        } else {
            // convert any dashes to underscores, as sometimes locales are mis-represented
            String ls = localeString.replace('-', '_');
            String[] localeElements = ls.split("_");
            String lang = "";
            String country = "";
            String variant = "";
            if (localeElements.length >= 3) {
                variant = localeElements[2];
            }
            if (localeElements.length >= 2) {
                country = localeElements[1];
            }
            if (localeElements.length >= 1) {
                lang = localeElements[0];
            }
            locale = new Locale(lang, country, variant);
        }

        try {
            m_bundle = ResourceBundle.getBundle("sipxopenfire-prompts", locale, classloader);
        } catch (MissingResourceException e) {
            log.error("can't find sipxopenfire-prompts properties file: " + e.getMessage());
        }
    }

    public synchronized String localize(String promptToLocalize) {

        return m_bundle == null ? "Cannot find prompts file" : m_bundle.getString(promptToLocalize);
    }

}
