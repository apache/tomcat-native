/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * Description: Install script for Tomcat JK2 ISAPI redirector
 * Author:      Mladen Turk <mturk@apache.org>                           
 * Version:     $Revision: 1.0 $                                           
 */


/**
 * Global variables
 */
//var IIsWebService;
//var IIsWebServer;
//var IIsROOT;
//var AppParams;

/**
 * Defaults
 */
_DEFAULT_SERVER_NAME = "Default Web Site";
_DEFAULT_DESCRIPTION = "JK2 ISAPI Redirector";
_DEFAULT_FILTER_NAME = "jakarta";
_DEFAULT_HEADERS     = "X-Powered-By: Apache Software Foundation"
_DEFAULT_OPTIONS     = "rbdi";
_DEFAULT_REGISTRY    = "HKLM\\SOFTWARE\\Apache Software Foundation\\Jakarta Isapi Redirector\\2.0";
_DEFAULT_FILTERLIB   = "isapi_redirector2.dll";
_DEFAULT_WORKERS2    = "\\conf\\workers2.properties";

/**
 * Set this to false to disable TRACE messages;
 */
_DEBUG = true;
_TRACE_COUNTER = 1;

/**
 * Constants variables
 */
_APP_INPROC  = 0;
_APP_OUTPROC = 1;
_APP_POOLED  = 2
_IIS_OBJECT  = "IIS://LocalHost/W3SVC";
_IIS_SERVER  = "IIsWebServer";
_IIS_WEBDIR  = "IIsWebVirtualDir";
_IIS_FILTERS = "IIsFilters";
_IIS_FILTER  = "IIsFilter";

function ERROR(args, sMsg)
{
    WScript.Echo("Error processing " + args.script + "\n" + sMsg);
    WScript.Quit(-1);
}

function RPAD(str, n)
{
    var p;
    p = str;
    for (i = str.length; i < n; i++)
        p += " ";
    return p;
}

function HEX(num)
{
    var digits = "0123456789ABCDEF";
    var n = num;
    var h = "";
    for (i = 0; i < 8; i++) {
        h = digits.charAt(n & 15) + h;
        n = n >>> 4;
    }
    return h;
}

function TRACE(sMsg)
{
    if (_DEBUG) {
        var line = _TRACE_COUNTER + "    ";
        WScript.Echo(line.substring(0, 4) + sMsg);   
        ++_TRACE_COUNTER;
    }
}

function EXCEPTION(exception, func)
{
    WScript.Echo(exception + " In function '" + 
                 func + "'\nError number: " +
                 HEX(exception.number) + " - " + exception.description + "");
    WScript.Quit(-1);                     
}


function Parameters()
{
    this.ServerName     = _DEFAULT_SERVER_NAME;
    this.WebDescription = _DEFAULT_DESCRIPTION;
    this.FilterName     = _DEFAULT_FILTER_NAME;
    this.FilterDesc     = _DEFAULT_DESCRIPTION;
    this.WebName        = _DEFAULT_FILTER_NAME;
    this.Headers        = _DEFAULT_HEADERS;
    this.WebOptions     = _DEFAULT_OPTIONS;
    this.RegistryKey    = _DEFAULT_REGISTRY;
    this.AppProtection  = _APP_POOLED;
    this.WebPath        = "C:";
    this.FilterLib      = _DEFAULT_FILTERLIB;
}

function findWebServiceObject(clsName, objName)
{
    var webService;
    var webObjects;
    try {
        webService = GetObject(_IIS_OBJECT);
        if (!clsName || !objName)
            return webService;
        webObjects = new Enumerator(webService);
        while (!webObjects.atEnd()) {
            TRACE(RPAD(webObjects.item().Class, 18) + 
                  RPAD(webObjects.item().Name, 15) + webObjects.item().AdsPath);
            if (webObjects.item().Class == clsName &&
                webObjects.item().Name  == objName)
                return webObjects.item();
            
            webObjects.moveNext();            
        }        
    }
    catch(exception) {
        EXCEPTION(exception, "findWebServiceObject");
    } 
    
    return null;
}

function findWebServer(serverComment)
{
    var webService;
    var webObjects;
    try {
        webService = GetObject(_IIS_OBJECT);

        webObjects = new Enumerator(webService);
        while (!webObjects.atEnd()) {
            if (webObjects.item().Class == _IIS_SERVER &&
                webObjects.item().ServerComment  == serverComment)
                return webObjects.item();
            
            webObjects.moveNext();            
        }        
    }
    catch(exception) {
        EXCEPTION(exception, "findWebServer");
    } 
    
    return null;
}

function findDefaultWebServer()
{
    return findWebServer(_DEFAULT_SERVER_NAME);
}


function findADSIObject(adsiObject, clsName, objName)
{
    var adsiObjects;
    try {
        adsiObjects = new Enumerator(adsiObject);
        while (!adsiObjects.atEnd()) {
            TRACE(RPAD(adsiObjects.item().Class, 18) + 
                  RPAD(adsiObjects.item().Name, 15) + adsiObjects.item().AdsPath);
            if (adsiObjects.item().Class == clsName &&
                adsiObjects.item().Name  == objName)
                return adsiObjects.item();
            
            adsiObjects.moveNext();            
        }        
    }
    catch(exception) {        
        EXCEPTION(exception, "findADSIObject");
    } 
    
    return null;
        
}

function hasOption(optString, optName)
{
    if (optString.indexOf(optName) == -1)
        return false;
    else {
        /* Check if the option is dissabled using '-' prefix */
        if (optString.indexOf("-" + optName) == -1)
            return true;
        else
            return false;
    }
}

function createVirtualDir(webRootDir, appParams)
{
    var newDir;
    try {
        newDir = findADSIObject(webRootDir, _IIS_WEBDIR, appParams.WebName);
        if (newDir == null) {
            
            TRACE("Creating new directory...");
            
            newDir = webRootDir.Create(_IIS_WEBDIR, appParams.WebName);
            newDir.AppFriendlyName = appParams.WebDescription;
            newDir.Path = appParams.WebPath;
            newDir.AppCreate2(appParams.AppProtection);
            newDir.HttpCustomHeaders = appParams.Headers;
        }
        else {
            TRACE("Updating existing directory...");            
        }
        TRACE("Setting directory options...");
        newDir.AccessExecute  = hasOption(appParams.WebOptions, "x");
        newDir.AccessRead     = hasOption(appParams.WebOptions, "r");
        newDir.AccessWrite    = hasOption(appParams.WebOptions, "w");
        newDir.AccessScript   = hasOption(appParams.WebOptions, "s");
        newDir.ContentIndexed = hasOption(appParams.WebOptions, "i");
        newDir.EnableDirBrowsing = hasOption(appParams.WebOptions, "b");
        newDir.EnableDefaultDoc  = hasOption(appParams.WebOptions, "d");
        newDir.SetInfo();
        
        TRACE("Virtual directory [/" + appParams.WebName + "] set.");
        return newDir;
    }
    catch(exception) {        
        EXCEPTION(exception, "createVirtualDir");
    } 
    
    return null;        
}

function createISAPIFilter(webServer, appParams)
{
    var filters;
    var newFilter;
    try {
        filters = findADSIObject(webServer, _IIS_FILTERS, "Filters");
        if (filters == null) {
            //may have to create the website-level filters container
            filters = webserver.create(_IIS_FILTERS, "Filters");
        }
        newFilter = findADSIObject(filters, _IIS_FILTER, appParams.FilterName);
        if (newFilter == null) {
            
            TRACE("Creating new ISAPI filter...");
            
            newFilter = filters.Create(_IIS_FILTER, appParams.FilterName);
        }
        else {
            TRACE("Updating existing filter...");            
        }
        TRACE("Setting filter options...");

        TRACE("Filters order " + filters.FilterLoadOrder);
        newFilter.FilterPath  = appParams.WebPath + "\\" + appParams.FilterLib;
        newFilter.FilterDescription  = appParams.FilterDesc;
        newFilter.SetInfo();
        if (filters.FilterLoadOrder.indexOf(appParams.FilterName) == -1) {
            filters.FilterLoadOrder += ", " +  appParams.FilterName;   
            filters.SetInfo();
        }
        TRACE("Filter [" + appParams.FilterName + "] set.");
        return newFilter;
    }
    catch(exception) {        
        EXCEPTION(exception, "createISAPIFilter");
    } 
    
    return null;    
}

function createVirtualExecDir(webRootDir, appParams)
{
    var op, rv;
    op = appParams.WebOptions;
    appParams.WebOptions = op + "+x-r-i-b-d";
    rv = createVirtualDir(webRootDir, appParams);
    appParams.WebOptions = op;        
    return rv;
}

function deleteADSIObject(adsiObject, clsName, objName)
{
    var rv = false;
    try {
        adsiObject.Delete(clsName, objName);
        rv = true;
    }
    catch(exception) {  
        /* 
         * Exception is thrown if the object doesn't exist
         * Just ignore...
         */
    } 
    
    return rv;
}

function Arguments()
{
    this.argv = WScript.Arguments;    
    this.argc = WScript.Arguments.length;
    this.optarg = null;
    this.optind = 0;
    this.optopt = null;
    this.opterr = null;

    this.program = WScript.FullName.toLowerCase();
    this.program = this.program.substr(this.program.lastIndexOf("\\") + 1);
    if (this.program.indexOf("wscript.exe") == -1)
        _DEBUG = false;
    this.script = WScript.ScriptName;
}

function getopt(args, ostr)
{
    if (args.optind >= args.argc) {
        return null;    
    }
    try {
        var opt = args.argv(args.optind);
        if (opt.charAt(0) == "-") {
            var oi = ostr.indexOf(opt.charAt(1));
            if (oi == -1) {
                args.opterr = "Invalid option switch " + opt;                
                args.optopt = null;
                args.optarg = null;
                return null;                
            }
            ++args.optind;
            if (ostr.charAt(oi + 1) == ":") {
                if (args.optind < args.argc) {
                    args.optarg = args.argv(args.optind);
                    ++args.optind;
                }
                else {
                    args.opterr = "Missing required argument value for " + opt;
                    args.optopt = null;
                    args.optarg = null;
                    return null;                                    
                }
            }
            args.optopt = ostr.charAt(oi);
            return args.optopt;
        }
    }
    catch(exception) {        
        EXCEPTION(exception, "getopt");
    }
    return null; 
}

function checkFileExists(fname)
{
    var fso;
    fso = new ActiveXObject("Scripting.FileSystemObject");
   
    if (fso.FileExists(fname))
        return true;
    else
        return false;    
}

function checkFilterExists(params)
{
    return checkFileExists(params.WebPath + "\\" +  params.FilterLib);
}


function Usage(args)
{
    var prn;
    prn = "Usage: " + args.program + " " + args.script + " [option]... " + "[path] [tomcat_home]\n\n" +
          "  -s   WEBSERVER     Use the WEBSERVER     [" + _DEFAULT_SERVER_NAME + "]\n" +
          "  -f   FILTERNAME    Use the FILTERNAME    [" + _DEFAULT_FILTER_NAME + "]\n" +
          "  -d   DESCRIPTION   Filter description    [" + _DEFAULT_DESCRIPTION + "]\n" +
          "  -v   VIRTUALDIR    Create the VIRTUALDIR [/" + _DEFAULT_FILTER_NAME + "]\n" +
          "  -l   ISAPILIB      Use the ISAPILIB      [" + _DEFAULT_FILTERLIB + "]\n" +          
          "  -h                 display this help and exit\n" +
          "\n  [path]             Virtual directory path" +
          "\n                     Set this to directory containing " + _DEFAULT_FILTERLIB +
          "\n  [tomcat_home]      Path to the tomcat home directory";

    WScript.Echo(prn);             
}

function Main(args)
{
    var params;
    var opt;
    var IIsWebService;
    var IIsWebServer;
    var IIsROOT;
    var IIsFilters;
    var IIsFilter;
    var tchome   = null;
    var workers2 = null;
    
    params = new Parameters();
    
    while ((opt = getopt(args, "s:f:d:v:l:h"))) {
        switch (opt) {
            case "s":
                params.ServerName = args.optarg;   
                break;                                     
            case "f":
                params.FilterName = args.optarg;                        
                break;                                     
            case "d":
                params.FilterDesc = args.optarg;                        
                break;                                     
            case "l":
                params.FilterLib  = args.optarg;                        
                break;                                     
            case "v":
                params.WebName    = args.optarg;                        
                break;                                     
            case "h":
            default:
                Usage(args);
                return 0;                        
                break;                                                 
        }        
    }
    TRACE("argc " + args.argc + " optind " + args.optind);
    if (args.optind >= args.argc) {
        /* Case when isapi_redirector2.dll is inside TOMCAT_HOME\bin */
        params.WebPath = WScript.ScriptFullName.substr(0,
                                 WScript.ScriptFullName.lastIndexOf("\\"));
        tchome = params.WebPath.substr(0, params.WebPath.lastIndexOf("\\"));                                         
    }
    else {
        params.WebPath = args.argv(args.optind);
        ++args.optind;
    }
    if (!checkFilterExists(params)) {
        ERROR(args, "The specified filter library could not be found...\n" +
              "File " + params.WebPath + "\\" + params.FilterLib + " does not exist.");
        
    }
    if (args.optind < args.argc)
        tchome = args.argv(args.optind);
    else if (!tchome)
        tchome = params.WebPath;                
    workers2 = tchome + _DEFAULT_WORKERS2 ;
    if (!checkFileExists(workers2)) {
        ERROR(args, "The specified configuration file could not be found...\n" +
              "File " + workers2 + " does not exist.");
        
    }

    if ((IIsWebService =  findWebServiceObject(null, null)) == null) {
        ERROR(args, "Unable to find Web Service ADSI object\n" +
              "Check the security settings...");            
    }
    
    if ((IIsWebServer =  findWebServer(params.ServerName)) == null) {
        ERROR(args, "Unable to find Web Server ADSI object...\n" +
              "The '" + params.ServerName + "' does not exists.");            
    
    }

    if ((IIsROOT = findADSIObject(IIsWebServer, _IIS_WEBDIR, "ROOT")) == null) {
        ERROR(args, "Unable to find Web Server ROOT direcrory.");
    }

    if (!createVirtualExecDir(IIsROOT, params)) {
        ERROR(args, "Unable to create virual directory /" + params.WebName);        
    }

    if (!createISAPIFilter(IIsWebServer, params)) {
        /* TODO: roll-back virtual dir */
        ERROR(args, "Unable to create the '" + params.FilterName + "' filter.");        
    }
        
    /** Finaly set the registry entries 
     */
    var WshShell = WScript.CreateObject("WScript.Shell");
     
    WshShell.RegWrite(_DEFAULT_REGISTRY + "\\extensionUri",
                      "/" + params.FilterName + "/" + params.FilterLib,
                      "REG_SZ");
    WshShell.RegWrite(_DEFAULT_REGISTRY + "\\serverRoot",
                      tchome, "REG_SZ");
    WshShell.RegWrite(_DEFAULT_REGISTRY + "\\workersFile",
                      workers2, "REG_SZ");
    WshShell.RegWrite(_DEFAULT_REGISTRY + "\\authComplete", "0", "REG_SZ");
    WshShell.RegWrite(_DEFAULT_REGISTRY + "\\threadPool", "20", "REG_SZ");
    
            
    return 0;
}

/* The main program */
var args = new Arguments();
rv = Main(args);
WScript.Quit(rv);
