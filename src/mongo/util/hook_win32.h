// hook_win32.h

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

// Used to hook Windows functions imported through the Import Address Table (IAT).

#pragma once

#if defined(_WIN32)

namespace mongo {

    struct HookWin32 {
        HookWin32(
            char* hookModuleAddress,    // ptr to start of importing executable
            char* functionModuleName,   // name of module containing function to hook
            char* functionName,         // name of function to hook
            void* hookFunction );       // ptr to replacement (hook) function
        ~HookWin32();
        void* originalFunction() const { return _originalFunction; };

    private:
        void*   _originalFunction;      // original (unhooked) function
        void**  _tablePointer;          // location in Import Address Table
    };

} // namespace mongo

#endif // #if defined(_WIN32)
