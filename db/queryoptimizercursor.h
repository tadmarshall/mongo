// @file queryoptimizercursor.h

/**
 *    Copyright (C) 2011 10gen Inc.
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

namespace mongo {
    
    /** Helper class for caching and counting matches during execution of a QueryPlan. */
    class CachedMatchCounter {
    public:
        CachedMatchCounter( long long &aggregateNscanned, int cumulativeCount ) : _aggregateNscanned( aggregateNscanned ), _nscanned(), _cumulativeCount( cumulativeCount ), _count(), _checkDups(), _match( Unknown ) {}
        void setCheckDups( bool checkDups ) { _checkDups = checkDups; }
        void resetMatch() { _match = Unknown; }
        void setMatch( bool match ) { _match = match ? True : False; }
        bool knowMatch() const { return _match != Unknown; }
        void countMatch( const DiskLoc &loc ) {
            if ( _match == True && !getsetdup( loc ) ) {
                ++_cumulativeCount;
                ++_count;
            }
        }
        bool enoughCumulativeMatchesToChooseAPlan() const {
            // This is equivalent to the default condition for switching from
            // a query to a getMore, which was the historical default match count for
            // choosing a plan.
            return _cumulativeCount >= 101;
        }
        bool enoughMatchesToRecordPlan() const {
            // Recording after 50 matches is a historical default (101 default limit / 2).
            return _count > 50;
        }
        int cumulativeCount() const { return _cumulativeCount; }
        int count() const { return _count; }
        void updateNscanned( long long nscanned ) {
            _aggregateNscanned += ( nscanned - _nscanned );
            _nscanned = nscanned;
        }
        long long nscanned() const { return _nscanned; }
        long long &aggregateNscanned() const { return _aggregateNscanned; }
    private:
        bool getsetdup( const DiskLoc &loc ) {
            if ( !_checkDups ) {
                return false;
            }
            pair<set<DiskLoc>::iterator, bool> p = _dups.insert( loc );
            return !p.second;
        }
        long long &_aggregateNscanned;
        long long _nscanned;
        int _cumulativeCount;
        int _count;
        bool _checkDups;
        enum MatchState { Unknown, False, True };
        MatchState _match;
        set<DiskLoc> _dups;
    };
    
    /** Dup tracking class, optimizing one common case with small set and few initial reads. */
    class SmallDupSet {
    public:
        SmallDupSet() : _accesses() {
            _vec.reserve( 250 );
        }
        bool getsetdup( const DiskLoc &loc ) {
            access();
            return vec() ? getsetdupVec( loc ) : getsetdupSet( loc );
        }
        bool getdup( const DiskLoc &loc ) {
            access();
            return vec() ? getdupVec( loc ) : getdupSet( loc );
        }            
    private:
        void access() {
            ++_accesses;
            mayUpgrade();
        }
        void mayUpgrade() {
            if ( vec() && _accesses > 500 ) {
                _set.insert( _vec.begin(), _vec.end() );
            }
        }
        bool vec() const {
            return _set.size() == 0;
        }
        bool getsetdupVec( const DiskLoc &loc ) {
            if ( getdupVec( loc ) ) {
                return true;
            }
            _vec.push_back( loc );
            return false;
        }
        bool getdupVec( const DiskLoc &loc ) const {
            for( vector<DiskLoc>::const_iterator i = _vec.begin(); i != _vec.end(); ++i ) {
                if ( *i == loc ) {
                    return true;
                }
            }
            return false;
        }
        bool getsetdupSet( const DiskLoc &loc ) {
            pair<set<DiskLoc>::iterator, bool> p = _set.insert(loc);
            return !p.second;
        }
        bool getdupSet( const DiskLoc &loc ) {
            return _set.count( loc ) > 0;
        }
        vector<DiskLoc> _vec;
        set<DiskLoc> _set;
        long long _accesses;
    };
} // namespace mongo
