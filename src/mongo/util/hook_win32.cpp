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

#include "mongo/util/hook_win32.h"
#include <iostream>

namespace mongo {

    bool testHook( char* moduleName, char* functionName ) {
        std::cout << "  >>> Inside testHook()" << std::endl;
        char* myAddress = reinterpret_cast<char*>( GetModuleHandle( NULL ) );
        ULONG entrySize;
        IMAGE_SECTION_HEADER* entryPtr;
        void* imageEntry = ImageDirectoryEntryToDataEx(
                myAddress,                      // want info for this EXE file
                TRUE,                           // mapped as image
                IMAGE_DIRECTORY_ENTRY_IMPORT,   // desired directory
                &entrySize,                     // returned directory size
                &entryPtr                       // returned ptr to directory
        );
        IMAGE_IMPORT_DESCRIPTOR* importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(imageEntry);
        for ( size_t index = 0; index < entrySize; index += sizeof(IMAGE_IMPORT_DESCRIPTOR) ) {
            if ( importDescriptor->Name == 0 ) {
                break;
            }
            char* foundModuleName = myAddress + importDescriptor->Name;
            if ( _strcmpi(foundModuleName, moduleName) == 0 ) {
                std::cout << "Found module '" << foundModuleName << "'" << std::endl;
                return true;
            }
            ++importDescriptor;
        }
        return false;
    }

}

#endif // #if defined(_WIN32)
