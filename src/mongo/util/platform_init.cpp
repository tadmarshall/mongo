/**
*    Copyright (C) 2012 10gen Inc.
*
*    This program is free software: you can redistribute it and/or modify
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

#ifdef _WIN32
#include <crtdbg.h>
#include <stdlib.h>

#include "mongo/util/stacktrace.h"
#endif

#ifdef __sunos__
#include <dlfcn.h>
#include <string>

#include "mongo/base/status.h"
#include "mongo/util/log.h" // only needed if logging (debugging)
#endif

#include "mongo/base/init.h"

#ifdef _WIN32

namespace mongo {

    MONGO_INITIALIZER(Behaviors_Win32)(InitializerContext*) {

        // do not display dialog on abort()
        _set_abort_behavior(0, _CALL_REPORTFAULT | _WRITE_ABORT_MSG);

        // hook the C runtime's error display
        _CrtSetReportHook(crtDebugCallback);

        return Status::OK();
    }

} // namespace mongo

#endif // _WIN32

#ifdef __sunos__

namespace mongo {

    typedef char* (*StrCaseStrFunc)(const char* haystack, const char* needle);
    //return strcasestr( haystack.c_str(), phrase.c_str() ) > 0;

    MONGO_INITIALIZER(SolarisDynamicLinks)(InitializerContext*) {

        // try to find some functions (should be in libc.so.1 if found)
        static const char* libraryName = "/lib/64/libc.so.1";
        void* libraryHandle = dlopen(libraryName, RTLD_LAZY);
        log() << "Library '" << libraryName << "' "
              << (std::string((libraryHandle != NULL) ? "was" : "was NOT" ))
              << " found" << std::endl;
        if (libraryHandle == NULL) {
            return Status(ErrorCodes::InternalError, "Library not found");
        }
        static const char* functionName = "strcasestr";
        void* functionAddress = dlsym(libraryHandle, functionName);

        // note whether or not it was found
        log() << "Symbol '" << functionName << "' "
              << (std::string((functionAddress != NULL) ? "was" : "was NOT" ))
              << " found in '" << libraryName << "'" << std::endl;

        if (functionAddress == NULL) {
            return Status(ErrorCodes::InternalError, "Symbol not found");
        }
        return Status::OK();
    }

} // namespace mongo

#endif // __sunos__
