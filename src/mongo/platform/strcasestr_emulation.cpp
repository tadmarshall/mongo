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

#if defined(_WIN32) || defined(__sunos__)

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace mongo {

    /**
     * strcasestr_emulation -- case-insensitive search for a substring within another string.
     *
     * @param haystack      ptr to C-string to search
     * @param needle        ptr to C-string to try to find within 'haystack'
     * @return              ptr to start of 'needle' within 'haystack' if found, NULL otherwise
     */
    const char* strcasestr_emulation(const char* haystack, const char* needle) {

        std::string haystackLower(haystack);
        std::transform(haystackLower.begin(),
                       haystackLower.end(),
                       haystackLower.begin(),
                       ::tolower);

        std::string needleLower(needle);
        std::transform(needleLower.begin(),
                       needleLower.end(),
                       needleLower.begin(),
                       ::tolower);

        return strstr(haystackLower.c_str(), needleLower.c_str());
    }

} // namespace mongo

#endif // #if defined(_WIN32) || defined(__sunos__)
