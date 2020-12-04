/*
 * 
 * 
 * Copyright (C) 2009 Pingtel Corp., certain elements licensed under a Contributor Agreement.  
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 * 
 */

package org.sipfoundry.sipxivr.email;


/**
 * A formatter for Medium length e-mail messages 
 */
public class EmailFormatterImap extends EmailFormatter {
    
    /**
     * The Subject part of the e-mail
     * @return
     */
    @Override
    public String getSubject() {
        return fmt("SubjectFull");
    }


    /**
     * The HTML body part of the e-mail
     * @return
     */
    @Override
    public String getHtmlBody() {
        return null;
    }

    /**
     * The text body part of the e-mail
     * @return
     */
    @Override
    public String getTextBody() {
        return null;
    }
}



