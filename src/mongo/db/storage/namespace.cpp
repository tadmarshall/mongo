// namespace.cpp

/**
*    Copyright (C) 2008 10gen Inc.
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

#include "mongo/db/storage/namespace.h"

#include <boost/static_assert.hpp>

#include "mongo/db/namespace_string.h"

namespace mongo {
    namespace {
        BOOST_STATIC_ASSERT( sizeof(Namespace) == 128 );
        BOOST_STATIC_ASSERT( Namespace::MaxNsLen == MaxDatabaseNameLen );
    }
}

