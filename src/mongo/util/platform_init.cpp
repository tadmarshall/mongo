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

#if defined(_WIN32)
#include <crtdbg.h>
#include <stdlib.h>

#include "mongo/util/stacktrace.h"
#endif

#if defined(__sunos__)
#include <dlfcn.h>
#include <string>

#include "mongo/base/status.h"
#include "mongo/platform/strcasestr.h"
#include "mongo/util/log.h" // only needed if logging (debugging)
#endif

#include "mongo/base/init.h"

#if defined(_WIN32)

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

#if defined(__sunos__)

namespace mongo {

    MONGO_INITIALIZER(SolarisDynamicLinks)(InitializerContext*) {

    /*    Support run-time dynamic linking to functions that may or may not be present.
     *
     *    Some functions are available on Solaris 11 but not on Solaris 10.  In order to
     *    ship a single binary that runs on both versions, we do not generate load-time
     *    references to these functions but instead probe for them at run-time and either
     *    call them if available of provide fallback behavior if not available.
     */

        // try to find some functions (should be in libc.so.1 if found)
        static const char* libraryName = "/lib/64/libc.so.1";
        void* libraryHandle = dlopen(libraryName, RTLD_LAZY);
        //log() << "Library '" << libraryName << "' "
        //      << (std::string((libraryHandle != NULL) ? "was" : "was NOT" ))
        //      << " found" << std::endl;
        if (libraryHandle == NULL) {
            return Status::OK();
            //return Status(ErrorCodes::InternalError, "Library not found");
        }
        static const char* functionName = "strcasestr";
        void* functionAddress = dlsym(libraryHandle, functionName);

        // note whether or not it was found
        //log() << "Symbol '" << functionName << "' "
        //      << (std::string((functionAddress != NULL) ? "was" : "was NOT" ))
        //      << " found in '" << libraryName << "'" << std::endl;

        if (functionAddress != NULL) {
            pal::dynamic::strcasestr = reinterpret_cast<StrCaseStrFunc>(functionAddress);
        }
        return Status::OK();
    }

} // namespace mongo

#endif // __sunos__
