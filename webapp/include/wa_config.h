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
 * 4. The names  "The  Jakarta  Project",  "WebApp",  and  "Apache  Software *
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

/**
 * @package Configuration
 * @author  Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>
 * @version $Id$
 */
#ifndef _WA_CONFIG_H_
#define _WA_CONFIG_H_

/**
 * The WebApp Library connection structure.
 * <br>
 * This structure holds all required data required by a connection provider
 * to connect to a web-application container and to handle HTTP requests.
 */
struct wa_connection {
    /** The connection name. */
    char *name;
    /** The connection provider. */
    wa_provider *prov;
    /** The connection parameter (as in the configuration file). */
    char *parm;
    /** The provider-specific configuration member for this connection. */
    void *conf;
};

/**
 * The WebApp Library virtual host structure.
 * <br>
 * This structure holds informations related to a virtual host under which
 * web-applications are deployed.
 */
struct wa_virtualhost {
    /** The virtual host name. */
    char *name;
    /** The virtual host port. */
    int port;
    /** The list of all applications deployed in this virtual hosts. */
    wa_chain *apps;
};

/**
 * The WebApp Library application structure.
 * <br>
 * This structure holds all informations associated with an application.
 * Applications are not grouped in virtual hosts inside the library as in
 * specific cases (like when load balancing is in use), multiple applications
 * can share the same root URL path, or (like when applications are shared),
 * a single web application can be shared across multiple virtual host.
 */
struct wa_application {
    /** The application virtual host. */
    wa_virtualhost *host;
    /** The application connection. */
    wa_connection *conn;
    /** The provider-specific configuration member for this application. */
    void *conf;
    /** The application name. */
    char *name;
    /** The application root URL path. */
    char *rpth;
    /** The local expanded application path (if any). */
    char *lpth;
    /** Wether this web-application has been deployed or not. */
    wa_boolean depl;
};

/**
 * Allocate and set up a <code>wa_application</code> member.
 *
 * @param a Where the pointer to where the <code>wa_application</code> member
 *          must be stored.
 * @param n The application name. This parameter will be passed to the
 *          application container as its unique selection key within its
 *          array of deployable applications (for example the .war file name).
 * @param p The root URL path of the web application to deploy.
 * @return <b>NULL</b> on success or an error message on faliure.
 */
const char *wa_capplication(wa_application **a,
                            const char *n,
                            const char *p);

/**
 * Allocate and set up a <code>wa_virtualhost</code> member.
 *
 * @param h The pointer to where the <code>wa_virtualhost</code> member must
 *          be stored.
 * @param n The virtual host base name.
 * @param p The virtual host primary port.
 * @return <b>NULL</b> on success or an error message on faliure.
 */
const char *wa_cvirtualhost(wa_virtualhost **h,
                            const char *n,
                            int p);

/**
 * Allocate and set up a <code>wa_connection</code> member.
 *
 * @param c Where the pointer to where the <code>wa_connection</code> member
 *          must be stored.
 * @param n The connection name.
 * @param p The connection provider name.
 * @param a The connection provider parameter from a configuration file.
 * @return <b>NULL</b> on success or an error message on faliure.
 */
const char *wa_cconnection(wa_connection **c,
                           const char *n,
                           const char *p,
                           const char *a);

#endif /* ifndef _WA_CONFIG_H_ */
