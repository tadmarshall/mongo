/*    Copyright 2013 10gen Inc.
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

#pragma once

#if defined(_WIN32)

// Windows will call the emulation of strcasestr()
//
namespace mongo {
namespace pal {
namespace emulation {
    extern const char* strcasestr(const char* haystack, const char* needle);
}
}
    using pal::emulation::strcasestr;
}

#elif defined(__sunos__)

// Solaris/SmartOS will call strcasestr() through a pointer
//
namespace mongo {
    typedef const char* (*StrCaseStrFunc)(const char* haystack, const char* needle);
namespace pal {
namespace dynamic {
    extern StrCaseStrFunc strcasestr;
}
}
    using pal::dynamic::strcasestr;
}

#else

// Other platforms will call the system strcasestr() directly
//
#include <cstring>
namespace mongo {
    using ::strcasestr;
}

#endif
