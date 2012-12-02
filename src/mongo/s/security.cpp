// security.cpp
/*
 *    Copyright (C) 2010 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// security.cpp

#include "pch.h"

#include "mongo/db/auth/authorization_manager.h"
#include "../db/security_common.h"
#include "../db/security.h"
#include "config.h"
#include "client_info.h"
#include "grid.h"

// this is the _mongos only_ implementation of security.h

namespace mongo {

    bool AuthenticationInfo::_warned;

    void AuthenticationInfo::startRequest() {
        _checkLocalHostSpecialAdmin();
    }

    void AuthenticationInfo::setIsALocalHostConnectionWithSpecialAuthPowers() {
        verify(!_isLocalHost);
        _isLocalHost = true;
        _isLocalHostAndLocalHostIsAuthorizedForAll = true;
        _checkLocalHostSpecialAdmin();
    }

    bool AuthenticationInfo::_isAuthorizedSpecialChecks( const string& dbname ) const {
        return isSpecialLocalhostAdmin();
    }

    bool AuthenticationInfo::isSpecialLocalhostAdmin() const {
        return _isLocalHostAndLocalHostIsAuthorizedForAll;
    }

    void AuthenticationInfo::_checkLocalHostSpecialAdmin() {
        if (noauth || !_isLocalHost || !_isLocalHostAndLocalHostIsAuthorizedForAll) {
            return;
        }

        string adminNs = "admin.system.users";

        DBConfigPtr config = grid.getDBConfig( adminNs );
        Shard s = config->getShard( adminNs );

        //
        // Note: The connection mechanism here is *not* ideal, and should not be used elsewhere.
        // It is safe in this particular case because the admin database is always on the config
        // server and does not move.
        //
        scoped_ptr<ScopedDbConnection> conn(
                ScopedDbConnection::getInternalScopedDbConnection(s.getConnString(), 30.0));

        BSONObj result = (*conn)->findOne("admin.system.users", Query());
        if( result.isEmpty() ) {
            if( ! _warned ) {
                // you could get a few of these in a race, but that's ok
                _warned = true;
                log() << "note: no users configured in admin.system.users, allowing localhost access" << endl;
            }

            // Must return conn to pool
            // TODO: Check for errors during findOne(), or just let the conn die?
            conn->done();
            _isLocalHostAndLocalHostIsAuthorizedForAll = true;
            return;
        }

        // Must return conn to pool
        conn->done();
        _isLocalHostAndLocalHostIsAuthorizedForAll = false;
    }

    bool CmdLogout::run(const string& dbname , BSONObj& cmdObj, int, string& errmsg, BSONObjBuilder& result, bool fromRepl) {
        AuthenticationInfo *ai = ClientInfo::get()->getAuthenticationInfo();
        ai->logout(dbname);
        return true;
    }
}
