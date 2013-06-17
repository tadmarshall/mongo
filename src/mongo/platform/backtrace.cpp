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

#if !defined(_WIN32)
#if defined(__sunos__) || !defined(MONGO_HAVE_EXECINFO_BACKTRACE)

#include "mongo/platform/backtrace.h"

#include <boost/smart_ptr/scoped_array.hpp>
#include <dlfcn.h>
#include <string>
#include <ucontext.h>
#include <vector>

#include "mongo/base/init.h"
#include "mongo/base/status.h"

using std::string;
using std::vector;

namespace mongo {
namespace pal {

namespace {
    class WalkcontextCallback {
    public:
        WalkcontextCallback(uintptr_t* array, int size)
            : _position(0),
              _count(size),
              _addresses(array) {}
        static int callbackFunction(uintptr_t address,
                                    int signalNumber,
                                    WalkcontextCallback* thisContext) {
            if (thisContext->_position < thisContext->_count) {
                thisContext->_addresses[thisContext->_position++] = address;
                return 0;
            }
            return 1;
        }
        int getCount() const { return static_cast<int>(_position); }
    private:
        size_t _position;
        size_t _count;
        uintptr_t* _addresses;
    };
}

    typedef int (*WalkcontextCallbackFunc)(uintptr_t address, int signalNumber, void* thisContext);

    int backtrace_emulation(void** array, int size) {
        WalkcontextCallback walkcontextCallback(reinterpret_cast<uintptr_t*>(array), size);
        ucontext_t context;
        if (getcontext(&context) != 0) {
            return 0;
        }
        int wcReturn = walkcontext(
                &context,
                reinterpret_cast<WalkcontextCallbackFunc>(WalkcontextCallback::callbackFunction),
                reinterpret_cast<void*>(&walkcontextCallback));
        if (wcReturn == 0) {
            return walkcontextCallback.getCount();
        }
        return 0;
    }

    char** backtrace_symbols_emulation(void* const* array, int size) {
        vector<string> stringVector;
        vector<size_t> stringLengths;
        size_t blockSize = size * sizeof(char*);
        size_t blockPtr = blockSize;
        for (int i = 0; i < size; ++i) {
            const size_t BUFFER_SIZE = 1024;
            boost::scoped_array<char> stringBuffer(new char[BUFFER_SIZE]);
            addrtosymstr(reinterpret_cast<uintptr_t*>(array)[i], stringBuffer, BUFFER_SIZE);
            string oneString(stringBuffer);
            size_t thisLength = oneString.length() + 1;
            stringVector.push_back(oneString);
            stringLengths.push_back(thisLength);
            blockSize += thisLength;
        }
        char** singleBlock = static_cast<char**>(malloc(blockSize));
        for (int i = 0; i < size; ++i) {
            singleBlock[i] = reinterpret_cast<char*>(singleBlock) + blockPtr;
            strncpy(singleBlock[i], stringVector[i].c_str(), stringLengths[i]);
            blockPtr += stringLengths[i];
        }
        return singleBlock;
    }

    void backtrace_symbols_fd_emulation(void* const* array, int size, int fd) {
    }

    typedef int (*BacktraceFunc)(void** array, int size);
    static BacktraceFunc backtrace_switcher =
            mongo::pal::backtrace_emulation;

    typedef char** (*BacktraceSymbolsFunc)(void* const* array, int size);
    static BacktraceSymbolsFunc backtrace_symbols_switcher =
            mongo::pal::backtrace_symbols_emulation;

    typedef void (*BacktraceSymbolsFdFunc)(void* const* array, int size, int fd);
    static BacktraceSymbolsFdFunc backtrace_symbols_fd_switcher =
            mongo::pal::backtrace_symbols_fd_emulation;

    int backtrace(void** array, int size) {
        return backtrace_switcher(array, size);
    }

    char** backtrace_symbols(void* const* array, int size) {
        return backtrace_symbols_switcher(array, size);
    }

    void backtrace_symbols_fd(void* const* array, int size, int fd) {
        backtrace_symbols_fd_switcher(array, size, fd);
    }

} // namespace pal

    // 'backtrace()', 'backtrace_symbols()' and 'backtrace_symbols_fd()' on Solaris will call
    // emulation functions if the symbols are not found
    //
    MONGO_INITIALIZER_GENERAL(SolarisBacktrace,
                              MONGO_NO_PREREQUISITES,
                              ("default"))(InitializerContext* context) {
        void* functionAddress = dlsym(RTLD_DEFAULT, "backtraceX");
        if (functionAddress != NULL) {
            mongo::pal::backtrace_switcher =
                    reinterpret_cast<mongo::pal::BacktraceFunc>(functionAddress);
        }
        functionAddress = dlsym(RTLD_DEFAULT, "backtrace_symbolsX");
        if (functionAddress != NULL) {
            mongo::pal::backtrace_symbols_switcher =
                    reinterpret_cast<mongo::pal::BacktraceSymbolsFunc>(functionAddress);
        }
        functionAddress = dlsym(RTLD_DEFAULT, "backtrace_symbols_fdX");
        if (functionAddress != NULL) {
            mongo::pal::backtrace_symbols_fd_switcher =
                    reinterpret_cast<mongo::pal::BacktraceSymbolsFdFunc>(functionAddress);
        }
        return Status::OK();
    }

} // namespace mongo

#endif // #if defined(__sunos__)
#endif // #if !defined(_WIN32)
