/*
 * Process tests
 *
 * Copyright 2021 Jinoh Kang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <stdlib.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <winternl.h>

#include "wine/test.h"

static NTSTATUS (WINAPI *pNtQueryObject)(HANDLE,OBJECT_INFORMATION_CLASS,PVOID,ULONG,PULONG);

static BOOL (WINAPI *pCompareObjectHandles)(HANDLE, HANDLE);
static HANDLE (WINAPI *pOpenFileMappingFromApp)( ULONG, BOOL, LPCWSTR);
static HANDLE (WINAPI *pCreateFileMappingFromApp)(HANDLE, PSECURITY_ATTRIBUTES, ULONG, ULONG64, PCWSTR);
static LPVOID (WINAPI *pMapViewOfFileFromApp)(HANDLE, ULONG, ULONG64, SIZE_T);
static LPVOID (WINAPI *pMapViewOfFile3)(HANDLE, HANDLE, PVOID, ULONG64 offset, SIZE_T size,
        ULONG, ULONG, MEM_EXTENDED_PARAMETER *, ULONG);
static LPVOID (WINAPI *pVirtualAlloc2)(HANDLE, void *, SIZE_T, DWORD, DWORD, MEM_EXTENDED_PARAMETER *, ULONG);

static void test_CompareObjectHandles(void)
{
    HANDLE h1, h2;
    BOOL ret;

    if (!pCompareObjectHandles)
    {
        skip("CompareObjectHandles is not available.\n");
        return;
    }

    ret = pCompareObjectHandles( GetCurrentProcess(), GetCurrentProcess() );
    ok( ret, "comparing GetCurrentProcess() to self failed with %u\n", GetLastError() );

    ret = pCompareObjectHandles( GetCurrentThread(), GetCurrentThread() );
    ok( ret, "comparing GetCurrentThread() to self failed with %u\n", GetLastError() );

    SetLastError(0);
    ret = pCompareObjectHandles( GetCurrentProcess(), GetCurrentThread() );
    ok( !ret && GetLastError() == ERROR_NOT_SAME_OBJECT,
        "comparing GetCurrentProcess() to GetCurrentThread() returned %u\n", GetLastError() );

    h1 = NULL;
    ret = DuplicateHandle( GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(),
                           &h1, 0, FALSE, DUPLICATE_SAME_ACCESS );
    ok( ret, "failed to duplicate current process handle: %u\n", GetLastError() );

    ret = pCompareObjectHandles( GetCurrentProcess(), h1 );
    ok( ret, "comparing GetCurrentProcess() with %p failed with %u\n", h1, GetLastError() );

    CloseHandle( h1 );

    h1 = CreateFileA( "\\\\.\\NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0 );
    ok( h1 != INVALID_HANDLE_VALUE, "CreateFile failed (%d)\n", GetLastError() );

    h2 = NULL;
    ret = DuplicateHandle( GetCurrentProcess(), h1, GetCurrentProcess(),
                           &h2, 0, FALSE, DUPLICATE_SAME_ACCESS );
    ok( ret, "failed to duplicate handle %p: %u\n", h1, GetLastError() );

    ret = pCompareObjectHandles( h1, h2 );
    ok( ret, "comparing %p with %p failed with %u\n", h1, h2, GetLastError() );

    CloseHandle( h2 );

    h2 = CreateFileA( "\\\\.\\NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0 );
    ok( h2 != INVALID_HANDLE_VALUE, "CreateFile failed (%d)\n", GetLastError() );

    SetLastError(0);
    ret = pCompareObjectHandles( h1, h2 );
    ok( !ret && GetLastError() == ERROR_NOT_SAME_OBJECT,
        "comparing %p with %p returned %u\n", h1, h2, GetLastError() );

    CloseHandle( h2 );
    CloseHandle( h1 );
}

static void test_MapViewOfFile3(void)
{
    static const char testfile[] = "testfile.xxx";
    HANDLE file, mapping;
    void *ptr;
    BOOL ret;

    if (!pMapViewOfFile3)
    {
        win_skip("MapViewOfFile3() is not supported.\n");
        return;
    }

    SetLastError(0xdeadbeef);
    file = CreateFileA( testfile, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "CreateFile error %lu\n", GetLastError() );
    SetFilePointer( file, 12288, NULL, FILE_BEGIN );
    SetEndOfFile( file );

    SetLastError(0xdeadbeef);
    mapping = CreateFileMappingA( file, NULL, PAGE_READWRITE, 0, 4096, NULL );
    ok( mapping != 0, "CreateFileMapping error %lu\n", GetLastError() );

    SetLastError(0xdeadbeef);
    ptr = pMapViewOfFile3( mapping, GetCurrentProcess(), NULL, 0, 4096, 0, PAGE_READONLY, NULL, 0);
    ok( ptr != NULL, "MapViewOfFile FILE_MAP_READ error %lu\n", GetLastError() );
    UnmapViewOfFile( ptr );

    CloseHandle( mapping );
    CloseHandle( file );
    ret = DeleteFileA( testfile );
    ok(ret, "Failed to delete a test file.\n");
}

static void test_VirtualAlloc2(void)
{
    void *placeholder1, *placeholder2, *view1, *view2, *addr;
    MEMORY_BASIC_INFORMATION info;
    HANDLE section;
    SIZE_T size;
    BOOL ret;

    if (!pVirtualAlloc2)
    {
        win_skip("VirtualAlloc2() is not supported.\n");
        return;
    }

    size = 0x80000;
    addr = pVirtualAlloc2(NULL, NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE, NULL, 0);
    ok(!!addr, "Failed to allocate, error %lu.\n", GetLastError());
    ret = VirtualFree(addr, 0, MEM_RELEASE);
    ok(ret, "Unexpected return value %d, error %lu.\n", ret, GetLastError());

    /* Placeholder splitting functionality */
    placeholder1 = pVirtualAlloc2(NULL, NULL, 2 * size, MEM_RESERVE_PLACEHOLDER | MEM_RESERVE, PAGE_NOACCESS, NULL, 0);
    ok(!!placeholder1, "Failed to create a placeholder range.\n");
    if (!placeholder1) return;

    memset(&info, 0, sizeof(info));
    VirtualQuery(placeholder1, &info, sizeof(info));
    ok(info.AllocationProtect == PAGE_NOACCESS, "Unexpected protection %#lx.\n", info.AllocationProtect);
    ok(info.State == MEM_RESERVE, "Unexpected state %#lx.\n", info.State);
    ok(info.Type == MEM_PRIVATE, "Unexpected type %#lx.\n", info.Type);
    ok(info.RegionSize == 2 * size, "Unexpected size.\n");

    ret = VirtualFree(placeholder1, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    ok(ret, "Failed to split placeholder.\n");

    memset(&info, 0, sizeof(info));
    VirtualQuery(placeholder1, &info, sizeof(info));
    ok(info.AllocationProtect == PAGE_NOACCESS, "Unexpected protection %#lx.\n", info.AllocationProtect);
    ok(info.State == MEM_RESERVE, "Unexpected state %#lx.\n", info.State);
    ok(info.Type == MEM_PRIVATE, "Unexpected type %#lx.\n", info.Type);
    ok(info.RegionSize == size, "Unexpected size.\n");

    placeholder2 = (void *)((BYTE *)placeholder1 + size);
    memset(&info, 0, sizeof(info));
    VirtualQuery(placeholder2, &info, sizeof(info));
    ok(info.AllocationProtect == PAGE_NOACCESS, "Unexpected protection %#lx.\n", info.AllocationProtect);
    ok(info.State == MEM_RESERVE, "Unexpected state %#lx.\n", info.State);
    ok(info.Type == MEM_PRIVATE, "Unexpected type %#lx.\n", info.Type);
    ok(info.RegionSize == size, "Unexpected size.\n");

    section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, NULL);
    ok(!!section, "Failed to create a section.\n");

    view1 = pMapViewOfFile3(section, NULL, placeholder1, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    ok(!!view1, "Failed to map a section.\n");

    view2 = pMapViewOfFile3(section, NULL, placeholder2, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    ok(!!view2, "Failed to map a section.\n");

    memset(&info, 0, sizeof(info));
    VirtualQuery(placeholder1, &info, sizeof(info));
    ok(info.AllocationProtect == PAGE_READWRITE, "Unexpected protection %#lx.\n", info.AllocationProtect);
    ok(info.State == MEM_COMMIT, "Unexpected state %#lx.\n", info.State);
    ok(info.Type == MEM_MAPPED, "Unexpected type %#lx.\n", info.Type);
    ok(info.RegionSize == size, "Unexpected size.\n");

    memset(&info, 0, sizeof(info));
    VirtualQuery(placeholder2, &info, sizeof(info));
    ok(info.AllocationProtect == PAGE_READWRITE, "Unexpected protection %#lx.\n", info.AllocationProtect);
    ok(info.State == MEM_COMMIT, "Unexpected state %#lx.\n", info.State);
    ok(info.Type == MEM_MAPPED, "Unexpected type %#lx.\n", info.Type);
    ok(info.RegionSize == size, "Unexpected size.\n");

    CloseHandle(section);
    UnmapViewOfFile(view1);
    UnmapViewOfFile(view2);

    VirtualFree(placeholder1, 0, MEM_RELEASE);
    VirtualFree(placeholder2, 0, MEM_RELEASE);
}

static void test_OpenFileMappingFromApp(void)
{
    OBJECT_BASIC_INFORMATION info;
    HANDLE file, mapping;
    NTSTATUS status;
    ULONG length;

    if (!pOpenFileMappingFromApp)
    {
        win_skip("OpenFileMappingFromApp is not available.\n");
        return;
    }

    file = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READ, 0, 4090, "foo");
    ok(!!file, "Failed to create a mapping.\n");

    mapping = pOpenFileMappingFromApp(FILE_MAP_READ, FALSE, L"foo");
    ok(!!mapping, "Failed to open a mapping.\n");
    status = pNtQueryObject(mapping, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    ok(info.GrantedAccess == SECTION_MAP_READ, "Unexpected access mask %#x.\n", info.GrantedAccess);
    CloseHandle(mapping);

    mapping = pOpenFileMappingFromApp(FILE_MAP_EXECUTE, FALSE, L"foo");
    ok(!!mapping, "Failed to open a mapping.\n");
    status = pNtQueryObject(mapping, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    todo_wine
    ok(info.GrantedAccess == SECTION_MAP_EXECUTE, "Unexpected access mask %#x.\n", info.GrantedAccess);
    CloseHandle(mapping);

    CloseHandle(file);
}

static void test_CreateFileMappingFromApp(void)
{
    OBJECT_BASIC_INFORMATION info;
    NTSTATUS status;
    ULONG length;
    HANDLE file;

    if (!pCreateFileMappingFromApp)
    {
        win_skip("CreateFileMappingFromApp is not available.\n");
        return;
    }

    file = pCreateFileMappingFromApp(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 1024, L"foo");
    ok(!!file || broken(!file) /* Win8 */, "Failed to create a mapping, error %u.\n", GetLastError());
    if (!file) return;

    status = pNtQueryObject(file, ObjectBasicInformation, &info, sizeof(info), &length);
    ok(!status, "Failed to get object information.\n");
    ok(info.GrantedAccess & SECTION_MAP_EXECUTE, "Unexpected access mask %#x.\n", info.GrantedAccess);

    CloseHandle(file);
}

static void test_MapViewOfFileFromApp(void)
{
    static const char testfile[] = "testfile.xxx";
    HANDLE file, mapping;
    void *ptr;
    BOOL ret;

    if (!pMapViewOfFileFromApp)
    {
        win_skip("MapViewOfFileFromApp() is not supported.\n");
        return;
    }

    SetLastError(0xdeadbeef);
    file = CreateFileA( testfile, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "Failed to create a file, error %u.\n", GetLastError() );
    SetFilePointer( file, 12288, NULL, FILE_BEGIN );
    SetEndOfFile( file );

    SetLastError(0xdeadbeef);
    mapping = CreateFileMappingA( file, NULL, PAGE_READWRITE, 0, 4096, NULL );
    ok( mapping != 0, "Failed to create file mapping, error %u.\n", GetLastError() );

    SetLastError(0xdeadbeef);
    ptr = pMapViewOfFileFromApp( mapping, PAGE_EXECUTE_READWRITE, 0, 4096 );
    ok( ptr != NULL, "Mapping failed, error %u.\n", GetLastError() );
    UnmapViewOfFile( ptr );

    CloseHandle( mapping );
    CloseHandle( file );
    ret = DeleteFileA( testfile );
    ok(ret, "Failed to delete a test file.\n");
}

static void init_funcs(void)
{
    HMODULE hmod = GetModuleHandleA("kernelbase.dll");

#define X(f) { p##f = (void*)GetProcAddress(hmod, #f); }
    X(CompareObjectHandles);
    X(CreateFileMappingFromApp);
    X(MapViewOfFileFromApp);
    X(OpenFileMappingFromApp);

    hmod = GetModuleHandleA("ntdll.dll");

    X(NtQueryObject);
#undef X
}

START_TEST(process)
{
    init_funcs();
    pMapViewOfFile3 = (void *)GetProcAddress(hmod, "MapViewOfFile3");
    pVirtualAlloc2 = (void *)GetProcAddress(hmod, "VirtualAlloc2");

    test_CompareObjectHandles();
    test_OpenFileMappingFromApp();
    test_CreateFileMappingFromApp();
    test_MapViewOfFileFromApp();
    test_MapViewOfFile3();
    test_VirtualAlloc2();
}
