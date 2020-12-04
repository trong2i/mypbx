/*
 *
 *
 * Copyright (C) 2007 Pingtel Corp., certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 *
 *
 */
package org.sipfoundry.sipxconfig.security;

import java.util.ArrayList;
import java.util.Collection;
import org.sipfoundry.sipxconfig.commserver.Location;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.authority.GrantedAuthorityImpl;
import org.springframework.security.core.userdetails.UserDetails;

public class LocationDetailsImpl implements UserDetails {
    private static final GrantedAuthority AUTH_LOCATION = new GrantedAuthorityImpl(Location.ROLE_LOCATION);

    private final String m_hostFqdn;
    private final String m_password;
    private final Collection<GrantedAuthority> m_authorities;

    /**
     * LocationDetails constructor
     *
     * Create an Spring Security LocationDetails object based on the Location, the userNameOrAlias
     * that is the location fqdn, and the authorities granted to this location.
     */
    public LocationDetailsImpl(Location location) {
        m_hostFqdn = location.getFqdn();
        m_password = location.getPassword();
        m_authorities = new ArrayList<GrantedAuthority>();
        m_authorities.add(AUTH_LOCATION);
    }

    public boolean isAccountNonExpired() {
        return true; // accounts don't expire
    }

    public boolean isAccountNonLocked() {
        return true; // accounts are never locked
    }

    public Collection<GrantedAuthority> getAuthorities() {
        return m_authorities;
    }

    public boolean isCredentialsNonExpired() {
        return true; // credentials don't expire
    }

    public boolean isEnabled() {
        return true; // accounts are always enabled
    }

    /** Return the password */
    public String getPassword() {
        return m_password;
    }

    /**
     * Returns the host fully qualified domain name.
     */
    public String getUsername() {
        return m_hostFqdn;
    }
}
