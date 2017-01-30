/*
 * Copyright 2010 Ričardas Barkauskas
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

#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "wlanapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(wlanapi);

DWORD WINAPI WlanEnumInterfaces(HANDLE handle, void *reserved, WLAN_INTERFACE_INFO_LIST **interface_list)
{
    FIXME("(%p, %p, %p) stub\n", handle, reserved, interface_list);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI WlanCloseHandle(HANDLE handle, void *reserved)
{
    FIXME("(%p, %p) stub\n", handle, reserved);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI WlanOpenHandle(DWORD client_version, void *reserved, DWORD *negotiated_version, HANDLE *handle)
{
    FIXME("(%u, %p, %p, %p) stub\n", client_version, reserved, negotiated_version, handle);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI WlanFreeMemory(void *ptr)
{
    TRACE("(%p)\n", ptr);

    HeapFree(GetProcessHeap(), 0, ptr);
}

void *WINAPI WlanAllocateMemory(DWORD size)
{
    void *ret;

    TRACE("(%d)", size);

    if (!size)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    ret = HeapAlloc(GetProcessHeap(), 0, size);
    if (!ret)
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);

    return ret;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *reserved)
{
    TRACE("(0x%p, %u, %p)\n", hinstDLL, reason, reserved);

    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
    }

    return TRUE;
}
