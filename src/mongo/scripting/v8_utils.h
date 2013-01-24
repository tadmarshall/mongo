// v8_utils.h

/*    Copyright 2009 10gen Inc.
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

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <v8.h>

namespace mongo {

#define jsassert(x,msg) uassert(16664, (msg), (x))

#define argumentCheck(mustBeTrue, errorMessage)         \
    if (!(mustBeTrue)) {                                \
        return v8AssertionException((errorMessage));    \
    }

    std::ostream& operator<<(std::ostream& s, const v8::Handle<v8::Value>& o);
    std::ostream& operator<<(std::ostream& s, const v8::Handle<v8::TryCatch>* try_catch);

    std::string toSTLString(const v8::Handle<v8::Value>& o);
    std::string toSTLString(const v8::TryCatch* try_catch);
    std::string v8ObjectToString(const v8::Handle<v8::Object>& o);

    class V8Scope;
    void installFork(V8Scope* scope,
                     v8::Handle<v8::Object>& global,
                     v8::Handle<v8::Context>& context);

    /** Throw a V8 exception from Mongo callback code; message text will be preceded by "Error: ".
     *  @param   errorMessage Error message text.
     *  @return  Empty handle to be returned from callback function.
     */
    v8::Handle<v8::Value> v8AssertionException(const char* errorMessage);
    v8::Handle<v8::Value> v8AssertionException(std::string& errorMessage);
}

