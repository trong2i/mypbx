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

public class SipPresenceBean {
    private String m_statusMessage;
    private String m_callingPartiId;

    public SipPresenceBean(String statusMessage, String callingPartiId) {
        m_statusMessage = statusMessage;
        m_callingPartiId = callingPartiId;
    }

    public String getStatusMessage() {
        return m_statusMessage;
    }

    public String getCallingPartiId() {
        return m_callingPartiId;
    }
}
