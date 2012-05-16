// @file hook_windows_memory.cpp : Used to hook Windows functions that allocate virtual memory

/**
*    Copyright (C) 2012 10gen Inc.
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(_WIN32)

#include "mongo/util/hook_windows_memory.h"

#include <vector>

#include "mongo/platform/windows_basic.h"
#include "mongo/util/concurrency/remap_lock.h"
#include "mongo/util/hook_win32.h"

namespace {

    // hooked function typedefs and routines

    typedef PVOID ( WINAPI *type_RtlCreateHeap ) ( ULONG, PVOID, SIZE_T, SIZE_T, PVOID, void* );

    type_RtlCreateHeap originalRtlCreateHeap1;

    static PVOID WINAPI myRtlCreateHeap1(
        ULONG Flags,
        PVOID HeapBase,
        SIZE_T ReserveSize,
        SIZE_T CommitSize,
        PVOID Lock,
        void* /* PRTL_HEAP_PARAMETERS */ Parameters
    ) {
        mongo::RemapLock remapLock;
        return originalRtlCreateHeap1( Flags, HeapBase, ReserveSize, CommitSize, Lock, Parameters );
    }

    type_RtlCreateHeap originalRtlCreateHeap2;

    static PVOID WINAPI myRtlCreateHeap2(
        ULONG Flags,
        PVOID HeapBase,
        SIZE_T ReserveSize,
        SIZE_T CommitSize,
        PVOID Lock,
        void* /* PRTL_HEAP_PARAMETERS */ Parameters
    ) {
        mongo::RemapLock remapLock;
        return originalRtlCreateHeap2( Flags, HeapBase, ReserveSize, CommitSize, Lock, Parameters );
    }

    typedef LONG ( WINAPI *type_NtAllocateVirtualMemory )
            ( HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG );

    type_NtAllocateVirtualMemory originalNtAllocateVirtualMemory1;

    static LONG WINAPI myNtAllocateVirtualMemory1(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        ULONG_PTR ZeroBits,
        PSIZE_T RegionSize,
        ULONG AllocationType,
        ULONG Protect
    ) {
        mongo::RemapLock remapLock;
        return originalNtAllocateVirtualMemory1(
                ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect );
    }

    type_NtAllocateVirtualMemory originalNtAllocateVirtualMemory2;

    static LONG WINAPI myNtAllocateVirtualMemory2(
        HANDLE ProcessHandle,
        PVOID* BaseAddress,
        ULONG_PTR ZeroBits,
        PSIZE_T RegionSize,
        ULONG AllocationType,
        ULONG Protect
    ) {
        mongo::RemapLock remapLock;
        return originalNtAllocateVirtualMemory2(
                ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect );
    }

} // namespace

namespace mongo {

    void hookWindowsMemory( void ) {

        // kernel32.dll calls directly prior to Windows 7
        char* hookModuleAddress = reinterpret_cast<char*>( GetModuleHandleA( "kernel32.dll" ) );
        originalRtlCreateHeap1 = reinterpret_cast<type_RtlCreateHeap>(
                HookWin32( hookModuleAddress, "ntdll.dll", "RtlCreateHeap", myRtlCreateHeap1 ) );
        originalNtAllocateVirtualMemory1 = reinterpret_cast<type_NtAllocateVirtualMemory>(
                HookWin32( hookModuleAddress, "ntdll.dll", "NtAllocateVirtualMemory",
                myNtAllocateVirtualMemory1 ) );

        // in Windows 7 and Server 2008 R2, calls are through kernelbase.dll
        hookModuleAddress = reinterpret_cast<char*>( GetModuleHandleA( "kernelbase.dll" ) );
        originalRtlCreateHeap2 = reinterpret_cast<type_RtlCreateHeap>(
                HookWin32( hookModuleAddress, "ntdll.dll", "RtlCreateHeap", myRtlCreateHeap2 ) );
        originalNtAllocateVirtualMemory2 = reinterpret_cast<type_NtAllocateVirtualMemory>(
                HookWin32( hookModuleAddress, "ntdll.dll", "NtAllocateVirtualMemory",
                myNtAllocateVirtualMemory2 ) );
    }

} //namespace mongo

#endif // #if defined(_WIN32)
