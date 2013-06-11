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

/*    Support run-time dynamic linking to functions that may or may not be present.
 *
 *    Some functions are available on Solaris 11 but not on Solaris 10.  In order to
 *    ship a single binary that runs on both versions, we do not generate load-time
 *    references to these functions but instead probe for them at run-time and either
 *    call them if available of provide fallback behavior if not available.
 */

#pragma once

//#if defined(__sunos__)

namespace mongo {

    /**
     * strcasestr -- case-insensitive search for a substring within another string.
     *
     * @param haystack      ptr to C-string to search
     * @param needle        ptr to C-string to try to find within 'haystack'
     * @return              ptr to start of 'needle' within 'haystack' if found, NULL otherwise
     */
    typedef const char* (*StrCaseStrFunc)(const char* haystack, const char* needle);
    extern StrCaseStrFunc strcasestr_ptr;

} // namespace mongo

//#endif
