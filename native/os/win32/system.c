/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>

#include "apr.h"
#include "apr_pools.h"
#include "apr_poll.h"
#include "apr_network_io.h"
#include "apr_arch_misc.h" /* for apr_os_level */
#include "apr_arch_atime.h"  /* for FileTimeToAprTime */

#include "tcn.h"
#include "ssl_private.h"

#pragma warning(push)
#pragma warning(disable : 4201)
#include <psapi.h>
#pragma warning(pop)


static CRITICAL_SECTION dll_critical_section;   /* dll's critical section */
static HINSTANCE        dll_instance = NULL;
static SYSTEM_INFO      dll_system_info;
static HANDLE           h_kernel = NULL;
static HANDLE           h_ntdll  = NULL;
static char             dll_file_name[MAX_PATH];

typedef BOOL (WINAPI *pfnGetSystemTimes)(LPFILETIME, LPFILETIME, LPFILETIME);
static pfnGetSystemTimes fnGetSystemTimes = NULL;

BOOL
WINAPI
DllMain(
    HINSTANCE instance,
    DWORD reason,
    LPVOID reserved)
{

    switch (reason) {
        /** The DLL is loading due to process
         *  initialization or a call to LoadLibrary.
         */
        case DLL_PROCESS_ATTACH:
            InitializeCriticalSection(&dll_critical_section);
            dll_instance = instance;
            GetSystemInfo(&dll_system_info);
            if ((h_kernel = LoadLibrary("kernel32.dll")) != NULL)
                fnGetSystemTimes = (pfnGetSystemTimes)GetProcAddress(h_kernel,
                                                            "GetSystemTimes");
            if (fnGetSystemTimes == NULL) {
                FreeLibrary(h_kernel);
                h_kernel = NULL;
#if (_WIN32_WINNT < 0x0501)
                if ((h_ntdll = LoadLibrary("ntdll.dll")) != NULL)
                    fnNtQuerySystemInformation =
                        (pfnNtQuerySystemInformation)GetProcAddress(h_ntdll,
                                                "NtQuerySystemInformation");

                if (fnNtQuerySystemInformation == NULL) {
                    FreeLibrary(h_ntdll);
                    h_ntdll = NULL;
                }
#endif
            }
            GetModuleFileName(instance, dll_file_name, sizeof(dll_file_name));
            break;
        /** The attached process creates a new thread.
         */
        case DLL_THREAD_ATTACH:
            break;

        /** The thread of the attached process terminates.
         */
        case DLL_THREAD_DETACH:
            ERR_remove_thread_state(NULL);
            break;

        /** DLL unload due to process termination
         *  or FreeLibrary.
         */
        case DLL_PROCESS_DETACH:
            if (h_kernel)
                FreeLibrary(h_kernel);
            if (h_ntdll)
                FreeLibrary(h_ntdll);
            DeleteCriticalSection(&dll_critical_section);
            break;

        default:
            break;
    }

    return TRUE;
    UNREFERENCED_PARAMETER(reserved);
}


static DWORD WINAPI password_thread(void *data)
{
    tcn_pass_cb_t *cb = (tcn_pass_cb_t *)data;
    MSG     msg;
    HWINSTA hwss;
    HWINSTA hwsu;
    HDESK   hwds;
    HDESK   hwdu;
    HWND    hwnd;

    /* Ensure connection to service window station and desktop, and
     * save their handles.
     */
    GetDesktopWindow();
    hwss = GetProcessWindowStation();
    hwds = GetThreadDesktop(GetCurrentThreadId());

    /* Impersonate the client and connect to the User's
     * window station and desktop.
     */
    hwsu = OpenWindowStation("WinSta0", FALSE, MAXIMUM_ALLOWED);
    if (hwsu == NULL) {
        ExitThread(1);
        return 1;
    }
    SetProcessWindowStation(hwsu);
    hwdu = OpenDesktop("Default", 0, FALSE, MAXIMUM_ALLOWED);
    if (hwdu == NULL) {
        SetProcessWindowStation(hwss);
        CloseWindowStation(hwsu);
        ExitThread(1);
        return 1;
    }
    SetThreadDesktop(hwdu);

    hwnd = CreateDialog(dll_instance, MAKEINTRESOURCE(1001), NULL, NULL);
    if (hwnd != NULL)
        ShowWindow(hwnd, SW_SHOW);
    else  {
        ExitThread(1);
        return 1;
    }
    while (1) {
        if (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_KEYUP) {
                int nVirtKey = (int)msg.wParam;
                if (nVirtKey == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                    break;
                }
                else if (nVirtKey == VK_RETURN) {
                    HWND he = GetDlgItem(hwnd, 1002);
                    if (he) {
                        int n = GetWindowText(he, cb->password, SSL_MAX_PASSWORD_LEN - 1);
                        cb->password[n] = '\0';
                    }
                    DestroyWindow(hwnd);
                    break;
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
            Sleep(100);
    }
    /* Restore window station and desktop.
     */
    SetThreadDesktop(hwds);
    SetProcessWindowStation(hwss);
    CloseDesktop(hwdu);
    CloseWindowStation(hwsu);

    ExitThread(0);
    return 0;
}

int WIN32_SSL_password_prompt(tcn_pass_cb_t *data)
{
    DWORD id;
    HANDLE thread;
    /* TODO: See how to display this from service mode */
    thread = CreateThread(NULL, 0,
                password_thread, data,
                0, &id);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return (int)strlen(data->password);
}
