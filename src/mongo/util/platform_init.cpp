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

/*    Support run-time dynamic linking to functions that may or may not be present.
 *
 *    Some functions are available on Solaris 11 but not on Solaris 10.  In order to
 *    ship a single binary that runs on both versions, we do not generate load-time
 *    references to these functions but instead probe for them at run-time and either
 *    call them if available or provide fallback behavior if not available.
 */

namespace mongo {

    MONGO_INITIALIZER(SolarisDynamicLinks)(InitializerContext*) {

        void* functionAddress = dlsym(RTLD_DEFAULT, "strcasestr");
        if (functionAddress != NULL) {
            pal::dynamic::strcasestr = reinterpret_cast<StrCaseStrFunc>(functionAddress);
        }
        return Status::OK();
    }

} // namespace mongo

#endif // __sunos__
