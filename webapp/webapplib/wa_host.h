/* ========================================================================= *
 *                                                                           *
 *                 The Apache Software License,  Version 1.1                 *
 *                                                                           *
 *         Copyright (c) 1999, 2000  The Apache Software Foundation.         *
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
 * 4. The names  "The  Jakarta  Project",  "Tomcat",  and  "Apache  Software *
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

// CVS $Id$
// Author: Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>

#ifndef _WA_HOST_H_
#define _WA_HOST_H_

/**
 * The wa_application structure contains all required configuration data for
 * web applications.
 */
struct wa_application {
    char *name;             // The name of this web application
    char *path;             // The web application root URI path
    void *conf;             // Provider specific configurations
    wa_connection *conn;    // Pointer to the appropriate connection
    wa_application *next;   // Pointer to the next web application
};

/**
 * The wa_host structure represents a configured host.
 */
struct wa_host {
    char *name;             // The main name of this host
    int port;               // The main port server
    wa_application *apps;   // The list of configured web applications
    wa_host *next;          // Pointer to the next configured host
};

/* The list of configured hosts */
extern wa_host *wa_hosts;

/* Function prototype declaration */
// Create configuration for a new host.
const char *wa_host_create(char *, int);
// Get the host configuration.
wa_host *wa_host_get(char *, int);
// Configure a web application for a specific host.
const char *wa_host_setapp(wa_host *, char *, char *, wa_connection *);
// Configure a web application for a specific host.
const char *wa_host_setapp_byname(char *, int, char *, char *, wa_connection *);
// Retrieve a web application for a specific host.
wa_application *wa_host_findapp(wa_host *, char *);
// Retrieve a web application for a specific host.
wa_application *wa_host_findapp_byname(char *, int , char *);

#endif // ifdef _WA_HOST_H_
