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
package org.sipfoundry.sipxconfig.recording;

import static org.sipfoundry.sipxconfig.recording.Recording.FEATURE;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.Writer;
import java.util.List;
import java.util.Set;

import org.apache.commons.io.IOUtils;
import org.sipfoundry.sipxconfig.address.Address;
import org.sipfoundry.sipxconfig.cfgmgt.ConfigManager;
import org.sipfoundry.sipxconfig.cfgmgt.ConfigProvider;
import org.sipfoundry.sipxconfig.cfgmgt.ConfigRequest;
import org.sipfoundry.sipxconfig.cfgmgt.ConfigUtils;
import org.sipfoundry.sipxconfig.cfgmgt.KeyValueConfiguration;
import org.sipfoundry.sipxconfig.commserver.Location;
import org.sipfoundry.sipxconfig.ivr.Ivr;
import org.springframework.beans.factory.annotation.Required;

public class RecordingConfig implements ConfigProvider {
    private Recording m_recording;
    private Ivr m_ivr;

    @Override
    public void replicate(ConfigManager manager, ConfigRequest request) throws IOException {
        if (!request.applies(FEATURE, Ivr.FEATURE)) {
            return;
        }

        Set<Location> locations = request.locations(manager);
        if (locations.isEmpty()) {
            return;
        }
        List<Address> ivrAddresses = manager.getAddressManager().getAddresses(Ivr.REST_API);
        for (Location location : locations) {
            boolean enabled = manager.getFeatureManager().isFeatureEnabled(FEATURE, location);
            File dir = manager.getLocationDataDirectory(location);
            ConfigUtils.enableCfengineClass(dir, "sipxrecording.cfdat", enabled, "sipxrecording");
            if (!enabled) {
                continue;
            }
            File f = new File(dir, "sipxrecording.properties.part");
            Writer wtr = new FileWriter(f);
            try {
                write(wtr, m_recording.getSettings(), ivrAddresses, m_ivr.getAudioFormat());
            } finally {
                IOUtils.closeQuietly(wtr);
            }
        }
    }

    void write(Writer wtr, RecordingSettings settings, List<Address> ivrAddresses, String audioFormat) throws IOException {
        KeyValueConfiguration config = KeyValueConfiguration.equalsSeparated(wtr);
        config.writeSettings(settings.getSettings());
        StringBuilder ivrAddressesStr = new StringBuilder();
        for (Address address : ivrAddresses) {
            ivrAddressesStr.
                append(address).
                append(" ");
        }
        if (ivrAddressesStr.length() > 0) {
            config.write("config.ivrNodes", ivrAddressesStr.toString());
        }
        config.write("audio.format", audioFormat);
    }

    @Required
    public void setRecording(Recording recording) {
        m_recording = recording;
    }

    @Required
    public void setIvr(Ivr ivr) {
        m_ivr = ivr;
    }
}
