// hook_win32.cpp

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
#include "mongo/util/hook_win32.h"
#include <DbgHelp.h>

namespace mongo {

    HookWin32::HookWin32( char* moduleName, char* functionName, void* hookFunction ) {

        // look up the original function by module and function name
        _originalFunction = GetProcAddress( GetModuleHandleA( moduleName ), functionName );

        // get a pointer to this executable's Import Table
        char* myAddress = reinterpret_cast<char*>( GetModuleHandle( NULL ) );
        ULONG entrySize;
        void* imageEntry = ImageDirectoryEntryToDataEx(
                myAddress,                      // want info for this EXE file
                TRUE,                           // mapped as image
                IMAGE_DIRECTORY_ENTRY_IMPORT,   // desired directory
                &entrySize,                     // returned directory size
                NULL                            // returned section ptr (always zero for IAT)
        );
        IMAGE_IMPORT_DESCRIPTOR* importModule = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(imageEntry);

        // walk the imported module list until we find the desired module
        while ( importModule->Name ) {
            char* foundModuleName = myAddress + importModule->Name;
            if ( _strcmpi( foundModuleName, moduleName ) == 0 ) {

                // search the list of imported functions for the address of the one we want to hook
                IMAGE_THUNK_DATA* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>( myAddress + importModule->FirstThunk );
                while ( thunk->u1.Function ) {
                    if ( reinterpret_cast<void*>( thunk->u1.Function ) == _originalFunction ) {

                        // we found our function, remember its location and then hook it
                        _tablePointer = reinterpret_cast<void**>( &thunk->u1.Function );
                        MEMORY_BASIC_INFORMATION mbi;
                        VirtualQuery( _tablePointer, &mbi, sizeof(mbi) );
                        VirtualProtect( mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect );
                        *_tablePointer = hookFunction;
                        DWORD unused;
                        VirtualProtect( mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &unused );
                        return;
                    }
                    ++thunk;
                }
                return;
            }
            ++importModule;
        }
    }

    HookWin32::~HookWin32() {
        *_tablePointer = _originalFunction;
    }

}

#endif // #if defined(_WIN32)
