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
 * @package Configuration
 * @author  Pier Fumagalli <mailto:pier@betaversion.org>
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
    /** The list of allowed (can be served by the web server) URL-patterns. */
    wa_chain *allw;
    /** The list of denied (can't be served by the web server) URL-patterns. */
    wa_chain *deny;
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
