' ========================================================================= 
'                                                                           
'                 The Apache Software License,  Version 1.1                 
'                                                                           
'          Copyright (c) 1999-2001 The Apache Software Foundation.          
'                           All rights reserved.                            
'                                                                           
' ========================================================================= 
'                                                                           
' Redistribution and use in source and binary forms,  with or without modi- 
' fication, are permitted provided that the following conditions are met:   
'                                                                           
' 1. Redistributions of source code  must retain the above copyright notice 
'    notice, this list of conditions and the following disclaimer.          
'                                                                           
' 2. Redistributions  in binary  form  must  reproduce the  above copyright 
'    notice,  this list of conditions  and the following  disclaimer in the 
'    documentation and/or other materials provided with the distribution.   
'                                                                           
' 3. The end-user documentation  included with the redistribution,  if any, 
'    must include the following acknowlegement:                             
'                                                                           
'       "This product includes  software developed  by the Apache  Software 
'        Foundation <http://www.apache.org/>."                              
'                                                                           
'    Alternately, this acknowlegement may appear in the software itself, if 
'    and wherever such third-party acknowlegements normally appear.         
'                                                                           
' 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     
'    Foundation"  must not be used  to endorse or promote  products derived 
'    from this  software without  prior  written  permission.  For  written 
'    permission, please contact <apache@apache.org>.                        
'                                                                           
' 5. Products derived from this software may not be called "Apache" nor may 
'    "Apache" appear in their names without prior written permission of the 
'    Apache Software Foundation.                                            
'                                                                           
' THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES 
' INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY 
' AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL 
' THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY 
' DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL 
' DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS 
' OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) 
' HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, 
' STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
' ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE 
' POSSIBILITY OF SUCH DAMAGE.                                               
'                                                                           
' ========================================================================= 
'                                                                           
' This software  consists of voluntary  contributions made  by many indivi- 
' duals on behalf of the  Apache Software Foundation.  For more information 
' on the Apache Software Foundation, please see <http://www.apache.org/>.   
'                                                                           
' =========================================================================

' =========================================================================
' Description: Install script for Jakarta Tomcat ISAPI Redirector 2                              
' Author:      Mladen Turk <mturk@apache.org>                           
' Version:     $Revision$                                           
' =========================================================================
'
Option Explicit

Dim WshShell, WshEnvironment
Dim FileSys

Dim TOMCAT_HOME, APACHE_HOME, APACHE2_HOME, JAVA_HOME

Const BIF_returnonlyfsdirs   = &H0001
Const BIF_browseincludefiles = &H4000
Const ForReading = 1
Const ForWriting = 1

Const JK2_REGISTRY           = "HKEY_LOCAL_MACHINE\SOFTWARE\Apache Software Foundation\Jakarta Isapi Redirector\2.0\"
' Get the WshShell object.
Set WshShell = CreateObject("WScript.Shell")
' Get collection by using the Environment property.
Set WshEnvironment = WshShell.Environment("Process")
' Create FileSystemObject object to access file system.
Set FileSys = WScript.CreateObject("Scripting.FileSystemObject")

' Read Environment variables
TOMCAT_HOME  = WshEnvironment("TOMCAT_HOME")

Function IsValue(obj)
    ' Check whether a value has been returned.
    Dim tmp
    On Error Resume Next
    tmp = " " & obj
    If Err <> 0 Then
        IsValue = False
    Else
        IsValue = True
    End If
    On Error GoTo 0
End Function

Dim objDlg, objF

If TOMCAT_HOME = "" Then
    Set objDlg = WScript.CreateObject("Shell.Application")
    Set objF = objDlg.BrowseForFolder(&H0, _
                "Select the Tomcat Instalaton folder", _
                BIF_returnonlyfsdirs, "")
    If IsValue(objF) Then 
        TOMCAT_HOME = objF.ParentFolder.ParseName(objF.Title).Path
    Else
        WScript.Quit
    End If
End If    

Dim workers2properties
Dim jk2properties
Dim isapi_redirector2
Dim server, serverId

workers2properties = TOMCAT_HOME & "\conf\workers2.properties"
jk2properties     = TOMCAT_HOME & "\conf\jk2.properties"
isapi_redirector2  = TOMCAT_HOME & "\native\i386\isapi_redirector2.dll2"

If Not FileSys.FileExists(workers2properties) Then
    WScript.Echo  "Cannot find " & workers2properties
    WScript.Quit
End If
If Not FileSys.FileExists(jk2properties) Then
    WScript.Echo  "Cannot find " & jk2properties
    WScript.Quit
End If
If Not FileSys.FileExists(isapi_redirector2) Then
    Set objDlg = WScript.CreateObject("Shell.Application")
    Set objF = objDlg.BrowseForFolder(&H0, _
                "Select the ISAPI redirector dll", _
                BIF_returnonlyfsdirs + BIF_browseincludefiles, "")
    If IsValue(objF) Then 
        isapi_redirector2 = objF.ParentFolder.ParseName(objF.Title).Path
    Else
        WScript.Quit
    End If
End If

WshShell.RegWrite JK2_REGISTRY & "serverRoot", TOMCAT_HOME
WshShell.RegWrite JK2_REGISTRY & "extensionUri", "/jakarta/isapi_redirector2.dll" 
WshShell.RegWrite JK2_REGISTRY & "workersFile", TOMCAT_HOME & "\conf\workers2.properties"
WshShell.RegWrite JK2_REGISTRY & "authComplete", "0"
WshShell.RegWrite JK2_REGISTRY & "threadPool", "20"

Function CreateVirtualDir(filterName, homeDir)
    On Error GoTo 0

    Dim root, vdir
    Dim service, thing

    Set service = GetObject("IIS://LocalHost/W3SVC" )

    serverId = "" 
    For Each thing In service
        If thing.Class = "IIsWebServer" then
            If thing.ServerComment = "Default Web Site" Then 
                Set server = thing
                    serverId = thing.name
                Exit For
            End If
        End If
    Next

    Set root = GetObject( "IIS://LocalHost/W3SVC/" + serverID + "/ROOT" )
    On Error Resume Next
    Set vdir = root.Create("IISWebVirtualDir", filterName )
    
    If err Then
        On Error Resume Next
        root.delete "IISWebVirtualDir", filterName
        root.SetInfo
        If err Then
            WScript.Echo"Error Deleting Directory " & filtername
            WScript.Quit        
        End If
        Set vdir = root.create("IISWebVirtualDir", filterName )
        If err Then
            WScript.Echo "Error Creating Directory " & filterName
            WScript.Quit        
        End If
    End If     

' Set the directory information as an application directory
    vdir.AppCreate2 1
    vdir.AccessExecute = TRUE
    vdir.AppFriendlyName = filterName
    vdir.AccessRead = false
    vdir.ContentIndexed = false
    vdir.Path = homeDir
    vdir.SetInfo
    If err Then
        WScript.Echo "Error saving " & filterName & " directory"
        WScript.Quit        
    End If    
End Function

Function CreateISAPIFilter(filterName, filterPath)
    On Error Resume Next

    Dim FiltersObj, Filter, loadorder

    Set FiltersObj = GetObject("IIS://LocalHost/W3SVC/" & serverId & "/Filters")

    If err Then 
        err.clear
        Set FiltersObj = server.create( "IIsFilters", "Filters" )
        FiltersObj.SetInfo
        If err Then
            WScript.Echo "Error Creating Filters"
            WScript.Quit        
        End If
    End if
'
' Create the filter - if it fails then delete it and try again
'
    Set Filter = FiltersObj.Create( "IISFilter", filterName )
    If err Then
        err.clear
        FiltersObj.Delete "IISFilter", filterName
        If err Then
            WScript.Echo "Error Deleting Filter " & filterName & vbCrLf & " Delete the filtermanualy"
            WScript.Quit        
        End If
        FiltersObj.SetInfo
        Set Filter = FiltersObj.Create( "IISFilter", filterName )
        If err Then
            WScript.Echo "Error Creating Filter"
            WScript.Quit        
        End If
    End If
    Filter.FilterPath = filterPath
    Filter.SetInfo
    
    loadorder = FiltersObj.FilterLoadOrder

    If loadorder <> "" Then loadorder = loadorder & ","
    ' Add new filter to end of load order list
    loadorder = loadorder & filterName
    FiltersObj.FilterLoadOrder = loadorder

    ' Write changes back to Metabase
    FiltersObj.SetInfo    
End Function

Function RewriteConfig(fileName)
    Dim f, o
    Dim l, p
    FileSys.CopyFile TOMCAT_HOME & "\" & fileName, TOMCAT_HOME & "\" & fileName & ".bak", True
    
    Set f = FileSys.OpenTextFile(TOMCAT_HOME & "\" & fileName & ".bak", ForReading)    
    Set o = FileSys.GetFile(TOMCAT_HOME & "\" & fileName)
    o.Delete
    Set o = FileSys.CreateTextFile(TOMCAT_HOME & "\" & fileName, ForWriting, True)
    p = Replace(TOMCAT_HOME, "\", "/")
    Do While f.AtEndOfStream <> True
      l = Replace( f.ReadLine, "${serverRoot}", p )
      l = Replace( l, "${TOMCAT_HOME}", TOMCAT_HOME )
      o.WriteLine l
    Loop
    f.Close
    o.Close
End Function


CreateVirtualDir "jakarta", TOMCAT_HOME & "\native\i386"
CreateISAPIFilter "jakarta", TOMCAT_HOME & "\native\i386\isapi_redirector2.dll"

RewriteConfig "conf\jk2.properties"
RewriteConfig "conf\workers2.properties"

WScript.Echo "JK2 ISAPI Redirector Succesfuly installed" & vbCrLf & "Please restart the IIS service"
