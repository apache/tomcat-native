/*
 * Copyright 1999-2004 The Apache Software Foundation.
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

#define MAKEVERSION(a, b, c, d) \
	(((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

/* the _memicmp() function is available */
#if defined(WIN32)

#define HAVE_MEMICMP
#undef USE_INIFILE

#elif defined(LINUX)

#undef HAVE_MEMICMP
#define USE_INIFILE

#elif defined(SOLARIS)

#undef HAVE_MEMICMP
#define USE_INIFILE

#else
#error Please define one of WIN32, LINUX or SOLARIS
#endif

/* define if you don't have the Notes C API which is available from
 *
 *    http://www.lotus.com/rw/dlcapi.nsf
 */
/* #undef NO_CAPI */

#define DEBUG(args) \
	do { /*printf args ;*/ } while (0)

#endif /* __config_h */
