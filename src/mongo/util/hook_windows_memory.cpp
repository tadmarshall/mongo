// hook_windows_memory.cpp

/*
 *    Copyright 2012 10gen Inc.
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

// Used to hook Windows functions that are imported in this executable's
// Import Address Table (IAT).

#if defined(_WIN32)

#include "mongo/platform/windows_basic.h"
#include "mongo/util/hook_windows_memory.h"
#include "mongo/util/hook_win32.h"
#include <vector>
#include <iostream>

namespace mongo {

    // list of hooked functions
    std::vector<HookWin32*> windowsHookList;

    // functions must be hooked in this order
    enum HookedFunction {
        index_HeapCreate,
        index_MapViewOfFile,
    };

    // hooked function typedefs and routines

    typedef HANDLE ( WINAPI *type_HeapCreate ) ( DWORD, SIZE_T, SIZE_T );
    HANDLE WINAPI myHeapCreate(
        DWORD flOptions,
        SIZE_T dwInitialSize,
        SIZE_T dwMaximumSize
    ) {
        std::cout << "HeapCreate called" << std::endl;
        return ( reinterpret_cast<type_HeapCreate>( windowsHookList[index_HeapCreate]->originalFunction() ) )( flOptions, dwInitialSize, dwMaximumSize );
    }

    typedef LPVOID ( WINAPI *type_MapViewOfFile ) ( HANDLE, DWORD, DWORD, DWORD, SIZE_T );
    LPVOID WINAPI myMapViewOfFile(
        HANDLE hFileMappingObject,
        DWORD dwDesiredAccess,
        DWORD dwFileOffsetHigh,
        DWORD dwFileOffsetLow,
        SIZE_T dwNumberOfBytesToMap
    ) {
        std::cout << "MapViewOfFile called" << std::endl;
        return ( reinterpret_cast<type_MapViewOfFile>( windowsHookList[index_MapViewOfFile]->originalFunction() ) )(
                hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap );
    }

    void hookWindowsMemory( void ) {
        windowsHookList.push_back( new HookWin32( "kernel32.dll", "HeapCreate", myHeapCreate ) );
        windowsHookList.push_back( new HookWin32( "kernel32.dll", "MapViewOfFile", myMapViewOfFile ) );

        // test the HeapCreate call
        HeapCreate( 0, 4 * 1024, 1024 * 1024 );
    }

    void unhookWindowsMemory( void ) {
        for ( int i = 0, len = windowsHookList.size(); i < len; ++i ) {
            delete windowsHookList[i];
        }
        windowsHookList.clear();
    }

}

#endif // #if defined(_WIN32)
