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

// Used to hook Windows functions that are imported in this executable's
// Import Address Table (IAT).

#pragma once

#if defined(_WIN32)

#include "mongo/platform/windows_basic.h"
#include <DbgHelp.h>
#include "mongo/util/concurrency/remap_lock.h"

namespace mongo {

    bool testHook( char* moduleName, char* functionName );

}

#endif // #if defined(_WIN32)
