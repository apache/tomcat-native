/*
 * Copyright 1999-2001,2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/***************************************************************************
 * Description: DSAPI plugin for Lotus Domino                              *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef __config_h
#define __config_h

#define NONBLANK(s) \
    (NULL != (s) && '\0' != *(s))

/* the _memicmp() function is available */
#if defined(WIN32)

#define HAVE_MEMICMP
#define PATHSEP '\\'

#else

#undef HAVE_MEMICMP
#define PATHSEP '/'
#define MAX_PATH 512

#endif

#ifdef _DEBUG
#define DEBUG(args) \
    do { _printf args ; } while (0)
#else
#define DEBUG(args) \
    do { } while (0)
#endif

#if !defined(DLLEXPORT)
#if defined(WIN32) && !defined(TESTING)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif
#endif

/* Configuration tags */
#define SERVER_ROOT_TAG     "serverRoot"
#define WORKER_FILE_TAG     "workersFile"
#define TOMCAT_START_TAG    "tomcatStart"
#define TOMCAT_STOP_TAG     "tomcatStop"
#define TOMCAT_TIMEOUT_TAG  "tomcatTimeout"
#define VERSION             "2.0.5"
#define VERSION_STRING      "Jakarta/DSAPI/" VERSION
#define FILTERDESC          "Apache Tomcat Interceptor (" VERSION_STRING ")"
#define SERVERDFLT          "Lotus Domino"
#define REGISTRY_LOCATION   "Software\\Apache Software Foundation\\Jakarta Dsapi Redirector\\2.0"
#define TOMCAT_STARTSTOP_TO 30000               /* 30 seconds */
#define CONTENT_LENGTH      "Content-length"    /* Name of CL header */
#define PROPERTIES_EXT      ".properties"

#endif /* __config_h */
