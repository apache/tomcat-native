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
 * @package Main
 * @author  Pier Fumagalli <mailto:pier@betaversion.org>
 * @version $Id$
 */
#ifndef _WA_MAIN_H_
#define _WA_MAIN_H_

/**
 * A simple chain structure holding lists of pointers.
 */
struct wa_chain {
    /** The pointer to the current element in the chain. */
    void *curr;
    /**
     * The pointer to the next chain structure containing the next element
     * or <b>NULL</b> if this is the last element in the chain. */
    wa_chain *next;
};

/**
 * The WebApp Library connection provider structure.
 * <br>
 * This structure contains all data and function pointers to be implemented
 * by a connection provider.
 */
struct wa_provider {
    /**
     * The name of this provider.
     */
    const char *name;

    /**
     * Initialize a provider.
     * <br>
     * This function is called when the WebApp Library is initialized, before
     * any web application is deployed.
     *
     * @return An error message or NULL.
     */
    const char *(*init)(void);

    /**
     * Notify the provider of its imminent startup.
     * <br>
     * After all configurations are generated, this function is called to
     * notify the provider to expect requests to be handled.
     */
    void (*startup)(void);

    /**
     * Cleans up all resources allocated by the provider.
     * <br>
     * The provider is notified that no further requests will be handled, and
     * the WebApp library is being shut down.
     */
    void (*shutdown)(void);

    /**
     * Configure a connection with the parameter from the web server
     * configuration file.
     *
     * @param conn The connection to configure.
     * @param param The extra parameter from web server configuration.
     * @return An error message or NULL.
     */
    const char *(*connect)(wa_connection *conn, const char *param);

    /**
     * Receive notification of the deployment of an application.
     *
     * @param appl The application being deployed.
     * @return An error message or NULL.
     */
    const char *(*deploy)(wa_application *appl);

    /**
     * Describe the configuration member found in a connection.
     *
     * @param conn The connection for wich a description must be produced.
     * @param pool The memory pool where the returned string must be allocated.
     * @return The description for a connection configuration or <b>NULL</b>.
     */
    char *(*conninfo)(wa_connection *conn, apr_pool_t *pool);

    /**
     * Describe the configuration member found in a web application.
     *
     * @param appl The application for wich a description must be produced.
     * @param pool The memory pool where the returned string must be allocated.
     * @return The description for an application configuration or <b>NULL</b>.
     */
    char *(*applinfo)(wa_application *appl, apr_pool_t *pool);

    /**
     * Handle a connection from the web server.
     *
     * @param req The request data.
     * @param appl The application associated with the request.
     * @param return The HTTP status code of the response.
     */
    int (*handle)(wa_request *req, wa_application *appl);
};

/**
 * Initialize the WebApp Library.
 * This function must be called <b>before</b> any other calls to any other
 * function to set up the APR and WebApp Library internals. If any other
 * function is called before this function has been invoked will result in
 * impredictable results.
 *
 * @return <b>NULL</b> on success or an error message on faliure.
 */
const char *wa_init(void);

/**
 * Clean up the WebApp Library.
 * This function releases all memory and resouces used by the WebApp library
 * and must be called before the underlying web server process exits. Any call
 * to any other WebApp Library function after this function has been invoked
 * will result in impredictable results.
 */
void wa_startup(void);

/**
 * Clean up the WebApp Library.
 * This function releases all memory and resouces used by the WebApp library
 * and must be called before the underlying web server process exits. Any call
 * to any other WebApp Library function after this function has been invoked
 * will result in impredictable results.
 */
void wa_shutdown(void);

/**
 * Deploy a web-application.
 *
 * @param a The <code>wa_application</code> member of the web-application to
 *          deploy.
 * @param h The <code>wa_virtualhost</code> member of the host under which the
 *          web-application has to be deployed.
 * @param c The <code>wa_connection</code> member of the connection used to
 *          reach the application.
 * @return <b>NULL</b> on success or an error message on faliure.
 */
const char *wa_deploy(wa_application *a,
                      wa_virtualhost *h,
                      wa_connection *c);

/**
 * Dump some debugging information.
 *
 * @param f The file where this function was called.
 * @param l The line number where this function was called.
 * @param fmt The format string of the debug message (printf style).
 * @param others The parameters to the format string.
 */
void wa_debug(const char *f, const int l, const char *fmt, ...);

/**
 * Report an error to the web-server log file.
 * <br>
 * NOTE: This function is _NOT_ defined in the WebApp library. It _MUST_ be
 * defined and implemented within the web-server module.
 *
 * @param f The file where this function was called.
 * @param l The line number where this function was called.
 * @param fmt The format string of the debug message (printf style).
 * @param others The parameters to the format string.
 */
void wa_log(const char *f, const int l, const char *fmt, ...);

/**
 * The WebApp library memory pool.
 */
extern apr_pool_t *wa_pool;

/**
 * The configuration member of the library.
 * <br>
 * This list of <code>wa_chain</code> structure contain the configuration of
 * the WebApp Library in the form of all deployed <code>wa_application</code>,
 * <code>wa_virtualhost</code> and <code>wa_connection</code> structures.
 * <br>
 * The <code>curr</code> member in the <code>wa_chain</code> structure contains
 * always a <code>wa_virtualhost</code> structure, from which all configured
 * <code>wa_application</code> members and relative <code>wa_connection</code>
 * members can be retrieved.
 */
extern wa_chain *wa_configuration;

/**
 * A <b>NULL</b>-terminated array of all compiled-in <code>wa_provider</code>
 * members.
 */
extern wa_provider *wa_providers[];

#endif /* ifndef _WA_MAIN_H_ */
