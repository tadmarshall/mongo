// @file util.cpp

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

#include "pch.h"
#include "goodies.h"
#include "unittest.h"
#include "file_allocator.h"
#include "optime.h"
#include "time_support.h"
#include "mongoutils/str.h"

#ifdef _WIN32
#include <DbgHelp.h>
#endif

namespace mongo {

    string hexdump(const char *data, unsigned len) {
        assert( len < 1000000 );
        const unsigned char *p = (const unsigned char *) data;
        stringstream ss;
        for( unsigned i = 0; i < 4 && i < len; i++ ) {
            ss << std::hex << setw(2) << setfill('0');
            unsigned n = p[i];
            ss << n;
            ss << ' ';
        }
        string s = ss.str();
        return s;
    }

    boost::thread_specific_ptr<string> _threadName;

    unsigned _setThreadName( const char * name ) {
        if ( ! name ) name = "NONE";

        static unsigned N = 0;

        if ( strcmp( name , "conn" ) == 0 ) {
            string* x = _threadName.get();
            if ( x && mongoutils::str::startsWith( *x , "conn" ) ) {
                int n = atoi( x->c_str() + 4 );
                if ( n > 0 )
                    return n;
                warning() << "unexpected thread name [" << *x << "] parsed to " << n << endl;
            }
            unsigned n = ++N;
            stringstream ss;
            ss << name << n;
            _threadName.reset( new string( ss.str() ) );
            return n;
        }

        _threadName.reset( new string(name) );
        return 0;
    }

#if defined(_WIN32)
#define MS_VC_EXCEPTION 0x406D1388
#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    void setWinThreadName(const char *name) {
        /* is the sleep here necessary???
           Sleep(10);
           */
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = -1;
        info.dwFlags = 0;
        __try {
            RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    unsigned setThreadName(const char *name) {
        unsigned n = _setThreadName( name );
#if !defined(_DEBUG)
        // naming might be expensive so don't do "conn*" over and over
        if( string("conn") == name )
            return n;
#endif
        setWinThreadName(name);
        return n;
    }

#else

    unsigned setThreadName(const char * name ) {
        return _setThreadName( name );
    }

#endif

    string getThreadName() {
        string * s = _threadName.get();
        if ( s )
            return *s;
        return "";
    }

    vector<UnitTest*> *UnitTest::tests = 0;
    bool UnitTest::running = false;

    const char *default_getcurns() { return ""; }
    const char * (*getcurns)() = default_getcurns;

    int logLevel = 0;
    int tlogLevel = 0;
    mongo::mutex Logstream::mutex("Logstream");
    int Logstream::doneSetup = Logstream::magicNumber();

    bool isPrime(int n) {
        int z = 2;
        while ( 1 ) {
            if ( z*z > n )
                break;
            if ( n % z == 0 )
                return false;
            z++;
        }
        return true;
    }

    int nextPrime(int n) {
        n |= 1; // 2 goes to 3...don't care...
        while ( !isPrime(n) )
            n += 2;
        return n;
    }

    struct UtilTest : public UnitTest {
        void run() {
            assert( isPrime(3) );
            assert( isPrime(2) );
            assert( isPrime(13) );
            assert( isPrime(17) );
            assert( !isPrime(9) );
            assert( !isPrime(6) );
            assert( nextPrime(4) == 5 );
            assert( nextPrime(8) == 11 );

            assert( endsWith("abcde", "de") );
            assert( !endsWith("abcde", "dasdfasdfashkfde") );

            assert( swapEndian(0x01020304) == 0x04030201 );

        }
    } utilTest;

    OpTime OpTime::last(0, 0);

    /* this is a good place to set a breakpoint when debugging, as lots of warning things
       (assert, wassert) call it.
    */
    void sayDbContext(const char *errmsg) {
        if ( errmsg ) {
            problem() << errmsg << endl;
        }
        printStackTrace();
    }

    /* note: can't use malloc herein - may be in signal handler.
             logLockless() likely does not comply and should still be fixed todo
             likewise class string?
    */
    void rawOut( const string &s ) {
        if( s.empty() ) return;

        char buf[64];
        time_t_to_String( time(0) , buf );
        /* truncate / don't show the year: */
        buf[19] = ' ';
        buf[20] = 0;

        Logstream::logLockless(buf);
        Logstream::logLockless(s);
        Logstream::logLockless("\n");
    }

    ostream& operator<<( ostream &s, const ThreadSafeString &o ) {
        s << o.toString();
        return s;
    }

    bool StaticObserver::_destroyingStatics = false;

#if defined(_WIN32)

    /**
     * Get the display name of the executable module containing the specified address.
     * 
     * @param process               Process handle
     * @param address               Address to find
     * @param returnedModuleName    Returned module name
     */
    static void getModuleName( HANDLE process, DWORD64 address, std::string* returnedModuleName ) {
        IMAGEHLP_MODULE64 module64;
        memset ( &module64, 0, sizeof(module64) );
        module64.SizeOfStruct = sizeof(module64);
        BOOL ret = SymGetModuleInfo64( process, address, &module64 );
        if ( FALSE == ret ) {
            returnedModuleName->clear();
            return;
        }
        char* moduleName = module64.LoadedImageName;
        char* backslash = strrchr( moduleName, '\\' );
        if ( backslash ) {
            moduleName = backslash + 1;
        }
        *returnedModuleName = moduleName;
    }

    /**
     * Get the display name and line number of the source file containing the specified address.
     * 
     * @param process               Process handle
     * @param address               Address to find
     * @param returnedSourceAndLine Returned source code file name with line number
     */
    static void getSourceFileAndLineNumber( HANDLE process,
                                            DWORD64 address,
                                            std::string* returnedSourceAndLine ) {
        IMAGEHLP_LINE64 line64;
        memset( &line64, 0, sizeof(line64) );
        line64.SizeOfStruct = sizeof(line64);
        DWORD displacement32;
        BOOL ret = SymGetLineFromAddr64( process, address, &displacement32, &line64 );
        if ( FALSE == ret ) {
            returnedSourceAndLine->clear();
            return;
        }

        std::string filename( line64.FileName );
        std::string::size_type start = filename.find( "\\src\\mongo\\" );
        if ( start == std::string::npos ) {
            start = filename.find( "\\src\\third_party\\" );
        }
        if ( start != std::string::npos ) {
            std::string shorter( "..." );
            shorter += filename.substr( start );
            filename.swap( shorter );
        }
        static const size_t bufferSize = 32;
        boost::scoped_array<char> lineNumber( new char[bufferSize] );
        _snprintf( lineNumber.get(), bufferSize, "(%u)", line64.LineNumber );
        filename += lineNumber.get();
        returnedSourceAndLine->swap( filename );
    }

    /**
     * Get the display text of the symbol and offset of the specified address.
     * 
     * @param process                   Process handle
     * @param address                   Address to find
     * @param symbolInfo                Caller's pre-built SYMBOL_INFO struct (for efficiency)
     * @param returnedSymbolAndOffset   Returned symbol and offset
     */
    static void getsymbolAndOffset( HANDLE process,
                                    DWORD64 address,
                                    SYMBOL_INFO* symbolInfo,
                                    std::string* returnedSymbolAndOffset ) {
        DWORD64 displacement64;
        BOOL ret = SymFromAddr( process, address, &displacement64, symbolInfo );
        if ( FALSE == ret ) {
            *returnedSymbolAndOffset = "???";
            return;
        }
        std::string symbolString( symbolInfo->Name );
        static const size_t bufferSize = 32;
        boost::scoped_array<char> symbolOffset( new char[bufferSize] );
        _snprintf( symbolOffset.get(), bufferSize, "+0x%x", displacement64 );
        symbolString += symbolOffset.get();
        returnedSymbolAndOffset->swap( symbolString );
    }

    struct TraceItem {
        std::string moduleName;
        std::string sourceAndLine;
        std::string symbolAndOffset;
    };

    static const int maxBackTraceFrames = 40;

    /**
     * Print a stack backtrace for the current thread to the specified ostream.
     * 
     * @param context   CONTEXT record for stack trace
     */
    void printWindowsStackTrace( CONTEXT& context ) {
        HANDLE process = GetCurrentProcess();
        BOOL ret = SymInitialize( process, NULL, TRUE );
        if ( ret == FALSE ) {
            DWORD dosError = GetLastError();
            log() << "Stack trace failed, SymInitialize failed with error " <<
                    std::dec << dosError << std::endl;
            return;
        }
        DWORD options = SymGetOptions();
        options |= SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
        SymSetOptions( options );

        STACKFRAME64 frame64;
        memset( &frame64, 0, sizeof(frame64) );

#if defined(_M_AMD64)
        DWORD imageType = IMAGE_FILE_MACHINE_AMD64;
        frame64.AddrPC.Offset = context.Rip;
        frame64.AddrFrame.Offset = context.Rbp;
        frame64.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
        DWORD imageType = IMAGE_FILE_MACHINE_I386;
        frame64.AddrPC.Offset = context.Eip;
        frame64.AddrFrame.Offset = context.Ebp;
        frame64.AddrStack.Offset = context.Esp;
#else
#error Neither _M_IX86 nor _M_AMD64 were defined
#endif
        frame64.AddrPC.Mode = AddrModeFlat;
        frame64.AddrFrame.Mode = AddrModeFlat;
        frame64.AddrStack.Mode = AddrModeFlat;

        const size_t nameSize = 1024;
        const size_t symbolBufferSize = sizeof(SYMBOL_INFO) + nameSize;
        boost::scoped_array<char> symbolCharBuffer( new char[symbolBufferSize] );
        memset( symbolCharBuffer.get(), 0, symbolBufferSize );
        SYMBOL_INFO* symbolBuffer = reinterpret_cast<SYMBOL_INFO*>( symbolCharBuffer.get() );
        symbolBuffer->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbolBuffer->MaxNameLen = nameSize;

        // build list
        std::vector<TraceItem> traceList;
        TraceItem traceItem;
        size_t moduleWidth = 0;
        size_t sourceWidth = 0;
        for ( size_t i = 0; i < maxBackTraceFrames; ++i ) {
            ret = StackWalk64( imageType,
                               process,
                               GetCurrentThread(),
                               &frame64,
                               &context,
                               NULL,
                               NULL,
                               NULL,
                               NULL );
            if ( ret == FALSE || frame64.AddrReturn.Offset == 0 ) {
                break;
            }
            DWORD64 address = frame64.AddrPC.Offset;
            getModuleName( process, address, &traceItem.moduleName );
            size_t width = traceItem.moduleName.length();
            if ( width > moduleWidth ) {
                moduleWidth = width;
            }
            getSourceFileAndLineNumber( process, address, &traceItem.sourceAndLine );
            width = traceItem.sourceAndLine.length();
            if ( width > sourceWidth ) {
                sourceWidth = width;
            }
            getsymbolAndOffset( process, address, symbolBuffer, &traceItem.symbolAndOffset );
            traceList.push_back( traceItem );
        }
        SymCleanup( process );

        // print list
        ++moduleWidth;
        ++sourceWidth;
        size_t frameCount = traceList.size();
        for ( size_t i = 0; i < frameCount; ++i ) {
            stringstream ss;
            ss << traceList[i].moduleName << " ";
            size_t width = traceList[i].moduleName.length();
            while ( width < moduleWidth ) {
                ss << " ";
                ++width;
            }
            ss << traceList[i].sourceAndLine << " ";
            width = traceList[i].sourceAndLine.length();
            while ( width < sourceWidth ) {
                ss << " ";
                ++width;
            }
            ss << traceList[i].symbolAndOffset;
            log() << ss.str() << std::endl;
        }
    }

#endif

} // namespace mongo
