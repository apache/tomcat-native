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
 * Description: JK version header file                                     *
 * Author:      Jean-Frederic Clere <jfrederic.clere@fujitsu-siemens.com>  *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef __JK_VERSION_H
#define __JK_VERSION_H

/************** START OF AREA TO MODIFY BEFORE RELEASING *************/
#define JK_VERMAJOR     1
#define JK_VERMINOR     2
#define JK_VERFIX       6 
#define JK_VERSTRING    "1.2.6"

/* Beta number */
#define JK_VERBETA      0
#define JK_BETASTRING   "1"
/* set JK_VERISRELEASE to 1 when release (do not forget to commit!) */
#define JK_VERISRELEASE 0
/************** END OF AREA TO MODIFY BEFORE RELEASING *************/

#define PACKAGE "mod_jk/"
/* Build JK_EXPOSED_VERSION and JK_VERSION */
#define JK_EXPOSED_VERSION_INT PACKAGE JK_VERSTRING

#if ( JK_VERISRELEASE == 1 )
  #define JK_RELEASE_STR  JK_EXPOSED_VERSION_INT
#else
  #define JK_RELEASE_STR  JK_EXPOSED_VERSION_INT "-dev"
#endif

#if ( JK_VERBETA == 0 )
    #define JK_EXPOSED_VERSION JK_RELEASE_STR
    #undef JK_VERBETA
    #define JK_VERBETA 255
#else
    #define JK_EXPOSED_VERSION JK_RELEASE_STR "-beta-" JK_BETASTRING
#endif

#define JK_MAKEVERSION(major, minor, fix, beta) (((major) << 24) + ((minor) << 16) + ((fix) << 8) + (beta))

#define JK_VERSION JK_MAKEVERSION(JK_VERMAJOR, JK_VERMINOR, JK_VERFIX, JK_VERBETA)

#endif /* __JK_VERSION_H */
