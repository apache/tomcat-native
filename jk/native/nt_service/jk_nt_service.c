/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

/***************************************************************************
 * Description: NT System service for Jakarta/Tomcat                       *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 *              Dave Oxley <Dave@JungleMoss.com>                           *
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
static char                    *shutdown_secret = NULL;
static char                    *shutdown_cmd=NULL;

typedef enum ActionEnum
{   acNoAction  = 0,
    acInstall   = 1,
    acRemove    = 2,
    acStartTC   = 3,
    acStopTC    = 4
}   ActionEnum;


struct jk_tomcat_startup_data {
    char *cmd_line; /* Start command line */
    char *stdout_file;
    char *stderr_file;
    char *extra_path;
    char *tomcat_home;
    char *java_bin;

    char *shutdown_protocol;
    /* for cmd */
    char *stop_cmd;
    /* For ajp13/ajp12/catalina */
    int  shutdown_port;
    char *shutdown_secret;

    /* Optional/not needed */
    char *classpath;
    char *tomcat_class;
    char *server_file;
};

typedef struct jk_tomcat_startup_data jk_tomcat_startup_data_t;

// internal function prototypes
static void WINAPI service_ctrl(DWORD dwCtrlCode);
static void WINAPI service_main(DWORD dwArgc, 
                                char **lpszArgv);
static void install_service(char *name,
                            char *dname,
                            char *user, 
                            char *password, 
                            char *deps, 
                            BOOL bAutomatic, 
                            char *rel_prp_file);
static void remove_service(char *name);
static void start_service(char *name,
                          char *machine);
static void stop_service(char *name,
                         char *machine);
static char *GetLastErrorText(char *lpszBuf, DWORD dwSize);
static void AddToMessageLog(char *lpszMsg);
static BOOL ReportStatusToSCMgr(DWORD dwCurrentState,
                                DWORD dwWin32ExitCode,
                                DWORD dwWaitHint);
static void start_jk_service(char *name);
static void stop_jk_service(void);
static int set_registry_values(SC_HANDLE   schService, char *name, 
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
static void stop_tomcat(char *name,
                        short port, 
                        const char *protocol,
                        char *secret,
                        HANDLE hTomcat);
static int read_startup_data(jk_map_t *init_map, 
                             jk_tomcat_startup_data_t *data, 
                             jk_pool_t *p);
static int exec_cmd(const char *name, HANDLE *hTomcat, char *cmdLine);

static void usage_message(const char *name)
{
    printf("%s - Usage:\n\n", name);
    printf("To install the service:\n");
    printf("%s -i <service name> {optional params} <config properties file>\n", name);
    printf("    Optional parameters\n");
    printf("        -u <user name> - In the form DomainName\\UserName (.\\UserName for local)\n");
    printf("        -n <service display name> - In quotes if contains non-lphanumeric chars\n");
    printf("        -p <user password>\n");
    printf("        -a - Set startup type to automatic\n");
    printf("        -d <service dependency> - Can be entered multiple times\n\n");
    printf("To remove the service:\n");
    printf("%s -r <service name>\n\n", name);
    printf("To start the service:\n");
    printf("%s -s <service name> {optional params}\n", name);
    printf("    Optional parameters\n");
    printf("        -m <machine>\n\n");
    printf("To stop the service:\n");
    printf("%s -t <service name> {optional params}\n", name);
    printf("    Optional parameters\n");
    printf("        -m <machine>\n");
}

void main(int argc, char **argv)
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int i;
    int err;
    int count;
    int iAction = acNoAction;
    char *pServiceDisplayName = NULL;
    char *pServiceName = NULL;
    char *pUserName = NULL;
    char *pPassword = NULL;
    char *pMachine = NULL;
    BOOL bAutomatic = FALSE;
    char strDependancy[256] = "";

    memset(strDependancy, 0, 255);

    wVersionRequested = MAKEWORD(1, 1); 
    err = WSAStartup(wVersionRequested, &wsaData);
    if(0 != err) {
        fprintf(stderr, "Error connecting to winsock");
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
        if(argc > 2) {
            count=0;
            for (i=1;i<argc;i++) {
                if ((*argv[i] == '-') || (*argv[i] == '/')) {
                    char *cmd = argv[i];
                    cmd++;
                    if(0 == stricmp("i", cmd)) {
                        iAction = acInstall;
                        pServiceName = argv[i+1];
                    } else if(0 == stricmp("r", cmd)) {
                        iAction = acRemove;
                        pServiceName = argv[i+1];
                    } else if(0 == stricmp("s", cmd)) {
                        iAction = acStartTC;
                        pServiceName = argv[i+1];
                    } else if(0 == stricmp("t", cmd)) {
                        iAction = acStopTC;
                        pServiceName = argv[i+1];
                    } else if(0 == stricmp("u", cmd)) {
                        pUserName = argv[i+1];
                    } else if(0 == stricmp("p", cmd)) {
                        pPassword = argv[i+1];
                    } else if(0 == stricmp("m", cmd)) {
                        pMachine = argv[i+1];
                    } else if(0 == stricmp("a", cmd)) {
                        bAutomatic = TRUE;
                    } else if(0 == stricmp("n", cmd)) {
                        pServiceDisplayName = argv[i+1];
                    } else if(0 == stricmp("d", cmd)) {
                        memcpy(strDependancy+count, argv[i+1], strlen(argv[i+1]));
                        count+= strlen(argv[i+1])+1;
                    }
                }
            }
            switch (iAction) {
            case acInstall:
                if (pServiceDisplayName == NULL) {
                    pServiceDisplayName = pServiceName;
                }
                install_service(pServiceName, pServiceDisplayName, pUserName,
                                pPassword, strDependancy, bAutomatic, argv[i-1]);
                return;
            case acRemove:
                remove_service(pServiceName);
                return;
            case acStartTC:
                start_service(pServiceName, pMachine);
                return;
            case acStopTC:
                stop_service(pServiceName, pMachine);
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

typedef WINADVAPI BOOL (WINAPI * pfnChangeServiceConfig2_t)
                       (SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo);


void install_service(char *name, 
                     char *dname, 
                     char *user, 
                     char *password, 
                     char *deps, 
                     BOOL bAutomatic,
                     char *rel_prp_file)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    char        szExecPath[2048];
    char        szPropPath[2048];
    char        szTrueName[256];
    char        *dummy;
    char        *src, *dst;

    dst = szTrueName;
    for (src = name; *src; ++src) {
        if (dst >= szTrueName + sizeof(szTrueName) - 1) {
            break;
        }
        if (!isspace(*src) && *src != '/' && *src != '\\') {
            *(dst++) = *src;
        }
    }
    *dst = '\0';

    if (0 == stricmp("", deps))
        deps = NULL;

    /* XXX strcat( deps, "Tcpip\0Afd\0" ); */
    
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

    szExecPath[0] = '\"';
    if(GetModuleFileName( NULL, szExecPath + 1, sizeof(szExecPath) - 2) == 0) {
        /* Was: if(GetModuleFileName( NULL, szExecPath, sizeof(szExecPath) - 1) == 0) { */
        printf("Unable to install %s - %s\n", 
               name, 
               GetLastErrorText(szErr, sizeof(szErr)));
        return;
    }
    strcat(szExecPath, "\" ");
    strcat(szExecPath, szTrueName);


    schSCManager = OpenSCManager(NULL,     // machine (NULL == local)
                                 NULL,     // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS);   // access required                       
    if(schSCManager) {

        schService = CreateService(schSCManager, // SCManager database
                                   szTrueName,   // name of service
                                   dname,         // name to display
                                   SERVICE_ALL_ACCESS, // desired access
                                   SERVICE_WIN32_OWN_PROCESS,  // service type
                                   bAutomatic ? SERVICE_AUTO_START : SERVICE_DEMAND_START,       // start type
                                   SERVICE_ERROR_NORMAL,       // error control type
                                   szExecPath,                 // service's binary
                                   NULL,                       // no load ordering group
                                   NULL,                       // no tag identifier
                                   deps,                       // dependencies
                                   user,                       // account
                                   password);                  // password

        if(schService) {
            
            printf("The service named %s was created. Now adding registry entries\n", name);
            
            if(set_registry_values(schService, szTrueName, szPropPath)) {
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
    char        szNameBuff[256];
    DWORD       lenNameBuff = 256;
    char        *szTrueName = name;

    schSCManager = OpenSCManager(NULL,          // machine (NULL == local)
                                 NULL,          // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS );  // access required
                        
    if(schSCManager) {
        if (GetServiceKeyName(schSCManager, name, szNameBuff, &lenNameBuff)) {
            szTrueName = szNameBuff;
        }
        schService = OpenService(schSCManager, szTrueName, SERVICE_ALL_ACCESS);

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

void start_service(char *name, char *machine)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;

    schSCManager = OpenSCManager(machine,  // machine (NULL == local)
                                 NULL,     // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS);   // access required                       

    if(schSCManager) {
        schService = OpenService(schSCManager, name, SERVICE_ALL_ACCESS);
 
       if(schService) {
            // try to start the service
            if(StartService(schService, 0, NULL)) {
                printf("Starting %s.", name);
                Sleep(1000);

                while(QueryServiceStatus(schService, &ssStatus )) {
                    if(ssStatus.dwCurrentState == SERVICE_START_PENDING) {
                        printf(".");
                        Sleep(1000);
                    } else {
                        break;
                    }
                }

                if(ssStatus.dwCurrentState == SERVICE_RUNNING) {
                    printf("\n%s started.\n", name);
                } else {
                    printf("\n%s failed to start.\n", name);
                }
            }
            else
                printf("StartService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));

            CloseServiceHandle(schService);
        } else {
            printf("OpenService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
        }

        CloseServiceHandle(schSCManager);
    } else {
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
    }
}

void stop_service(char *name, char *machine)
{
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;

    schSCManager = OpenSCManager(machine,  // machine (NULL == local)
                                 NULL,     // database (NULL == default)
                                 SC_MANAGER_ALL_ACCESS);   // access required                       

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
            else
                printf("StopService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));

            CloseServiceHandle(schService);
        } else {
            printf("OpenService failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
        }

        CloseServiceHandle(schSCManager);
    } else {
        printf("OpenSCManager failed - %s\n", GetLastErrorText(szErr, sizeof(szErr)));
    }
}

static int set_registry_values(SC_HANDLE   schService, char *name, 
                               char *prp_file)
{
    char  tag[1024];
    HKEY  hk;
    int rc;
    /* Api based */
    HANDLE hAdvApi32;
    char *szDescription = "Jakarta Tomcat Server";
    pfnChangeServiceConfig2_t pfnChangeServiceConfig2;
            
    if((hAdvApi32 = GetModuleHandle("advapi32.dll"))
       && ((pfnChangeServiceConfig2 = (pfnChangeServiceConfig2_t)
            GetProcAddress(hAdvApi32, "ChangeServiceConfig2A")))) {
        (void) pfnChangeServiceConfig2(schService, // Service Handle
                                       1,          // SERVICE_CONFIG_DESCRIPTION
                                       &szDescription);
    } else {
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
                    printf("If you have already updated wrapper.properties you may start the %s"
                           "service by executing \"jk_nt_service -s %s\" from the command prompt\n",
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
                char   szNameBuff[256];
                DWORD  lenNameBuff = 256;
                char   *szTrueName = name;
                SC_HANDLE   schSCManager;
                int rc;

                schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS );
                if(schSCManager) {
                    if (GetServiceKeyName(schSCManager, name, szNameBuff, &lenNameBuff)) {
                        szTrueName = szNameBuff;
                    }
                    CloseServiceHandle(schSCManager);
                }

                rc = start_tomcat(szTrueName, &hTomcat);

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
                        stop_tomcat(name, (short)shutdown_port, shutdown_protocol,
                                    shutdown_secret, hTomcat);
                        break;
                    case (WAIT_OBJECT_0 + 1):
                        /* 
                         * Tomcat died !!!
                         */ 
                        CloseHandle(hServerStopEvent);
                        CloseHandle(hTomcat);
                        exit(0); // exit ungracefully so
                                 // Service Control Manager 
                                 // will attempt a restart.
                        break;
                    default:
                        /* 
                         * some error... 
                         * close the servlet container and exit 
                         */ 
                        stop_tomcat(name, (short)shutdown_port, shutdown_protocol,
                                    shutdown_secret, hTomcat);
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

static void stop_tomcat(char *name,
                        short port, 
                        const char *protocol,
                        char *secret,
                        HANDLE hTomcat)
{
    struct sockaddr_in in;

    if(strcasecmp(protocol, "cmd") == 0 ) {
        exec_cmd( name, hTomcat, shutdown_cmd);
        /* XXX sleep 100 */
        TerminateProcess(hTomcat, 0);
        return;
    } 
    
    if(jk_resolve("localhost", port, &in)) {
        int sd = jk_open_socket(&in, JK_TRUE, 0, NULL);
        if(sd >0) {
            int rc = JK_FALSE;

            if(strcasecmp(protocol, "catalina") == 0 ) {
                char len;
                
                if( secret==NULL )
                    secret="SHUTDOWN";
                len=strlen( secret );
                
                rc = send(sd, secret, len , 0);
                if(len == rc) {
                    rc = JK_TRUE;
                }
            } else if(!strcasecmp(protocol, "ajp13")) {
                jk_pool_t pool;
                jk_msg_buf_t *msg = NULL;
                jk_pool_atom_t buf[TINY_POOL_SIZE];

                jk_open_pool(&pool, buf, sizeof(buf));

                msg = jk_b_new(&pool);
                jk_b_set_buffer_size(msg, 512); 

                rc = ajp13_marshal_shutdown_into_msgb(msg, 
                                                      &pool,
                                                      NULL);
                if( secret!=NULL ) {
                    /** will work with old clients, as well as new
                     */
                    rc = jk_b_append_string(msg, secret);
                }
                if(rc) {
                    jk_b_end(msg, AJP13_PROTO);
    
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

static int exec_cmd(const char *name, HANDLE *hTomcat, char *cmdLine)
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
                        
						if( cmdLine==NULL ) 
							cmdLine=data.cmd_line;

                        printf(cmdLine);
                        if(CreateProcess(data.java_bin,
                                        cmdLine,
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
                            shutdown_secret = data.shutdown_secret;
                            shutdown_protocol = strdup(data.shutdown_protocol);
							shutdown_cmd = strdup(data.stop_cmd);

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

static int start_tomcat(const char *name, HANDLE *hTomcat)
{
    return exec_cmd( name, hTomcat, NULL );
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

    /* All this is wrong - you just need to configure cmd_line */
    /* Optional - you may have cmd_line defined */
    data->server_file = map_get_string(init_map, 
                                       "wrapper.server_xml", 
                                       NULL);
    data->classpath = map_get_string(init_map, 
                                     "wrapper.class_path", 
                                       NULL);
    data->tomcat_home = map_get_string(init_map, 
                                       "wrapper.tomcat_home", 
                                       NULL);
    data->java_bin = map_get_string(init_map, 
                                    "wrapper.javabin", 
                                    NULL);
    data->tomcat_class = map_get_string(init_map,
                                        "wrapper.startup_class",
                                        "org.apache.tomcat.startup.Tomcat");

    data->cmd_line = map_get_string(init_map,
                                    "wrapper.cmd_line",
                                    NULL);

    data->stop_cmd = map_get_string(init_map,
                                    "wrapper.stop_cmd",
                                    NULL);

    if(NULL == data->cmd_line &&
       ( (NULL == data->tomcat_class) ||
         (NULL == data->server_file) ||
         (NULL == data->tomcat_home) ||
         (NULL == data->java_bin) )) {
       return JK_FALSE;
    }

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

    data->shutdown_secret = map_get_string(init_map,
                                           "wrapper.shutdown_secret", NULL );
    
    data->shutdown_protocol = map_get_string(init_map,
                                             "wrapper.shutdown_protocol",
                                             AJP12_TAG);

    data->extra_path = map_get_string(init_map,
                                      "wrapper.ld_path",
                                      NULL);

    data->stdout_file = map_get_string(init_map,
                                       "wrapper.stdout",
                                       NULL);

    if(NULL == data->stdout_file && NULL == data->tomcat_home ) {
        return JK_FALSE;
    }
    
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

