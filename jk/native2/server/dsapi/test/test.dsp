# Microsoft Developer Studio Project File - Name="test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=test - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "test.mak" CFG="test - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "test - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "test - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "test - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".." /I "..\..\..\include" /I "$(JAVA_HOME)\include" /I "$(JAVA_HOME)\include\win32" /I "..\..\..\..\..\..\apr\include" /I "..\..\..\..\..\..\apr-util\include" /I "..\..\..\..\..\..\pcre\include" /I "$(NOTESAPI)\include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_USRDLL" /D "HAVE_JNI" /D "HAS_APR" /D "HAS_PCRE" /D "NT" /D "TESTING" /U "NOUSER" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libapr.lib libaprutil.lib ws2_32.lib wsock32.lib pcre.lib pcreposix.lib notes.lib /nologo /subsystem:console /machine:I386 /libpath:"..\..\..\..\..\..\apr\Release" /libpath:"..\..\..\..\..\..\pcre\lib" /libpath:"..\..\..\..\..\..\apr-util\Release" /libpath:"$(NOTESAPI)\lib\mswin32"

!ELSEIF  "$(CFG)" == "test - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".." /I "..\..\..\include" /I "$(JAVA_HOME)\include" /I "$(JAVA_HOME)\include\win32" /I "..\..\..\..\..\..\apr\include" /I "..\..\..\..\..\..\apr-util\include" /I "..\..\..\..\..\..\pcre\include" /I "$(NOTESAPI)\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_USRDLL" /D "HAVE_JNI" /D "HAS_APR" /D "HAS_PCRE" /D "NT" /D "TESTING" /D "NO_CAPI" /U "NOUSER" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libapr.lib libaprutil.lib ws2_32.lib wsock32.lib pcre.lib pcreposix.lib notes.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"..\..\..\..\..\..\apr\Release" /libpath:"..\..\..\..\..\..\pcre\lib" /libpath:"..\..\..\..\..\..\apr-util\Release" /libpath:"$(NOTESAPI)\lib\mswin32"

!ENDIF 

# Begin Target

# Name "test - Win32 Release"
# Name "test - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "JK2 Common Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\common\jk_channel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_channel_apr_socket.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_channel_jni.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_channel_un.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_config.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_config_file.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_endpoint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_env.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_handler_logon.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_handler_response.c
# End Source File
# Begin Source File

SOURCE=..\..\..\jni\jk_jni_aprImpl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_logger_file.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_logger_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_map.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_md5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_msg_ajp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_mutex.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_mutex_proc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_mutex_thread.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_nwmain.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_objCache.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_pool_apr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_registry.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_requtil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_shm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_signal.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_uriEnv.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_uriMap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_user.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_vm_default.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_worker_ajp13.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_worker_jni.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_worker_lb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_worker_run.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_worker_status.c
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_workerEnv.c
# End Source File
# End Group
# Begin Group "DSAPI Source"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\jk_dsapi_plugin.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\printf_logger.c
# End Source File
# Begin Source File

SOURCE=.\test.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "JK2 Common Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\include\jk_channel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_config.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_endpoint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_env.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_global.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_handler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_logger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_logger_win32_message.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_map.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_md5.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_msg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_objCache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_pool.h
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_registry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_requtil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_service.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_shm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_uriEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_uriMap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_vm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_worker.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\jk_workerEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\jni\org_apache_jk_apr_AprImpl.h
# End Source File
# End Group
# Begin Group "DSAPI Headers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\config.h
# End Source File
# Begin Source File

SOURCE=..\dsapifilter.h
# End Source File
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\common\jk_logger_win32_message.mc
# End Source File
# Begin Source File

SOURCE=..\..\..\common\jk_logger_win32_message.rc
# End Source File
# End Group
# End Target
# End Project
