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
 * @package Web-Server Plug-In
 * @author  Pier Fumagalli <mailto:pier.fumagalli@eng.sun.com>
 * @version $Id$
 */
#ifndef _WA_WEBSERVER_H_
#define _WA_WEBSERVER_H_

/**
 * The wa_callbacks structure contains function pointers for callbacks to the
 * web server.
 */
struct wa_webserver {
    /**
     * Log data on the web server log file.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param f The source file of this log entry.
     * @param l The line number within the source file of this log entry.
     * @param msg The message to be logged.
     */
    void (*log)(void *d, const char *f, int l, char *msg);

    /**
     * Read part of the request content.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param buf The buffer that will hold the data.
     * @param len The buffer length.
     * @return The number of bytes read, 0 on end of file or -1 on error.
     */
    int (*read)(void *d, char *buf, int len);

    /**
     * Set the HTTP response status code.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param status The HTTP status code for the response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*status)(void *d, int status);

    /**
     * Set the HTTP response mime content type.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param type The mime content type of the HTTP response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*ctype)(void *d, char *type);

    /**
     * Set an HTTP mime header.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param name The mime header name.
     * @param value The mime header value.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*header)(void *d, char *name, char *value);

    /**
     * Commit the first part of the response (status and headers).
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*commit)(void *d);

    /**
     * Write part of the response data back to the client.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @param buf The buffer containing the data to be written.
     * @param len The number of characters to be written.
     * @return The number of characters written to the client or -1 on error.
     */
    int (*write)(void *d, char *buf, int len);

    /**
     * Flush any unwritten response data to the client.
     *
     * @param d The web-server specific callback data specified in
     *          <code>wa_request-&gt;data</code>.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*flush)(void *d);
};

/**
 * Log data on the web server log file.
 *
 * @param r The wa_request structure associated with the current request.
 * @param f The source file of this log entry.
 * @param l The line number within the source file of this log entry.
 * @param msg The message to be logged.
 */
void wa_log(wa_request *r, const char *f, int l, char *msg);

/**
 * Read part of the request content.
 *
 * @param req The request member associated with this call.
 * @param buf The buffer that will hold the data.
 * @param len The buffer length.
 * @return The number of bytes read, 0 on end of file or -1 on error.
 */
int wa_read(wa_request *req, char *buf, int len);

/**
 * Set the HTTP response status code.
 *
 * @param req The request member associated with this call.
 * @param status The HTTP status code for the response.
 * @return TRUE on success, FALSE otherwise
 */
boolean wa_status(wa_request *req, int status);

/**
 * Set the HTTP response mime content type.
 *
 * @param req The request member associated with this call.
 * @param type The mime content type of the HTTP response.
 * @return TRUE on success, FALSE otherwise
 */
boolean wa_ctype(wa_request *req, char *type);

/**
 * Set an HTTP mime header.
 *
 * @param req The request member associated with this call.
 * @param name The mime header name.
 * @param value The mime header value.
 * @return TRUE on success, FALSE otherwise
 */
boolean wa_header(wa_request *req, char *name, char *value);

/**
 * Commit the first part of the response (status and headers).
 *
 * @param req The request member associated with this call.
 * @return TRUE on success, FALSE otherwise
 */
boolean wa_commit(wa_request *req);

/**
 * Write part of the response data back to the client.
 *
 * @param req The request member associated with this call.
 * @param buf The buffer containing the data to be written.
 * @param len The number of characters to be written.
 * @return The number of characters written to the client or -1 on error.
 */
int wa_write(wa_request *req, char *buf, int len);

/**
 * Flush any unwritten response data to the client.
 *
 * @param req The request member associated with this call.
 * @return TRUE on success, FALSE otherwise
 */
boolean wa_flush(wa_request *req);

#endif /* ifndef _WA_WEBSERVER_H_ */
