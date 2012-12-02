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

#include "mongo/s/type_database.h"

#include "mongo/s/field_parser.h"
#include "mongo/util/mongoutils/str.h"

namespace mongo {

    using mongoutils::str::stream;

    const std::string DatabaseType::ConfigNS = "config.databases";

    BSONField<std::string> DatabaseType::name("_id");
    BSONField<std::string> DatabaseType::primary("primary");
    BSONField<bool> DatabaseType::scattered("scatterCollections");
    BSONField<bool> DatabaseType::draining("draining");

    BSONField<bool> DatabaseType::DEPRECATED_partitioned("partitioned");
    BSONField<std::string> DatabaseType::DEPRECATED_name("name");
    BSONField<bool> DatabaseType::DEPRECATED_sharded("sharded");

    DatabaseType::DatabaseType() {
        clear();
    }

    DatabaseType::~DatabaseType() {
    }

    bool DatabaseType::isValid(std::string* errMsg) const {
        std::string dummy;
        if (errMsg == NULL) {
            errMsg = &dummy;
        }

        // All the mandatory fields must be present.
        if (_name.empty()) {
            *errMsg = stream() << "missing " << name.name() << " field";
            return false;
        }
        if (_primary.empty()) {
            *errMsg = stream() << "missing " << primary.name() << " field";
            return false;
        }

        return true;
    }

    BSONObj DatabaseType::toBSON() const {
        BSONObjBuilder builder;
        if (!_name.empty()) builder.append(name(), _name);
        if (!_primary.empty()) builder.append(primary(), _primary);
        if (_scattered) builder.append(scattered(), _scattered);
        if (_draining) builder.append(draining(), _draining);
        return builder.obj();
    }

    void DatabaseType::parseBSON(BSONObj source) {
        clear();

        bool ok = true;
        ok &= FieldParser::extract(source, name, "", &_name);
        ok &= FieldParser::extract(source, primary, "", &_primary);
        ok &= FieldParser::extract(source, scattered, false, &_scattered );
        ok &= FieldParser::extract(source, draining, false, &_draining);
        if (! ok) {
            clear();
            return;
        }

        //
        // backward compatibility
        //

        // There used to be a flag called "partitioned" that would allow collections
        // under that database to be sharded. We don't require that anymore.
        bool partitioned;
        if (! FieldParser::extract(source, DEPRECATED_partitioned, false, &partitioned)) {
            clear();
            return;
        }
    }

    void DatabaseType::clear() {
        _name.clear();
        _primary.clear();
        _scattered = false;
        _draining = false;
    }

    void DatabaseType::cloneTo(DatabaseType* other) {
        other->clear();
        other->_name = _name;
        other->_primary = _primary;
        other->_scattered = _scattered;
        other->_draining = _draining;
    }

    std::string DatabaseType::toString() const {
        return toBSON().toString();
    }

} // namespace mongo
