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

/***************************************************************************
 * Description: DSAPI plugin for Lotus Domino                              *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef __inifile_h
#define __inifile_h

#include "jk_pool.h"

#define ERRTYPE const char *
#define ERRFMT  "%s"			/* natural printf format for errors */
#define ERRTXT(e) (e)			/* macro to return text for an error */
#define ERRNONE NULL
extern ERRTYPE inifile_outofmemory;
extern ERRTYPE inifile_filenotfound;
extern ERRTYPE inifile_readerror;

/* Read an INI file from disk
 */
ERRTYPE inifile_read(jk_pool_t *pool, const char *name);

/* Find the value associated with the given key returning it or NULL
 * if no match is found. Key name matching is case insensitive.
 */
const char *inifile_lookup(const char *key);

#endif /* __inifile_h */
