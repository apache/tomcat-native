/*
 * Copyright (c) 1997-1999 The Java Apache Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *
 * 4. The names "Apache JServ", "Apache JServ Servlet Engine" and 
 *    "Java Apache Project" must not be used to endorse or promote products 
 *    derived from this software without prior written permission.
 *
 * 5. Products derived from this software may not be called "Apache JServ"
 *    nor may "Apache" nor "Apache JServ" appear in their names without 
 *    prior written permission of the Java Apache Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Java Apache 
 *    Project for use in the Apache JServ servlet engine project
 *    <http://java.apache.org/>."
 *    
 * THIS SOFTWARE IS PROVIDED BY THE JAVA APACHE PROJECT "AS IS" AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE JAVA APACHE PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Java Apache Group. For more information
 * on the Java Apache Project and the Apache JServ Servlet Engine project,
 * please see <http://java.apache.org/>.
 *
 */

/***************************************************************************
 * Description: NT System service for Jakarta/Tomcat                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_global.h"
#include "jk_util.h"
#include "jk_ajp13.h"
#include "jk_connect.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>

#define AJP12_TAG              ("ajp12")
#define AJP13_TAG              ("ajp13")
#define BASE_REGISTRY_LOCATION ("SYSTEM\\CurrentControlSet\\Services\\")
#define IMAGE_NAME             ("ImagePath")
#define PARAMS_LOCATION        ("Parameters")
#define PRP_LOCATION           ("PropertyFile")

// internal variables
static SERVICE_STATUS          ssStatus;       // current status of the service
static SERVICE_STATUS_HANDLE   sshStatusHandle;
static DWORD                   dwErr = 0;
static char                    szErr[1024] = "";
static HANDLE                  hServerStopEvent = NULL;
static int                     shutdown_port;
static char                    *shutdown_protocol = AJP12_TAG;

struct jk_tomcat_startup_data {
    char *classpath;
    char *tomcat_home;
    char *stdout_file;
    char *stderr_file;
    char *java_bin;
    char *tomcat_class;
    char *server_file;
    char *cmd_line;
    int  shutdown_port;
    char *shutdown_protocol;

    char *extra_path;
};
typedef struct jk_tomcat_startup_data jk_tomcat_startup_data_t;

// internal function prototypes
static void WINAPI service_ctrl(DWORD dwCtrlCode);
static void WINAPI service_main(DWORD dwArgc, 
                                char **lpszArgv);
static void install_service(char *name, 
                            char *prp_file);
static void remove_service(char *name);
static char *GetLastErrorText(char *lpszBuf, DWORD dwSize);
static void AddToMessageLog(char *lpszMsg);
static BOOL ReportStatusToSCMgr(DWORD dwCurrentState,
                                DWORD dwWin32ExitCode,
                                DWORD dwWaitHint);
static void start_jk_service(char *name);
static void stop_jk_service(void);
static int set_registry_values(char *name, 
                               char *prp_file);
static int create_registry_key(const char *tag, 
                               HKEY *key);
static int set_registry_config_parameter(HKEY hkey, 
                                         const char *tag, 
                                         char *value);
static int get_registry_config_parameter(HKEY hkey, 
                                         const char *tag,  
                                         char *b, DWORD sz);
static int start_tomcat(const char *name, 
                        HANDLE *hTomcat);
static void stop_tomcat(short port, 
                        const char *protocol,
                        HANDLE hTomcat);
static int read_startup_data(jk_map_t *init_map, 
                             jk_tomcat_startup_data_t *data, 
                             jk_pool_t *p);


static void usage_message(const char *name)
{
    printf("%s - Usage:\n", name);
    printf("%s -i <service name> <configuration properties file>\n", name);
    printf("\tto install the service\n");
    printf("%s -r <service name>\n", name);    
    printf("\tto remove the service\n");
}

void main(int argc, char **argv)
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err; 
        
    wVersionRequested = MAKEWORD(1, 1); 
    err = WSAStartup(wVersionRequested, &wsaData);
    if(0 != err) {
        fprintf(stderr, "Error connecting to winosck");
        return;
    } 

    if(LOBYTE( wsaData.wVersion ) != 1 || 
       HIBYTE( wsaData.wVersion ) != 1)  {
        fprintf(stderr, 
                "Error winsock version is %d %d \n", 
                LOBYTE( wsaData.wVersion ),HIBYTE( wsaData.wVersion ));
        WSACleanup();
        return; 
    } 

    fprintf(stderr, "Asked (and given) winsock %d.%d \n", 
                    LOBYTE(wsaData.wVersion),
                    HIBYTE(wsaData.wVersion));


    __try {
        if((argc > 2) && ((*argv[1] == '-') || (*argv[1] == '/'))) {
            char *cmd = argv[1];
            cmd++;
            if(0 == stricmp("i", cmd) && (4 == argc)) {
                install_service(argv[2], argv[3]);
                return;
            } else if(0 == stricmp("r", cmd) && (3 == argc)) {
                remove_service(argv[2]);
                return;
            } else if(0 == stricmp("s", cmd) && (3 == argc)) {
                HANDLE hTomcat;
                start_tomcat(argv[2], &hTomcat);
                return;
            }
        } else if(2  == argc) {

            SERVICE_TABLE_ENTRY dispatchTable[] =
            {
                { argv[1], (LPSERVICE_MAIN_FUNCTION)service_main },
                { NULL, NULL }
            };

            if(!StartServiceCtrlDispatcher(dispatchTable)) {
                AddToMessageLog("StartServiceCtrlDispatcher failed.");
            }
            return;
        } 

        usage_message(argv[0]);
        exit(-1);
    } __finally {
        WSACleanup();
    }
}

void WINAPI service_main(DWORD dwArgc, char **lpszArgv)
{
    // register our service control handler:
    //
    //
    sshStatusHandle = RegisterServiceCtrlHandler(lpszArgv[0], service_ctrl);

    if(sshStatusHandle) {

        ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        ssStatus.dwServiceSpecificExitCode = 0;

        // report the status to the service control manager.
        //
        if(ReportStatusToSCMgr(SERVICE_START_PENDING, // service state
                                NO_ERROR,              // exit code
                                3000)) {                 // wait hint    
            start_jk_service(lpszArgv[0]);
        }
    }

    // try to report the stopped status to the service control manager.
    //
    if(sshStatusHandle) {
        ReportStatusToSCMgr(SERVICE_STOPPED,
                            dwErr,
                            0);
    }
}


void WINAPI service_ctrl(DWORD dwCtrlCode)
{
    /*
     * Handle the requested control code.
     */
    switch(dwCtrlCode)
    {
        /*
         * Stop the service.
         */
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            ssStatus.dwCurrentState = SERVICE_STOP_PENDING;
            stop_jk_service();
            break;

        /*
         * Update the service status.
         */
        case SERVICE_CONTROL_INTERROGATE:
            break;

        /*
         * Invalid control code, nothing to do.
         */
        default:
            break;

    }

    ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0);

}

BOOL ReportStatusToSCMgr(DWORD dwCurrentState,
                         DWORD dwWin32ExitCode,
                         DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;
    BOOL fResult = TRUE;

    if(dwCurrentState == SERVICE_START_PENDING) {
        ssStatus.dwControlsAccepted = 0;
    } else {
        ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    ssStatus.dwCurrentState = dwCurrentState;
    ssStatus.dwWin32ExitCode = dwWin32ExitCode;
    ssStatus.dwWaitHint = dwWaitHint;

    if((dwCurrentState == SERVICE_RUNNING) ||
       (dwCurrentState == SERVICE_STOPPED)) {
        ssStatus.dwCheckPoint = 0;
    } else {
        ssStatus.dwCheckPoint = dwCheckPoint++;
    }

    if(!(fResult = SetServiceStatus(sshStatusHandle, &ssStatus))) {
        AddToMessageLog(TEXT("SetServiceStatus"));
    }

    return fResult;
}

void install_service(char *name, 
                     char *rel_prp_file)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    char        szExecPath[2048];
    char        szPropPath[2048];
    char        *dummy;

    if(!GetFullPathName(rel_prp_file, sizeof(szPropPath) - 1, szPropPath, &dummy)) {
        printf("Unable to install %s - %s\n", 
               name, 
               GetLastErrorText(szErr, sizeof(szErr)));
        return;
    }

    if(!jk_file_exists(szPropPath)) {
        printf("Unable to install %s - File [%s] does not exists\n", 
               name, 
               szPropPath);
        return;
    }

    if(GetModuleFileName( NULL, szExecPath, sizeof(szExecPath) - 1) == 0) {
        printf("Unable to install %s - %s\n", 
               name, 
               GetLastErrorText(szErr, sizeof(szErr)));
        return;
    }

    schSCManager = OpenSCManager(NULL,  // machine (NULL == local)
                                 NULL,  // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS);   // access required                       
    if(schSCManager) {
        schService = CreateService(schSCManager, // SCManager database
                                   name,         // name of service
                                   name,         // name to display
                                   SERVICE_ALL_ACCESS, // desired access
                                   SERVICE_WIN32_OWN_PROCESS,  // service type
                                   SERVICE_DEMAND_START,       // start type
                                   SERVICE_ERROR_NORMAL,       // error control type
                                   szExecPath,                 // service's binary
                                   NULL,                       // no load ordering group
                                   NULL,                       // no tag identifier
                                   NULL,                       // dependencies
                                   NULL,                       // LocalSystem account
                                   NULL);                      // no password

        if(schService) {
            printf("The service named %s was created. Now adding registry entries\n", name);
            
            if(set_registry_values(name, szPropPath)) {
                CloseServiceHandle(schService);
            } else {
                printf("CreateService failed setting the private registry - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
                DeleteService(schService);
                CloseServiceHandle(schService);
            }
        } else {
            printf("CreateService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
        }

        CloseServiceHandle(schSCManager);
    } else { 
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
    }
}

void remove_service(char *name)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;

    schSCManager = OpenSCManager(NULL,          // machine (NULL == local)
                                 NULL,          // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS );  // access required
                        
    if(schSCManager) {
        schService = OpenService(schSCManager, name, SERVICE_ALL_ACCESS);

        if(schService) {
            // try to stop the service
            if(ControlService( schService, SERVICE_CONTROL_STOP, &ssStatus )) {
                printf("Stopping %s.", name);
                Sleep(1000);

                while(QueryServiceStatus(schService, &ssStatus )) {
                    if(ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                        printf(".");
                        Sleep(1000);
                    } else {
                        break;
                    }
                }

                if(ssStatus.dwCurrentState == SERVICE_STOPPED) {
                    printf("\n%s stopped.\n", name);
                } else {
                    printf("\n%s failed to stop.\n", name);
                }
            }

            // now remove the service
            if(DeleteService(schService)) {
                printf("%s removed.\n", name);
            } else {
                printf("DeleteService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
            }

            CloseServiceHandle(schService);
        } else {
            printf("OpenService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
        }

        CloseServiceHandle(schSCManager);
    } else {
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
    }
}

static int set_registry_values(char *name, 
                               char *prp_file)
{
    char  tag[1024];
    HKEY  hk;
    int rc;

    strcpy(tag, BASE_REGISTRY_LOCATION);
    strcat(tag, name);
    strcat(tag, "\\");
    strcat(tag, PARAMS_LOCATION);

    rc = create_registry_key(tag, &hk);

    if(rc) {
        rc = set_registry_config_parameter(hk, PRP_LOCATION, prp_file);
        if(!rc) {
            printf("Error: Can not create value [%s] - %s\n", 
                    PRP_LOCATION, 
                    GetLastErrorText(szErr, sizeof(szErr)));                
        }
        RegCloseKey(hk);
    } else {
        printf("Error: Can not create key [%s] - %s\n", 
                tag, 
                GetLastErrorText(szErr, sizeof(szErr)));                
    }

    if(rc) {
        char value[2024];

        rc = JK_FALSE;

        strcpy(tag, BASE_REGISTRY_LOCATION);
        strcat(tag, name);
        
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                         tag,
                                         (DWORD)0,         
                                         KEY_WRITE | KEY_READ,
                                         &hk)) {
            rc = get_registry_config_parameter(hk,
                                               IMAGE_NAME, 
                                               value,
                                               sizeof(value));
            if(rc) {
                strcat(value, " ");
                strcat(value, name);
                rc = set_registry_config_parameter(hk,
                                                   IMAGE_NAME, 
                                                   value);
                if(rc) {
                    printf("Registry values were added\n");
                    printf("If you have already updated wrapper.properties you may start the %s service by executing \"net start %s\" from the command prompt\n",
                           name,
                           name);                    
                }
            }
            RegCloseKey(hk);
        }
        if(!rc) {
            printf("Error: Failed to update the service command line - %s\n", 
                    GetLastErrorText(szErr, sizeof(szErr)));                
        }
    }

    return rc;
}

static void start_jk_service(char *name)
{
    /*
     * report the status to the service control manager.
     */
    if(ReportStatusToSCMgr(SERVICE_START_PENDING, // service state
                           NO_ERROR,              // exit code
                           3000)) {               // wait hint
        
        /* 
         * create the event object. The control handler function signals
         * this event when it receives the "stop" control code.
         */
        hServerStopEvent = CreateEvent(NULL,    // no security attributes
                                       TRUE,    // manual reset event
                                       FALSE,   // not-signalled
                                       NULL);   // no name

        if(hServerStopEvent) {
            if(ReportStatusToSCMgr(SERVICE_START_PENDING, // service state
                                   NO_ERROR,              // exit code
                                   20000)) {              // wait hint
                HANDLE hTomcat = NULL;
                int rc = start_tomcat(name, &hTomcat);

                if(rc && ReportStatusToSCMgr(SERVICE_RUNNING, // service state
                                             NO_ERROR,        // exit code
                                             0)) {            // wait hint       
                    HANDLE waitfor[] = { hServerStopEvent, hTomcat};
                    DWORD dwIndex = WaitForMultipleObjects(2, waitfor, FALSE, INFINITE);

                    switch(dwIndex) {
                    case WAIT_OBJECT_0:
                        /* 
                         * Stop order arrived 
                         */ 
                        ResetEvent(hServerStopEvent);
                        stop_tomcat((short)shutdown_port, shutdown_protocol, hTomcat);
                        break;
                    case (WAIT_OBJECT_0 + 1):
                        /* 
                         * Tomcat died !!!
                         */ 
                        break;
                    default:
                        /* 
                         * some error... 
                         * close the servlet container and exit 
                         */ 
                        stop_tomcat((short)shutdown_port, shutdown_protocol, hTomcat);
                    }
                    CloseHandle(hServerStopEvent);
                    CloseHandle(hTomcat);
                }                
            }
        }
    }

    if(hServerStopEvent) {
        CloseHandle(hServerStopEvent);
    }
}


static void stop_jk_service(void)
{
    if(hServerStopEvent) {
        SetEvent(hServerStopEvent);
    }
}

static void AddToMessageLog(char *lpszMsg)
{   
    char    szMsg[2048];
    HANDLE  hEventSource;
    char *  lpszStrings[2];

    printf("Error: %s\n", lpszMsg);

    dwErr = GetLastError();

    hEventSource = RegisterEventSource(NULL, "Jakrta - Tomcat");

    sprintf(szMsg, "%s error: %d", "Jakrta - Tomcat", dwErr);
    lpszStrings[0] = szMsg;
    lpszStrings[1] = lpszMsg;

    if(hEventSource != NULL) {
        ReportEvent(hEventSource, // handle of event source
            EVENTLOG_ERROR_TYPE,  // event type
            0,                    // event category
            0,                    // event ID
            NULL,                 // current user's SID
            2,                    // strings in lpszStrings
            0,                    // no bytes of raw data
            lpszStrings,          // array of error strings
            NULL);                // no raw data

        DeregisterEventSource(hEventSource);
    }
    
}

//
//  FUNCTION: GetLastErrorText
//
//  PURPOSE: copies error message text to string
//
//  PARAMETERS:
//    lpszBuf - destination buffer
//    dwSize - size of buffer
//
//  RETURN VALUE:
//    destination buffer
//
//  COMMENTS:
//
char *GetLastErrorText( char *lpszBuf, DWORD dwSize )
{
    DWORD dwRet;
    char *lpszTemp = NULL;

    dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
                          NULL,
                          GetLastError(),
                          LANG_NEUTRAL,
                          (char *)&lpszTemp,
                          0,
                          NULL);

    // supplied buffer is not long enough
    if(!dwRet || ((long)dwSize < (long)dwRet+14)) {
        lpszBuf[0] = '\0';
    } else {
        lpszTemp[lstrlen(lpszTemp)-2] = '\0';  //remove cr and newline character
        sprintf(lpszBuf, "%s (0x%x)", lpszTemp, GetLastError());
    }

    if(lpszTemp) {
        LocalFree((HLOCAL) lpszTemp );
    }

    return lpszBuf;
}

static void stop_tomcat(short port, 
                        const char *protocol,
                        HANDLE hTomcat)
{
    struct sockaddr_in in;
    
    if(jk_resolve("localhost", port, &in)) {
        int sd = jk_open_socket(&in, JK_TRUE, NULL);
        if(sd >0) {
            int rc = JK_FALSE;
            if(!strcasecmp(protocol, "ajp13")) {
                jk_pool_t pool;
                jk_msg_buf_t *msg = NULL;
                jk_pool_atom_t buf[TINY_POOL_SIZE];

                jk_open_pool(&pool, buf, sizeof(buf));

                msg = jk_b_new(&pool);
                jk_b_set_buffer_size(msg, 512); 

                rc = ajp13_marshal_shutdown_into_msgb(msg, 
                                                      &pool,
                                                      NULL);
                if(rc) {
                    jk_b_end(msg);
    
                    if(0 > jk_tcp_socket_sendfull(sd, 
                                                  jk_b_get_buff(msg),
                                                  jk_b_get_len(msg))) {
                        rc = JK_FALSE;
                    }
                }                                                    
            } else {
                char b[] = {(char)254, (char)15};
                rc = send(sd, b, 2, 0);
                if(2 == rc) {
                    rc = JK_TRUE;
                }
            }
            jk_close_socket(sd);
            if(JK_TRUE == rc) {
                if(WAIT_OBJECT_0 == WaitForSingleObject(hTomcat, 30*1000)) {
                    return;
                }
            }            
        }
    }

    TerminateProcess(hTomcat, 0);    
}

static int start_tomcat(const char *name, HANDLE *hTomcat)
{
    char  tag[1024];
    HKEY  hk;

    strcpy(tag, BASE_REGISTRY_LOCATION);
    strcat(tag, name);
    strcat(tag, "\\");
    strcat(tag, PARAMS_LOCATION);

    if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                     tag,
                                     (DWORD)0,         
                                     KEY_READ,
                                     &hk)) {
        char prp_file[2048];
        if(get_registry_config_parameter(hk,
                                         PRP_LOCATION, 
                                         prp_file,
                                         sizeof(prp_file))) {
            jk_map_t *init_map;
            
            if(map_alloc(&init_map)) {
                if(map_read_properties(init_map, prp_file)) {
                    jk_tomcat_startup_data_t data;
                    jk_pool_t p;
                    jk_pool_atom_t buf[HUGE_POOL_SIZE];
                    jk_open_pool(&p, buf, sizeof(buf));
            
                    if(read_startup_data(init_map, &data, &p)) {
                        STARTUPINFO startupInfo;
                        PROCESS_INFORMATION processInformation;
                        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

                        if(data.extra_path) {
                            jk_append_libpath(&p, data.extra_path);
                        }

                        memset(&startupInfo, 0, sizeof(startupInfo));
                        startupInfo.cb = sizeof(startupInfo);
                        startupInfo.lpTitle = "Jakarta Tomcat";
                        startupInfo.dwFlags = STARTF_USESTDHANDLES;
                        startupInfo.hStdInput = NULL;
                        startupInfo.hStdOutput = CreateFile(data.stdout_file,
                                                            GENERIC_WRITE,
                                                            FILE_SHARE_READ,
                                                            &sa,
                                                            OPEN_ALWAYS,
                                                            FILE_ATTRIBUTE_NORMAL,
                                                            NULL);
                        SetFilePointer(startupInfo.hStdOutput,
                                       0,
                                       NULL,
                                       FILE_END);
                        startupInfo.hStdError = CreateFile(data.stderr_file,
                                                           GENERIC_WRITE,
                                                           FILE_SHARE_READ,
                                                           &sa,
                                                           OPEN_ALWAYS,
                                                           FILE_ATTRIBUTE_NORMAL,
                                                           NULL);
                        SetFilePointer(startupInfo.hStdError,
                                       0,
                                       NULL,
                                       FILE_END);

                        memset(&processInformation, 0, sizeof(processInformation));
                        
                        printf(data.cmd_line);
                        if(CreateProcess(data.java_bin,
                                        data.cmd_line,
                                        NULL,
                                        NULL,
                                        TRUE,
                                        CREATE_NEW_CONSOLE,
                                        NULL,
                                        data.tomcat_home,
                                        &startupInfo,
                                        &processInformation)){

                            *hTomcat = processInformation.hProcess;
                            CloseHandle(processInformation.hThread);
                            CloseHandle(startupInfo.hStdOutput);
                            CloseHandle(startupInfo.hStdError);
                            shutdown_port = data.shutdown_port;
                            shutdown_protocol = strdup(data.shutdown_protocol);

                            return JK_TRUE;
                        } else {
                            printf("Error: Can not create new process - %s\n", 
                                    GetLastErrorText(szErr, sizeof(szErr)));                
                        }

                    }                    
                }
            }
            map_free(&init_map);
        }
        RegCloseKey(hk);
    } 

    return JK_FALSE;
}

static int create_registry_key(const char *tag,
                               HKEY *key)
{
    LONG  lrc = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                               tag,
			                   0,
			                   NULL,
			                   REG_OPTION_NON_VOLATILE,
			                   KEY_WRITE,
			                   NULL,
			                   key,
			                   NULL);
    if(ERROR_SUCCESS != lrc) {
        return JK_FALSE;        
    }

    return JK_TRUE;
}

static int set_registry_config_parameter(HKEY hkey,
                                         const char *tag, 
                                         char *value)
{       
    LONG  lrc;

    lrc = RegSetValueEx(hkey, 
                        tag,            
			            0,              
			            REG_SZ,  
    			        value, 
                        strlen(value));

    if(ERROR_SUCCESS != lrc) {
        return JK_FALSE;        
    }

    return JK_TRUE;     
}



static int get_registry_config_parameter(HKEY hkey,
                                         const char *tag, 
                                         char *b,
                                         DWORD sz)
{   
    DWORD type = 0;
    LONG  lrc;

    lrc = RegQueryValueEx(hkey,     
                          tag,      
                          (LPDWORD)0,
                          &type,    
                          (LPBYTE)b,
                          &sz); 
    if(ERROR_SUCCESS != lrc) {
        return JK_FALSE;        
    }
    
    b[sz] = '\0';

    return JK_TRUE;     
}

static int read_startup_data(jk_map_t *init_map, 
                             jk_tomcat_startup_data_t *data, 
                             jk_pool_t *p)
{
    
    data->classpath = NULL;
    data->tomcat_home = NULL;
    data->stdout_file = NULL;
    data->stderr_file = NULL;
    data->java_bin = NULL;
    data->extra_path = NULL;
    data->tomcat_class = NULL;
    data->server_file = NULL;

    data->server_file = map_get_string(init_map, 
                                       "wrapper.server_xml", 
                                       NULL);
    if(!data->server_file) {
        return JK_FALSE;
    }

    data->classpath = map_get_string(init_map, 
                                     "wrapper.class_path", 
                                       NULL);
    if(!data->classpath) {
        return JK_FALSE;
    }

    data->tomcat_home = map_get_string(init_map, 
                                       "wrapper.tomcat_home", 
                                       NULL);
    if(!data->tomcat_home) {
        return JK_FALSE;
    }

    data->java_bin = map_get_string(init_map, 
                                    "wrapper.javabin", 
                                    NULL);
    if(!data->java_bin) {
        return JK_FALSE;
    }

    data->tomcat_class = map_get_string(init_map,
                                        "wrapper.startup_class",
                                        "org.apache.tomcat.startup.Tomcat");

    if(NULL == data->tomcat_class) {
        return JK_FALSE;
    }

    data->cmd_line = map_get_string(init_map,
                                    "wrapper.cmd_line",
                                    NULL);
    if(NULL == data->cmd_line) {
        data->cmd_line = (char *)jk_pool_alloc(p, (20 + 
                                                   strlen(data->java_bin) +
                                                   strlen(" -classpath ") +
                                                   strlen(data->classpath) +
                                                   strlen(data->tomcat_class) +
                                                   strlen(" -home ") +
                                                   strlen(data->tomcat_home) +
                                                   strlen(" -config ") +
                                                   strlen(data->server_file)
                                                   ) * sizeof(char));
        if(NULL == data->cmd_line) {
            return JK_FALSE;
        }

        strcpy(data->cmd_line, data->java_bin);
        strcat(data->cmd_line, " -classpath ");
        strcat(data->cmd_line, data->classpath);
        strcat(data->cmd_line, " ");
        strcat(data->cmd_line, data->tomcat_class);
        strcat(data->cmd_line, " -home ");
        strcat(data->cmd_line, data->tomcat_home);
        strcat(data->cmd_line, " -config ");
        strcat(data->cmd_line, data->server_file);
    }

    data->shutdown_port = map_get_int(init_map,
                                      "wrapper.shutdown_port",
                                      8007);

    data->shutdown_protocol = map_get_string(init_map,
                                             "wrapper.shutdown_protocol",
                                             AJP12_TAG);

    data->extra_path = map_get_string(init_map,
                                      "wrapper.ld_path",
                                      NULL);

    data->stdout_file = map_get_string(init_map,
                                       "wrapper.stdout",
                                       NULL);

    if(NULL == data->stdout_file) {
        data->stdout_file = jk_pool_alloc(p, strlen(data->tomcat_home) + 2 + strlen("\\stdout.log"));
        strcpy(data->stdout_file, data->tomcat_home);
        strcat(data->stdout_file, "\\stdout.log");        
    }

    data->stderr_file = map_get_string(init_map,
                                       "wrapper.stderr",
                                       NULL);

    if(NULL == data->stderr_file) {
        data->stderr_file = jk_pool_alloc(p, strlen(data->tomcat_home) + 2 + strlen("\\stderr.log"));
        strcpy(data->stderr_file, data->tomcat_home);
        strcat(data->stderr_file, "\\stderr.log");        
    }

    return JK_TRUE;
}

