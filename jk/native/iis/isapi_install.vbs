'
' Copyright 1999-2004 The Apache Software Foundation
'
' Licensed under the Apache License, Version 2.0 (the "License");
' you may not use this file except in compliance with the License.
' You may obtain a copy of the License at
'
'    http://www.apache.org/licenses/LICENSE-2.0
'
' Unless required by applicable law or agreed to in writing, software
' distributed under the License is distributed on an "AS IS" BASIS,
' WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
' See the License for the specific language governing permissions and
' limitations under the License.
'

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
filterLib = "\isapi_redirect.dll"
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
shell.RegWrite regRoot + "extension_uri", "/jakarta/isapi_redirect.dll"
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
