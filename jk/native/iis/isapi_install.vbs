' ========================================================================= 
'                                                                           
'                 The Apache Software License,  Version 1.1                 
'                                                                           
'          Copyright (c) 1999-2003 The Apache Software Foundation.          
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
' Description: Install script for Tomcat ISAPI redirector                              
' Author:      Peter S. Horne <horneps@yahoo.com.au>                           
' Version:     $Revision$                                           
' =========================================================================
'
' This script automatically installs the tomcat isapi_redirector for use in
' both out-of and in-process installations on IIS/Win2K. See the command line
' usage section for usage instructions.

'
'  Check the command line
'
set args = wscript.arguments
if args.count <> 6 then 
	info ""
	info "Tomcat ISAPI Redirector Installation Utility"
	info "usage: isapi_install <server> <fdir> <worker> <mount> <log> <level>"
	info "	server:	The Web Server Name (for example 'Default Web Site')"
	info "	fdir:	the full path to the directory that contains the isapi filter"
	info "	worker:	Full path and file name of the worker properties file"
	info "	mount:	Full path and file name of the worker mount properties file"
	info "	log:	Full path and file name of the log file"
	info "	level:	The log level emerg | info"
	info "(Re-runs are ok and will change/reset settings)"
	info ""
	fail "Incorrect Arguments"
end if

' Setup the args
serverName = args(0)
filterDir = args(1)
filterName = "jakarta"
filterLib = "\isapi_redirector.dll"
workerFile = args(2)
mountFile = args(3)
logFile = args(4)
logLevel = args(5)

'
' Get a shell
'
dim shell
set shell = WScript.CreateObject("WScript.Shell")

'
' Find the indicated server from all the servers in the service 
' Note: they aren't all Web!
'
set service = GetObject("IIS://LocalHost/W3SVC" )
serverId = ""
for each thing in service
	 if thing.Class = "IIsWebServer" then
		if thing.ServerComment = serverName then 
			set server = thing
			serverId = thing.name
			exit for
		end if
	end if
next
if serverId = "" then fail "Server " + serverName + " not found."
info "Found Server <" + serverName + "> at index [" + serverId + "]."

'
' Stop everything to release any dlls - needed for a re-install
'
' info "Stopping server <" + serverName + ">..."
' server.stop
' info "Done"

'
' Get a handle to the filters for the server - we process all errors
'
On Error Resume Next
dim filters
set filters = GetObject("IIS://LocalHost/W3SVC/" + serverId + "/Filters")
if err then 
	err.clear
	info "Filters not found for server - creating"
	set filters = server.create( "IIsFilters", "Filters" )
	filters.setInfo
	if err then fail "Error Creating Filters"
end if
info "Got Filters"

'
' Create the filter - if it fails then delete it and try again
'
name = filterName
info "Creating Filter  - " + filterName
dim filter
set filter = filters.Create( "IISFilter", filterName )
if err then
	err.clear
	info "Filter exists - deleting"
	filters.delete "IISFilter", filterName
	if err then fail "Error Deleting Filter"
	set filter = filters.Create( "IISFilter", filterName )
	if err then fail "Error Creating Filter"
end if
info "Created Filter"

'
' Set the filter info and save it
'
filter.FilterPath = filterDir + filterLib  
filter.FilterEnabled=true
filter.description = filterName
filter.notifyOrderHigh = true
filter.setInfo

'
' Set the load order - only if it's not in the list already
'
on error goto 0
loadOrders = filters.FilterLoadOrder
list = Split( loadOrders, "," )
found = false
for each item in list
	if Trim( item ) = filterName then found = true
next

if found = false then 
	info "Filter is not in load order - adding now."
	if len(loadOrders) <> 0  then loadOrders = loadOrders + ","
	filters.FilterLoadOrder = loadOrders + filterName
	filters.setInfo
	info "Filter added."
else
	info "Filter already exists in load order - no update required."
end if

'
' Set the registry up
' 
regRoot = "HKEY_LOCAL_MACHINE\SOFTWARE\Apache Software Foundation\Jakarta Isapi Redirector\1.0\"
err.clear
on error resume next
shell.RegDelete( regRoot )
if err then 
	info "Entering Registry Information for the first time"
else 
	info "Deleted existing Registry Setting"
end if

on error goto 0
info "Updating Registry"
shell.RegWrite regRoot + "extension_uri", "/jakarta/isapi_redirector.dll"
shell.RegWrite regRoot + "log_file", logFile
shell.RegWrite regRoot + "log_level", logLevel
shell.RegWrite regRoot + "worker_file", workerFile
shell.RegWrite regRoot + "worker_mount_file", mountFile
info "Registry Settings Created"

'
' Finally, create the virtual directory matching th extension uri
' 
on error goto 0
set root = GetObject( "IIS://LocalHost/W3SVC/" + serverID + "/ROOT" )
on error resume next
set vdir = root.Create("IISWebVirtualDir", filterName )
if err then
	info "Directory exists - deleting"
	on error resume next
	root.delete "IISWebVirtualDir", filterName
	root.setInfo
	if err then fail "Error Deleting Directory"
	set vdir = root.create("IISWebVirtualDir", filterName )
	if err then fail "Error Creating Directory"
end if
info "Directory Created"

' Set the directory information - make it an application directory
info "Setting Directory Information"
vdir.AppCreate2 1
vdir.AccessExecute = TRUE
vdir.AppFriendlyName = filterName
vdir.AccessRead = false
vdir.ContentIndexed = false
vdir.Path = filterDir
vdir.setInfo
if err then fail "Error saving new directory"
info "Directory Saved"
'
' Re Start 
'
' info "Starting server <" + serverName + ">..."
' server.start
' info "Done"

info "All done... Bye."
wscript.quit(0)

' 
' Helper function for snafus
'
function fail( message )
	wscript.echo "E: " + message
	wscript.quit(1)
end function

'
' Helper function for info
'
function info( message )
	wscript.echo " " + message
end function 
