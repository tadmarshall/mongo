// parameters.cpp

/**
*    Copyright (C) 2012 10gen Inc.
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

#include "mongo/pch.h"
#include "mongo/db/commands.h"
#include "mongo/db/cmdline.h"
#include "mongo/client/dbclient_rs.h"
#include "mongo/db/server_parameters.h"

namespace mongo {

    const char* fetchReplIndexPrefetchParam();

    class CmdGet : public Command {
    public:
        CmdGet() : Command( "getParameter" ) { }
        virtual bool slaveOk() const { return true; }
        virtual bool adminOnly() const { return true; }
        virtual LockType locktype() const { return NONE; }
        virtual void addRequiredPrivileges(const std::string& dbname,
                                           const BSONObj& cmdObj,
                                           std::vector<Privilege>* out) {
            ActionSet actions;
            actions.addAction(ActionType::getParameter);
            out->push_back(Privilege(AuthorizationManager::SERVER_RESOURCE_NAME, actions));
        }
        virtual void help( stringstream &help ) const {
            help << "get administrative option(s)\nexample:\n";
            help << "{ getParameter:1, notablescan:1 }\n";
            help << "supported so far:\n";
            help << "  quiet\n";
            help << "  notablescan\n";
            help << "  logLevel\n";
            help << "  syncdelay\n";
            help << "{ getParameter:'*' } to get everything\n";
        }
        bool run(const string& dbname, BSONObj& cmdObj, int, string& errmsg, BSONObjBuilder& result, bool fromRepl ) {
            bool all = *cmdObj.firstElement().valuestrsafe() == '*';

            int before = result.len();

            if( all || cmdObj.hasElement("quiet") ) {
                result.append("quiet", cmdLine.quiet );
            }
            if( all || cmdObj.hasElement("notablescan") ) {
                result.append("notablescan", cmdLine.noTableScan);
            }
            if( all || cmdObj.hasElement("logLevel") ) {
                result.append("logLevel", logLevel);
            }
            if( all || cmdObj.hasElement("syncdelay") ) {
                result.append("syncdelay", cmdLine.syncdelay);
            }
            if (all || cmdObj.hasElement("replIndexPrefetch")) {
                result.append("replIndexPrefetch", fetchReplIndexPrefetchParam());
            }

            const ServerParameter::Map& m = ServerParameterSet::getGlobal()->getMap();
            for ( ServerParameter::Map::const_iterator i = m.begin(); i != m.end(); ++i ) {
                if ( all || cmdObj.hasElement( i->first.c_str() ) ) {
                    i->second->append( result );
                }
            }

            if ( before == result.len() ) {
                errmsg = "no option found to get";
                return false;
            }
            return true;
        }
    } cmdGet;

    // tempish
    bool setParmsMongodSpecific(const string& dbname, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool fromRepl );

    class CmdSet : public Command {
    public:
        CmdSet() : Command( "setParameter" ) { }
        virtual bool slaveOk() const { return true; }
        virtual bool adminOnly() const { return true; }
        virtual LockType locktype() const { return NONE; }
        virtual void addRequiredPrivileges(const std::string& dbname,
                                           const BSONObj& cmdObj,
                                           std::vector<Privilege>* out) {
            ActionSet actions;
            actions.addAction(ActionType::setParameter);
            out->push_back(Privilege(AuthorizationManager::SERVER_RESOURCE_NAME, actions));
        }
        virtual void help( stringstream &help ) const {
            help << "set administrative option(s)\n";
            help << "{ setParameter:1, <param>:<value> }\n";
            help << "supported so far:\n";
            help << "  journalCommitInterval\n";
            help << "  logLevel\n";
            help << "  notablescan\n";
            help << "  quiet\n";
            help << "  syncdelay\n";
        }
        bool run(const string& dbname, BSONObj& cmdObj, int, string& errmsg, BSONObjBuilder& result, bool fromRepl ) {
            int s = 0;
            bool found = setParmsMongodSpecific(dbname, cmdObj, errmsg, result, fromRepl);
            if( cmdObj.hasElement("journalCommitInterval") ) {
                if( !cmdLine.dur ) {
                    errmsg = "journaling is off";
                    return false;
                }
                int x = (int) cmdObj["journalCommitInterval"].Number();
                verify( x > 1 && x < 500 );
                cmdLine.journalCommitInterval = x;
                log() << "setParameter journalCommitInterval=" << x << endl;
                s++;
            }
            if( cmdObj.hasElement("notablescan") ) {
                verify( !cmdLine.isMongos() );
                if( s == 0 )
                    result.append("was", cmdLine.noTableScan);
                cmdLine.noTableScan = cmdObj["notablescan"].Bool();
                s++;
            }
            if( cmdObj.hasElement("quiet") ) {
                if( s == 0 )
                    result.append("was", cmdLine.quiet );
                cmdLine.quiet = cmdObj["quiet"].Bool();
                s++;
            }
            if( cmdObj.hasElement("syncdelay") ) {
                verify( !cmdLine.isMongos() );
                if( s == 0 )
                    result.append("was", cmdLine.syncdelay );
                cmdLine.syncdelay = cmdObj["syncdelay"].Number();
                s++;
            }
            if( cmdObj.hasElement( "logLevel" ) ) {
                if( s == 0 )
                    result.append("was", logLevel );
                logLevel = cmdObj["logLevel"].numberInt();
                s++;
            }
            if( cmdObj.hasElement( "traceExceptions" ) ) {
                if( s == 0 ) result.append( "was", DBException::traceExceptions );
                DBException::traceExceptions = cmdObj["traceExceptions"].Bool();
                s++;
            }
            if( cmdObj.hasElement( "replMonitorMaxFailedChecks" ) ) {
                if( s == 0 ) result.append( "was", ReplicaSetMonitor::getMaxFailedChecks() );
                ReplicaSetMonitor::setMaxFailedChecks(
                        cmdObj["replMonitorMaxFailedChecks"].numberInt() );
                s++;
            }

            const ServerParameter::Map& m = ServerParameterSet::getGlobal()->getMap();
            BSONObjIterator i( cmdObj );
            i.next(); // skip past command name
            while ( i.more() ) {
                BSONElement e = i.next();
                ServerParameter::Map::const_iterator j = m.find( e.fieldName() );
                if ( j == m.end() )
                    continue;

                if ( ! j->second->allowedToChangeAtRuntime() ) {
                    errmsg = str::stream()
                        << "not allowed to change ["
                        << e.fieldName()
                        << "] at runtime";
                    return false;
                }

                Status status = j->second->set( e );
                if ( status.isOK() ) {
                    s++;
                    continue;
                }
                errmsg = status.reason();
                result.append( "code", status.code() );
                return false;
            }

            if( s == 0 && !found ) {
                errmsg = "no option found to set, use help:true to see options ";
                return false;
            }

            return true;
        }
    } cmdSet;

}
