# Microsoft Developer Studio Generated NMAKE File, Based on isapi.dsp
!IF "$(CFG)" == ""
CFG=isapi - Win32 Debug
!MESSAGE No configuration specified. Defaulting to isapi - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "isapi - Win32 Release" && "$(CFG)" != "isapi - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "isapi.mak" CFG="isapi - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "isapi - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "isapi - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "isapi - Win32 Release"

OUTDIR=.\isapi_release
INTDIR=.\isapi_release
# Begin Custom Macros
OutDir=.\isapi_release
# End Custom Macros

ALL : "$(OUTDIR)\isapi_redirect.dll"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_isapi_plugin.obj"
	-@erase "$(INTDIR)\jk_jni_worker.obj"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_nwmain.obj"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\isapi_redirect.dll"
	-@erase "$(OUTDIR)\isapi_redirect.exp"
	-@erase "$(OUTDIR)\isapi_redirect.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\common" /I "$(JAVA_HOME)\include" /I "$(JAVA_HOME)\include\win32" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ISAPI_EXPORTS" /Fp"$(INTDIR)\isapi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\isapi.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib advapi32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\isapi_redirect.pdb" /machine:I386 /def:".\isapi.def" /out:"$(OUTDIR)\isapi_redirect.dll" /implib:"$(OUTDIR)\isapi_redirect.lib" 
DEF_FILE= \
	".\isapi.def"
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_isapi_plugin.obj" \
	"$(INTDIR)\jk_jni_worker.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_nwmain.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj"

"$(OUTDIR)\isapi_redirect.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"

OUTDIR=.\isapi_debug
INTDIR=.\isapi_debug
# Begin Custom Macros
OutDir=.\isapi_debug
# End Custom Macros

ALL : "$(OUTDIR)\isapi_redirect.dll" "$(OUTDIR)\isapi.bsc"


CLEAN :
	-@erase "$(INTDIR)\jk_ajp12_worker.obj"
	-@erase "$(INTDIR)\jk_ajp12_worker.sbr"
	-@erase "$(INTDIR)\jk_ajp13.obj"
	-@erase "$(INTDIR)\jk_ajp13.sbr"
	-@erase "$(INTDIR)\jk_ajp13_worker.obj"
	-@erase "$(INTDIR)\jk_ajp13_worker.sbr"
	-@erase "$(INTDIR)\jk_ajp14.obj"
	-@erase "$(INTDIR)\jk_ajp14.sbr"
	-@erase "$(INTDIR)\jk_ajp14_worker.obj"
	-@erase "$(INTDIR)\jk_ajp14_worker.sbr"
	-@erase "$(INTDIR)\jk_ajp_common.obj"
	-@erase "$(INTDIR)\jk_ajp_common.sbr"
	-@erase "$(INTDIR)\jk_connect.obj"
	-@erase "$(INTDIR)\jk_connect.sbr"
	-@erase "$(INTDIR)\jk_context.obj"
	-@erase "$(INTDIR)\jk_context.sbr"
	-@erase "$(INTDIR)\jk_isapi_plugin.obj"
	-@erase "$(INTDIR)\jk_isapi_plugin.sbr"
	-@erase "$(INTDIR)\jk_jni_worker.obj"
	-@erase "$(INTDIR)\jk_jni_worker.sbr"
	-@erase "$(INTDIR)\jk_lb_worker.obj"
	-@erase "$(INTDIR)\jk_lb_worker.sbr"
	-@erase "$(INTDIR)\jk_map.obj"
	-@erase "$(INTDIR)\jk_map.sbr"
	-@erase "$(INTDIR)\jk_md5.obj"
	-@erase "$(INTDIR)\jk_md5.sbr"
	-@erase "$(INTDIR)\jk_msg_buff.obj"
	-@erase "$(INTDIR)\jk_msg_buff.sbr"
	-@erase "$(INTDIR)\jk_nwmain.obj"
	-@erase "$(INTDIR)\jk_nwmain.sbr"
	-@erase "$(INTDIR)\jk_pool.obj"
	-@erase "$(INTDIR)\jk_pool.sbr"
	-@erase "$(INTDIR)\jk_sockbuf.obj"
	-@erase "$(INTDIR)\jk_sockbuf.sbr"
	-@erase "$(INTDIR)\jk_uri_worker_map.obj"
	-@erase "$(INTDIR)\jk_uri_worker_map.sbr"
	-@erase "$(INTDIR)\jk_util.obj"
	-@erase "$(INTDIR)\jk_util.sbr"
	-@erase "$(INTDIR)\jk_worker.obj"
	-@erase "$(INTDIR)\jk_worker.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\isapi.bsc"
	-@erase "$(OUTDIR)\isapi_redirect.dll"
	-@erase "$(OUTDIR)\isapi_redirect.exp"
	-@erase "$(OUTDIR)\isapi_redirect.ilk"
	-@erase "$(OUTDIR)\isapi_redirect.lib"
	-@erase "$(OUTDIR)\isapi_redirect.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\common" /I "$(JAVA_HOME)\include" /I "$(JAVA_HOME)\include\win32" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ISAPI_EXPORTS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\isapi.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\isapi.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\jk_isapi_plugin.sbr" \
	"$(INTDIR)\jk_worker.sbr" \
	"$(INTDIR)\jk_ajp12_worker.sbr" \
	"$(INTDIR)\jk_ajp13.sbr" \
	"$(INTDIR)\jk_ajp13_worker.sbr" \
	"$(INTDIR)\jk_ajp14.sbr" \
	"$(INTDIR)\jk_ajp14_worker.sbr" \
	"$(INTDIR)\jk_connect.sbr" \
	"$(INTDIR)\jk_context.sbr" \
	"$(INTDIR)\jk_jni_worker.sbr" \
	"$(INTDIR)\jk_lb_worker.sbr" \
	"$(INTDIR)\jk_map.sbr" \
	"$(INTDIR)\jk_md5.sbr" \
	"$(INTDIR)\jk_msg_buff.sbr" \
	"$(INTDIR)\jk_nwmain.sbr" \
	"$(INTDIR)\jk_pool.sbr" \
	"$(INTDIR)\jk_sockbuf.sbr" \
	"$(INTDIR)\jk_uri_worker_map.sbr" \
	"$(INTDIR)\jk_util.sbr" \
	"$(INTDIR)\jk_ajp_common.sbr"

"$(OUTDIR)\isapi.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=wsock32.lib advapi32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\isapi_redirect.pdb" /debug /machine:I386 /def:".\isapi.def" /out:"$(OUTDIR)\isapi_redirect.dll" /implib:"$(OUTDIR)\isapi_redirect.lib" /pdbtype:sept 
DEF_FILE= \
	".\isapi.def"
LINK32_OBJS= \
	"$(INTDIR)\jk_ajp12_worker.obj" \
	"$(INTDIR)\jk_ajp13.obj" \
	"$(INTDIR)\jk_ajp13_worker.obj" \
	"$(INTDIR)\jk_ajp14.obj" \
	"$(INTDIR)\jk_ajp14_worker.obj" \
	"$(INTDIR)\jk_ajp_common.obj" \
	"$(INTDIR)\jk_connect.obj" \
	"$(INTDIR)\jk_context.obj" \
	"$(INTDIR)\jk_isapi_plugin.obj" \
	"$(INTDIR)\jk_jni_worker.obj" \
	"$(INTDIR)\jk_lb_worker.obj" \
	"$(INTDIR)\jk_map.obj" \
	"$(INTDIR)\jk_md5.obj" \
	"$(INTDIR)\jk_msg_buff.obj" \
	"$(INTDIR)\jk_nwmain.obj" \
	"$(INTDIR)\jk_pool.obj" \
	"$(INTDIR)\jk_sockbuf.obj" \
	"$(INTDIR)\jk_uri_worker_map.obj" \
	"$(INTDIR)\jk_util.obj" \
	"$(INTDIR)\jk_worker.obj"

"$(OUTDIR)\isapi_redirect.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("isapi.dep")
!INCLUDE "isapi.dep"
!ELSE 
!MESSAGE Warning: cannot find "isapi.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "isapi - Win32 Release" || "$(CFG)" == "isapi - Win32 Debug"
SOURCE=..\common\jk_ajp12_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp12_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp12_worker.obj"	"$(INTDIR)\jk_ajp12_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_ajp13.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp13.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp13.obj"	"$(INTDIR)\jk_ajp13.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_ajp13_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp13_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp13_worker.obj"	"$(INTDIR)\jk_ajp13_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_ajp14.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp14.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp14.obj"	"$(INTDIR)\jk_ajp14.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_ajp14_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp14_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp14_worker.obj"	"$(INTDIR)\jk_ajp14_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_ajp_common.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_ajp_common.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_ajp_common.obj"	"$(INTDIR)\jk_ajp_common.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_connect.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_connect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_connect.obj"	"$(INTDIR)\jk_connect.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_context.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_context.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_context.obj"	"$(INTDIR)\jk_context.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\jk_isapi_plugin.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_isapi_plugin.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_isapi_plugin.obj"	"$(INTDIR)\jk_isapi_plugin.sbr" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=..\common\jk_jni_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_jni_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_jni_worker.obj"	"$(INTDIR)\jk_jni_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_lb_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_lb_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_lb_worker.obj"	"$(INTDIR)\jk_lb_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_map.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_map.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_map.obj"	"$(INTDIR)\jk_map.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_md5.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_md5.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_md5.obj"	"$(INTDIR)\jk_md5.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_msg_buff.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_msg_buff.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_msg_buff.obj"	"$(INTDIR)\jk_msg_buff.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_nwmain.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_nwmain.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_nwmain.obj"	"$(INTDIR)\jk_nwmain.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_pool.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_pool.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_pool.obj"	"$(INTDIR)\jk_pool.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_sockbuf.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_sockbuf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_sockbuf.obj"	"$(INTDIR)\jk_sockbuf.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_uri_worker_map.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_uri_worker_map.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_uri_worker_map.obj"	"$(INTDIR)\jk_uri_worker_map.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_util.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_util.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_util.obj"	"$(INTDIR)\jk_util.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\common\jk_worker.c

!IF  "$(CFG)" == "isapi - Win32 Release"


"$(INTDIR)\jk_worker.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "isapi - Win32 Debug"


"$(INTDIR)\jk_worker.obj"	"$(INTDIR)\jk_worker.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

