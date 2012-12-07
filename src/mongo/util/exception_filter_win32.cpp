/*    Copyright 2012 10gen Inc.
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

#ifdef _WIN32

#include <ostream>

#include "mongo/platform/basic.h"
#include <DbgHelp.h>
#include "mongo/util/assert_util.h"
#include "mongo/util/exit_code.h"
#include "mongo/util/log.h"
#include "mongo/util/stacktrace.h"

namespace mongo {

    /* create a process dump.
        To use, load up windbg.  Set your symbol and source path.
        Open the crash dump file.  To see the crashing context, use .ecxr
        */
    void doMinidump(struct _EXCEPTION_POINTERS* exceptionInfo) {
        LPCWSTR dumpFilename = L"mongo.dmp";
        HANDLE hFile = CreateFileW(dumpFilename,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if ( INVALID_HANDLE_VALUE == hFile ) {
            DWORD lasterr = GetLastError();
            log() << "failed to open minidump file " << toUtf8String(dumpFilename) << " : "
                  << errnoWithDescription( lasterr ) << std::endl;
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION aMiniDumpInfo;
        aMiniDumpInfo.ThreadId = GetCurrentThreadId();
        aMiniDumpInfo.ExceptionPointers = exceptionInfo;
        aMiniDumpInfo.ClientPointers = TRUE;

        log() << "writing minidump diagnostic file " << toUtf8String(dumpFilename) << std::endl;
        BOOL bstatus = MiniDumpWriteDump(GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpNormal,
            &aMiniDumpInfo,
            NULL,
            NULL);
        if ( FALSE == bstatus ) {
            DWORD lasterr = GetLastError();
            log() << "failed to create minidump : "
                  << errnoWithDescription( lasterr ) << std::endl;
        }

        CloseHandle(hFile);
    }

    LONG WINAPI exceptionFilter( struct _EXCEPTION_POINTERS *excPointers ) {
        char exceptionString[128];
        sprintf_s( exceptionString, sizeof( exceptionString ),
                ( excPointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ) ?
                "(access violation)" : "0x%08X", excPointers->ExceptionRecord->ExceptionCode );
        char addressString[32];
        sprintf_s( addressString, sizeof( addressString ), "0x%p",
                 excPointers->ExceptionRecord->ExceptionAddress );
        log() << "*** unhandled exception " << exceptionString
              << " at " << addressString << ", terminating" << std::endl;
        if ( excPointers->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ) {
            ULONG acType = excPointers->ExceptionRecord->ExceptionInformation[0];
            const char* acTypeString;
            switch ( acType ) {
            case 0:
                acTypeString = "read from";
                break;
            case 1:
                acTypeString = "write to";
                break;
            case 8:
                acTypeString = "DEP violation at";
                break;
            default:
                acTypeString = "unknown violation at";
                break;
            }
            sprintf_s( addressString, sizeof( addressString ), " 0x%p",
                     excPointers->ExceptionRecord->ExceptionInformation[1] );
            log() << "*** access violation was a " << acTypeString << addressString << std::endl;
        }

        log() << "*** stack trace for unhandled exception:" << std::endl;
        printWindowsStackTrace( *excPointers->ContextRecord );
        doMinidump(excPointers);

        // Don't go through normal shutdown procedure. It may make things worse.
        log() << "*** immediate exit due to unhandled exception" << std::endl;
        ::_exit(EXIT_ABRUPT);

        // We won't reach here
        return EXCEPTION_EXECUTE_HANDLER;
    }

    LPTOP_LEVEL_EXCEPTION_FILTER filtLast = 0;

    void setWindowsUnhandledExceptionFilter() {
        filtLast = SetUnhandledExceptionFilter(exceptionFilter);
    }

} // namespace mongo

#endif // _WIN32
