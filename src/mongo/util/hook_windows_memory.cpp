// hook_windows_memory.cpp

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

// Used to hook Windows functions that allocate virtual memory

#if defined(_WIN32)

#include "mongo/platform/windows_basic.h"
#include "mongo/util/hook_windows_memory.h"
#include "mongo/util/hook_win32.h"
#include "mongo/util/concurrency/remap_lock.h"
#include <vector>

namespace mongo {

    // list of hooked functions
    static std::vector<HookWin32*> windowsHookList;

    // functions must be hooked in this order
    enum HookedFunction {
        index_RtlCreateHeap1,
        index_NtAllocateVirtualMemory1,
        index_RtlCreateHeap2,
        index_NtAllocateVirtualMemory2,
    };

    // hooked function typedefs and routines

    typedef PVOID ( WINAPI *type_RtlCreateHeap ) ( ULONG, PVOID, SIZE_T, SIZE_T, PVOID, void* /* PRTL_HEAP_PARAMETERS */ );
    static PVOID WINAPI myRtlCreateHeap1(
        ULONG Flags,
        PVOID HeapBase,
        SIZE_T ReserveSize,
        SIZE_T CommitSize,
        PVOID Lock,
        void* /* PRTL_HEAP_PARAMETERS */ Parameters
    ) {
        RemapLock remapLock;
        return ( reinterpret_cast<type_RtlCreateHeap>( windowsHookList[index_RtlCreateHeap1]->originalFunction() ) )(
                Flags, HeapBase, ReserveSize, CommitSize, Lock, Parameters );
    }
    static PVOID WINAPI myRtlCreateHeap2(
        ULONG Flags,
        PVOID HeapBase,
        SIZE_T ReserveSize,
        SIZE_T CommitSize,
        PVOID Lock,
        void* /* PRTL_HEAP_PARAMETERS */ Parameters
    ) {
        RemapLock remapLock;
        return ( reinterpret_cast<type_RtlCreateHeap>( windowsHookList[index_RtlCreateHeap2]->originalFunction() ) )(
                Flags, HeapBase, ReserveSize, CommitSize, Lock, Parameters );
    }

    typedef LONG ( WINAPI *type_NtAllocateVirtualMemory ) ( HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG );
    static LONG WINAPI myNtAllocateVirtualMemory1(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        ULONG_PTR ZeroBits,
        PSIZE_T RegionSize,
        ULONG AllocationType,
        ULONG Protect
    ) {
        RemapLock remapLock;
        return ( reinterpret_cast<type_NtAllocateVirtualMemory>( windowsHookList[index_NtAllocateVirtualMemory1]->originalFunction() ) )(
                ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect );
    }
    static LONG WINAPI myNtAllocateVirtualMemory2(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        ULONG_PTR ZeroBits,
        PSIZE_T RegionSize,
        ULONG AllocationType,
        ULONG Protect
    ) {
        RemapLock remapLock;
        return ( reinterpret_cast<type_NtAllocateVirtualMemory>( windowsHookList[index_NtAllocateVirtualMemory2]->originalFunction() ) )(
                ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect );
    }

    void hookWindowsMemory( void ) {

        // kernel32.dll calls directly prior to Windows 7
        char* hookModuleAddress = reinterpret_cast<char*>( GetModuleHandleA( "kernel32.dll" ) );
        windowsHookList.push_back( new HookWin32( hookModuleAddress, "ntdll.dll", "RtlCreateHeap", myRtlCreateHeap1 ) );
        windowsHookList.push_back( new HookWin32( hookModuleAddress, "ntdll.dll", "NtAllocateVirtualMemory", myNtAllocateVirtualMemory1 ) );

        // in Windows 7 and Server 2008 R2, calls are through kernelbase.dll
        hookModuleAddress = reinterpret_cast<char*>( GetModuleHandleA( "kernelbase.dll" ) );
        windowsHookList.push_back( new HookWin32( hookModuleAddress, "ntdll.dll", "RtlCreateHeap", myRtlCreateHeap2 ) );
        windowsHookList.push_back( new HookWin32( hookModuleAddress, "ntdll.dll", "NtAllocateVirtualMemory", myNtAllocateVirtualMemory2 ) );
    }

    void unhookWindowsMemory( void ) {
        for ( int i = 0, len = windowsHookList.size(); i < len; ++i ) {
            delete windowsHookList[i];
        }
        windowsHookList.clear();
    }

} //namespace mongo

#endif // #if defined(_WIN32)
