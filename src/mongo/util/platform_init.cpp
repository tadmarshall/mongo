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

#include <iosfwd>
#include <stdlib.h>

#include "mongo/base/init.h"
#include "mongo/util/log.h"
#include "mongo/util/stacktrace.h"

#ifdef _WIN32

namespace mongo {

    int crtDebugCallback(int debugHookCode, char* message, int* hook_retval) {
        log() << "*** C runtime error: " << message << std::endl;
        printStackTrace();
        *hook_retval = 0;   // 0 == continue, 1 == CrtDebugBreak
        return 1;           // 0 == not handled, non-0 == handled
    }

    MONGO_INITIALIZER(Behaviors_Win32)(InitializerContext*) {

        // do not display dialog on abort()
        _set_abort_behavior(0, _CALL_REPORTFAULT | _WRITE_ABORT_MSG);

        // send C runtime debug output to stderr
        //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        //_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        //_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        //_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        //_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        //_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

        // hook the C runtime's error display
        _CrtSetReportHook(crtDebugCallback);

        return Status::OK();
    }

} // namespace mongo

#endif // _WIN32
