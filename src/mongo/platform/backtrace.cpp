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

//#if !defined(_WIN32)
//#if defined(__sunos__) || !defined(MONGO_HAVE_EXECINFO_BACKTRACE)

#include "mongo/platform/backtrace.h"

#include <boost/smart_ptr/scoped_array.hpp>
#include <dlfcn.h>
#include <stdio.h>
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

    static int addrtosymstr(void* address, char* outputBuffer, int outputBufferSize) {
        Dl_info_t symbolInfo;
        if (dladdr(address, &symbolInfo) == 0) {    // no info: "[address]"
            return snprintf(outputBuffer, outputBufferSize, "[0x%p]", address);
        }
        if (symbolInfo.dli_sname == NULL) {
            return snprintf(outputBuffer,           // no symbol: "filename'offset [address]"
                            outputBufferSize,
                            "%s'0x%x [0x%p]",
                            symbolInfo.dli_fname,
                            reinterpret_cast<char*>(address) -
                                    reinterpret_cast<char*>(symbolInfo.dli_fbase),
                            address);
        }
        else {
            return snprintf(outputBuffer,           // symbol: "filename'symbol+offset [address]"
                            outputBufferSize,
                            "%s'%s+0x%x [0x%p]",
                            symbolInfo.dli_fname,
                            symbolInfo.dli_sname,
                            reinterpret_cast<char*>(address) -
                                    reinterpret_cast<char*>(symbolInfo.dli_saddr),
                            address);
        }
    }

    char** backtrace_symbols_emulation(void* const* array, int size) {
        vector<string> stringVector;
        vector<size_t> stringLengths;
        size_t blockSize = size * sizeof(char*);
        size_t blockPtr = blockSize;
        const size_t BUFFER_SIZE = 1024;
        boost::scoped_array<char> stringBuffer(new char[BUFFER_SIZE]);
        for (int i = 0; i < size; ++i) {
            addrtosymstr(array[i], stringBuffer.get(), BUFFER_SIZE);
            string oneString(stringBuffer.get());
            size_t thisLength = oneString.length() + 1;
            stringVector.push_back(oneString);
            stringLengths.push_back(thisLength);
            blockSize += thisLength;
        }
        char** singleBlock = static_cast<char**>(malloc(blockSize));
        if (singleBlock == NULL) {
            return NULL;
        }
        for (int i = 0; i < size; ++i) {
            singleBlock[i] = reinterpret_cast<char*>(singleBlock) + blockPtr;
            strncpy(singleBlock[i], stringVector[i].c_str(), stringLengths[i]);
            blockPtr += stringLengths[i];
        }
        return singleBlock;
    }

    void backtrace_symbols_fd_emulation(void* const* array, int size, int fd) {
        const size_t BUFFER_SIZE = 1024;
        char stringBuffer[BUFFER_SIZE];
        for (int i = 0; i < size; ++i) {
            int len = addrtosymstr(array[i], stringBuffer, BUFFER_SIZE);
            if (len >= BUFFER_SIZE) {
                len = BUFFER_SIZE - 1;
            }
            stringBuffer[len] = '\n';
            write(fd, stringBuffer, len + 1);
        }
    }

    typedef int (*BacktraceFunc)(void** array, int size);
    static BacktraceFunc backtrace_switcher =
            pal::backtrace_emulation;

    typedef char** (*BacktraceSymbolsFunc)(void* const* array, int size);
    static BacktraceSymbolsFunc backtrace_symbols_switcher =
            pal::backtrace_symbols_emulation;

    typedef void (*BacktraceSymbolsFdFunc)(void* const* array, int size, int fd);
    static BacktraceSymbolsFdFunc backtrace_symbols_fd_switcher =
            pal::backtrace_symbols_fd_emulation;

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
        void* functionAddress = dlsym(RTLD_DEFAULT, "backtrace");
        if (functionAddress != NULL) {
            pal::backtrace_switcher =
                    reinterpret_cast<pal::BacktraceFunc>(functionAddress);
        }
        functionAddress = dlsym(RTLD_DEFAULT, "backtrace_symbols");
        if (functionAddress != NULL) {
            pal::backtrace_symbols_switcher =
                    reinterpret_cast<pal::BacktraceSymbolsFunc>(functionAddress);
        }
        functionAddress = dlsym(RTLD_DEFAULT, "backtrace_symbols_fd");
        if (functionAddress != NULL) {
            pal::backtrace_symbols_fd_switcher =
                    reinterpret_cast<pal::BacktraceSymbolsFdFunc>(functionAddress);
        }
        return Status::OK();
    }

} // namespace mongo

//#endif // #if defined(__sunos__)
//#endif // #if !defined(_WIN32)
