/**
 *
 *
 * Copyright (c) 2014 eZuce, Inc. All rights reserved.
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
package org.sipfoundry.openfire.provider;

import java.util.HashMap;
import java.util.Map;

import org.jivesoftware.openfire.provider.UserPropertiesProvider;

import com.mongodb.BasicDBObject;
import com.mongodb.DBCollection;
import com.mongodb.DBObject;

public class MongoUserPropertiesProvider extends BaseMongoProvider implements UserPropertiesProvider {
    private static final String COLLECTION_NAME = "ofUserProp";

    public MongoUserPropertiesProvider() {
        setDefaultCollectionName(COLLECTION_NAME);
        DBCollection usrPropsCollection = getDefaultCollection();
        DBObject index = new BasicDBObject();

        index.put("username", 1);
        index.put("name", 1);
        usrPropsCollection.ensureIndex(index);
    }

    @Override
    public Map<String, String> loadProperties(String username) {
        Map<String, String> props = new HashMap<String, String>();
        DBCollection usrPropsCollection = getDefaultCollection();
        DBObject query = new BasicDBObject();

        query.put("username", username);

        for (DBObject usrPropObj : usrPropsCollection.find(query)) {
            String propName = (String) usrPropObj.get("name");
            String propValue = (String) usrPropObj.get("propValue");

            props.put(propName, propValue);
        }

        return props;
    }

    @Override
    public void insertProperty(String username, String propName, String propValue) {
        // nothing to do
    }

    @Override
    public String getPropertyValue(String username, String propName) {
        DBCollection usrPropsCollection = getDefaultCollection();
        DBObject usrPropObj = getPropObject(usrPropsCollection, username, propName);
        String propValue = null;

        if (usrPropObj != null) {
            propValue = (String) usrPropObj.get("propValue");
        }

        return propValue;
    }

    @Override
    public void updateProperty(String username, String propName, String propValue) {
        // nothing to do
    }

    @Override
    public boolean deleteUserProperties(String username) {
        // nothing to do

        return true;
    }

    @Override
    public void deleteProperty(String username, String propName) {
        // nothing to do
    }

    private static DBObject getPropObject(DBCollection usrPropsCollection, String username, String propName) {
        DBObject query = new BasicDBObject();

        query.put("username", username);
        query.put("name", propName);

        DBObject usrPropObj = usrPropsCollection.findOne(query);
        return usrPropObj;
    }
}
