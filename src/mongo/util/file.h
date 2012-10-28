// file.h cross platform basic file class. supports 64 bit offsets and such.

/*    Copyright 2009 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <boost/filesystem/operations.hpp>
#include <iostream>
#include <string>
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#endif

#include "mongo/platform/basic.h"
#include "mongo/platform/cstdint.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/text.h"

namespace mongo {

#ifndef _WIN32
    typedef int HANDLE;
#endif
    typedef uint64_t fileofs;

    // NOTE: not thread-safe. (at least the windows implementation isn't)

    class File {

        HANDLE _fd;
        bool _bad;
        std::string _name;

    public:
        File();
        ~File();

        static boost::intmax_t freeSpace(const std::string& path);

        bool bad() const { return _bad; }
        HANDLE fd() const { _fd; }
        void fsync() const;
        bool is_open() const;
        fileofs len();
        void open(const char* filename, bool readOnly = false, bool direct = false);
        void read(fileofs o, char* data, unsigned len);
        void truncate(fileofs size);
        void write(fileofs o, const char* data, unsigned len);
    };

}
