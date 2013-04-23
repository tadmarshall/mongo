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

#include <boost/filesystem.hpp>
#include <iostream>
#include <malloc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "mongo/util/file.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/processinfo.h"

using namespace std;

#define KLF "l"

namespace mongo {

    class LinuxProc {
    public:
        LinuxProc( pid_t pid = getpid() ) {
            char name[128];
            sprintf( name , "/proc/%d/stat"  , pid );

            FILE * f = fopen( name , "r");
            if ( ! f ) {
                stringstream ss;
                ss << "couldn't open [" << name << "] " << errnoWithDescription();
                string s = ss.str();
                msgasserted( 98765 , s.c_str() );
                //msgassertedNoTrace( 16809 , s.c_str() );
            }
            int found = fscanf(f,
                               "%d %s %c "
                               "%d %d %d %d %d "
                               "%lu %lu %lu %lu %lu "
                               "%lu %lu %ld %ld "  /* utime stime cutime cstime */
                               "%ld %ld "
                               "%ld "
                               "%ld "
                               "%lu "  /* start_time */
                               "%lu "
                               "%ld " // rss
                               "%lu %" KLF "u %" KLF "u %" KLF "u %" KLF "u %" KLF "u "
                               /*
                                 "%*s %*s %*s %*s "
                                 "%"KLF"u %*lu %*lu "
                                 "%d %d "
                                 "%lu %lu"
                               */

                               ,

                               &_pid,
                               _comm,
                               &_state,
                               &_ppid, &_pgrp, &_session, &_tty, &_tpgid,
                               &_flags, &_min_flt, &_cmin_flt, &_maj_flt, &_cmaj_flt,
                               &_utime, &_stime, &_cutime, &_cstime,
                               &_priority, &_nice,
                               &_nlwp,
                               &_alarm,
                               &_start_time,
                               &_vsize,
                               &_rss,
                               &_rss_rlim, &_start_code, &_end_code, &_start_stack, &_kstk_esp, &_kstk_eip

                               /*
                                 &_wchan,
                                 &_exit_signal, &_processor,
                                 &_rtprio, &_sched
                               */
                              );
            if ( found == 0 ) {
                cout << "system error: reading proc info" << endl;
            }
            fclose( f );
        }

        unsigned long getVirtualMemorySize() {
            return _vsize;
        }

        unsigned long getResidentSize() {
            return (unsigned long)_rss * 4 * 1024;
        }

        int _pid;
        // The process ID.

        char _comm[128];
        // The filename of the executable, in parentheses.  This is visible whether or not the executable is swapped out.

        char _state;
        //One character from the string "RSDZTW" where R is running, S is sleeping in an interruptible wait, D is waiting  in  uninterruptible
        //  disk sleep, Z is zombie, T is traced or stopped (on a signal), and W is paging.

        int _ppid;
        // The PID of the parent.

        int _pgrp;
        // The process group ID of the process.

        int _session;
        // The session ID of the process.

        int _tty;
        // The tty the process uses.

        int _tpgid;
        // The process group ID of the process which currently owns the tty that the process is connected to.

        unsigned long _flags; // %lu
        // The  kernel flags word of the process. For bit meanings, see the PF_* defines in <linux/sched.h>.  Details depend on the kernel version.

        unsigned long _min_flt; // %lu
        // The number of minor faults the process has made which have not required loading a memory page from disk.

        unsigned long _cmin_flt; // %lu
        // The number of minor faults that the process

        unsigned long _maj_flt; // %lu
        // The number of major faults the process has made which have required loading a memory page from disk.

        unsigned long _cmaj_flt; // %lu
        // The number of major faults that the process

        unsigned long _utime; // %lu
        // The number of jiffies that this process has been scheduled in user mode.

        unsigned long _stime; //  %lu
        // The number of jiffies that this process has been scheduled in kernel mode.

        long _cutime; // %ld
        // The number of jiffies that this removed field.

        long _cstime; // %ld

        long _priority;
        long _nice;

        long _nlwp; // %ld
        // number of threads

        unsigned long _alarm;
        // The time in jiffies before the next SIGALRM is sent to the process due to an interval timer. (unused since 2.6.17)

        unsigned long _start_time; // %lu
        // The time in jiffies the process started after system boot.

        unsigned long _vsize; // %lu
        // Virtual memory size in bytes.

        long _rss; // %ld
        // Resident Set Size: number of pages the process has in real memory, minus 3 for administrative purposes. This is just the pages which
        // count  towards  text,  data, or stack space.  This does not include pages which have not been demand-loaded in, or which are swapped out

        unsigned long _rss_rlim; // %lu
        // Current limit in bytes on the rss of the process (usually 4294967295 on i386).

        unsigned long _start_code; // %lu
        // The address above which program text can run.

        unsigned long _end_code; // %lu
        // The address below which program text can run.

        unsigned long _start_stack; // %lu
        // The address of the start of the stack.

        unsigned long _kstk_esp; // %lu
        // The current value of esp (stack pointer), as found in the kernel stack page for the process.

        unsigned long _kstk_eip; // %lu
        // The current EIP (instruction pointer).

    };

    class PlatformHelper {
    public:

        /**
        * Read the first 1023 bytes from a file
        */
        static string readLineFromFile( const char* fname ) {
            FILE* f;
            char fstr[1024] = { 0 };

            f = fopen( fname, "r" );
            if ( f != NULL ) {
                if ( fgets( fstr, 1023, f ) != NULL )
                    fstr[strlen( fstr ) < 1 ? 0 : strlen( fstr ) - 1] = '\0';
                fclose( f );
            }
            return fstr;
        }

   };

    ProcessInfo::ProcessInfo( pid_t pid ) : _pid( pid ) { }
    ProcessInfo::~ProcessInfo() { }

    bool ProcessInfo::supported() {
        return true;
    }

    int ProcessInfo::getVirtualMemorySize() {
        LinuxProc p(_pid);
        return (int)( p.getVirtualMemorySize() / ( 1024.0 * 1024 ) );
    }

    int ProcessInfo::getResidentSize() {
        LinuxProc p(_pid);
        return (int)( p.getResidentSize() / ( 1024.0 * 1024 ) );
    }

    void ProcessInfo::getExtraInfo( BSONObjBuilder& info ) {
        LinuxProc p(_pid);
        info.appendNumber("page_faults", static_cast<long long>(p._maj_flt) );
    }

    /**
     * Save a BSON obj representing the host system's details
     */
    void ProcessInfo::SystemInfo::collectSystemInfo() {
        struct utsname unameData;
        if (uname(&unameData) == -1) {
            log() << "Unable to collect detailed system information: " << strerror( errno ) << endl;
        }

        char buf_64[32];
        char buf_native[32];
        if (sysinfo(SI_ARCHITECTURE_64, buf_64, sizeof(buf_64)) != -1 &&
            sysinfo(SI_ARCHITECTURE_NATIVE, buf_native, sizeof(buf_native)) != -1) {
            addrSize = mongoutils::str::equals(buf_64, buf_native) ? 64 : 32;
        }
        else {
            log() << "Unable to determine system architecture: " << strerror( errno ) << endl;
        }

        osType = unameData.sysname;
        osName = mongoutils::str::ltrim(PlatformHelper::readLineFromFile("/etc/release"));
        osVersion = unameData.version;
        pageSize = static_cast<unsigned long long>(sysconf(_SC_PAGESIZE));
        memSize = pageSize * static_cast<unsigned long long>(sysconf(_SC_PHYS_PAGES));
        numCores = static_cast<unsigned>(sysconf(_SC_NPROCESSORS_CONF));
        cpuArch = unameData.machine;
        hasNuma = checkNumaEnabled();

        BSONObjBuilder bExtra;
        bExtra.append("kernelVersion", unameData.release);
        bExtra.append("pageSize", static_cast<long long>(pageSize));
        bExtra.append("numPages", static_cast<int>(sysconf(_SC_PHYS_PAGES)));
        bExtra.append("maxOpenFiles", static_cast<int>(sysconf(_SC_OPEN_MAX)));
        _extraStats = bExtra.obj();
    }

    /**
     * Determine if the process is running with (cc)NUMA
     */
    bool ProcessInfo::checkNumaEnabled() {
        if ( boost::filesystem::exists( "/sys/devices/system/node/node1" ) &&
             boost::filesystem::exists( "/proc/self/numa_maps" ) ) {
            // proc is populated with numa entries

            // read the second column of first line to determine numa state
            // ('default' = enabled, 'interleave' = disabled).  Logic from version.cpp's warnings.
            string line = PlatformHelper::readLineFromFile( "/proc/self/numa_maps" ).append( " \0" );
            size_t pos = line.find(' ');
            if ( pos != string::npos && line.substr( pos+1, 10 ).find( "interleave" ) == string::npos )
                // interleave not found;
                return true;
        }
        return false;
    }

    bool ProcessInfo::blockCheckSupported() {
        return true;
    }

    bool ProcessInfo::blockInMemory(const void* start) {
        char x = 0;
        if (mincore(static_cast<char*>(const_cast<void*>(alignToStartOfPage(start))), getPageSize(), &x)) {
            log() << "mincore failed: " << errnoWithDescription() << endl;
            return 1;
        }
        return x & 0x1;
    }

    bool ProcessInfo::pagesInMemory(const void* start, size_t numPages, vector<char>* out) {
        out->resize(numPages);
        if (mincore(static_cast<char*>(const_cast<void*>(alignToStartOfPage(start))),
                    numPages * getPageSize(),
                    &out->front())) {
            log() << "mincore failed: " << errnoWithDescription() << endl;
            return false;
        }
        for (size_t i = 0; i < numPages; ++i) {
            (*out)[i] &= 0x1;
        }
        return true;
    }

}
