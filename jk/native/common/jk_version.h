/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *          Copyright (c) 1999-2001 The Apache Software Foundation.          *
 *                           All rights reserved.                            *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * Redistribution and use in source and binary forms,  with or without modi- *
 * fication, are permitted provided that the following conditions are met:   *
 *                                                                           *
 * 1. Redistributions of source code  must retain the above copyright notice *
 *    notice, this list of conditions and the following disclaimer.          *
 *                                                                           *
 * 2. Redistributions  in binary  form  must  reproduce the  above copyright *
 *    notice,  this list of conditions  and the following  disclaimer in the *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. The end-user documentation  included with the redistribution,  if any, *
 *    must include the following acknowlegement:                             *
 *                                                                           *
 *       "This product includes  software developed  by the Apache  Software *
 *        Foundation <http://www.apache.org/>."                              *
 *                                                                           *
 *    Alternately, this acknowlegement may appear in the software itself, if *
 *    and wherever such third-party acknowlegements normally appear.         *
 *                                                                           *
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
 *    Foundation"  must not be used  to endorse or promote  products derived *
 *    from this  software without  prior  written  permission.  For  written *
 *    permission, please contact <apache@apache.org>.                        *
 *                                                                           *
 * 5. Products derived from this software may not be called "Apache" nor may *
 *    "Apache" appear in their names without prior written permission of the *
 *    Apache Software Foundation.                                            *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES *
 * INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY *
 * AND FITNESS FOR  A PARTICULAR PURPOSE  ARE DISCLAIMED.  IN NO EVENT SHALL *
 * THE APACHE  SOFTWARE  FOUNDATION OR  ITS CONTRIBUTORS  BE LIABLE  FOR ANY *
 * DIRECT,  INDIRECT,   INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL *
 * DAMAGES (INCLUDING,  BUT NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS *
 * OR SERVICES;  LOSS OF USE,  DATA,  OR PROFITS;  OR BUSINESS INTERRUPTION) *
 * HOWEVER CAUSED AND  ON ANY  THEORY  OF  LIABILITY,  WHETHER IN  CONTRACT, *
 * STRICT LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN *
 * ANY  WAY  OUT OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF  ADVISED  OF THE *
 * POSSIBILITY OF SUCH DAMAGE.                                               *
 *                                                                           *
 * ========================================================================= *
 *                                                                           *
 * This software  consists of voluntary  contributions made  by many indivi- *
 * duals on behalf of the  Apache Software Foundation.  For more information *
 * on the Apache Software Foundation, please see <http://www.apache.org/>.   *
 *                                                                           *
 * ========================================================================= */

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
#define JK_VERFIX       4
#define JK_VERSTRING    "1.2.4"

/* Beta number */
#define JK_VERBETA      0
#define JK_BETASTRING   "1"
/* set JK_VERISRELEASE to 1 when release (do not forget to commit!) */
#define JK_VERISRELEASE 1
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
