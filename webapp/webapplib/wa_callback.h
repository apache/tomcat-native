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

#ifndef _WA_CALLBACK_H_
#define _WA_CALLBACK_H_

/**
 * The wa_callbacks structure contains function pointers for callbacks to the
 * web server.
 */
struct wa_callback {
    /**
     * Add a component to the the current SERVER_SOFTWARE string and return the
     * new value.
     *
     * @param component The new component to add to the SERVER_SOFTWARE string
     *                  or NULL if no modification is necessary.
     * @return The updated SERVER_SOFTWARE string.
     */
    const char *(*serverinfo)(const char *component);

    /**
     * Log data on the web server log file.
     *
     * @param f The source file of this log entry.
     * @param l The line number within the source file of this log entry.
     * @param r The wa_request structure associated with the current request,
     *          or NULL.
     * @param msg The message to be logged.
     */
    void (*log)(const char *f, int l, wa_request *r, char *msg);

    /**
     * Allocate memory while processing a request. (The memory allocated by
     * this fuction must be released after the request is processed).
     *
     * @param req The request member associated with this call.
     * @param size The size in bytes of the memory to allocate.
     * @return A pointer to the allocated memory or NULL.
     */
    void *(*alloc)(wa_request *req, int size);

    /**
     * Read part of the request content.
     *
     * @param req The request member associated with this call.
     * @param buf The buffer that will hold the data.
     * @param len The buffer length.
     * @return The number of bytes read, 0 on end of file or -1 on error.
     */
    int (*read)(wa_request *req, char *buf, int len);

    /**
     * Set the HTTP response status code.
     *
     * @param req The request member associated with this call.
     * @param status The HTTP status code for the response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setstatus)(wa_request *req, int status);

    /**
     * Set the HTTP response mime content type.
     *
     * @param req The request member associated with this call.
     * @param type The mime content type of the HTTP response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*settype)(wa_request *req, char *type);

    /**
     * Set an HTTP mime header.
     *
     * @param req The request member associated with this call.
     * @param name The mime header name.
     * @param value The mime header value.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setheader)(wa_request *req, char *name, char *value);

    /**
     * Commit the first part of the response (status and headers).
     *
     * @param req The request member associated with this call.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*commit)(wa_request *req);

    /**
     * Write part of the response data back to the client.
     *
     * @param req The request member associated with this call.
     * @param buf The buffer containing the data to be written.
     * @param len The number of characters to be written.
     * @return The number of characters written to the client or -1 on error.
     */
    int (*write)(wa_request *req, char *buf, int len);

    /**
     * Flush any unwritten response data to the client.
     *
     * @param req The request member associated with this call.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*flush)(wa_request *req);
};

/* The list of configured hosts */
extern wa_callback *wa_callbacks;

/* Function prototype declaration */
// Add a component to the the current SERVER_SOFTWARE string.
const char *wa_callback_serverinfo(const char *);
// Log data on the web server log file.
void wa_callback_log(const char *, int, wa_request *, const char *, ...);
// Log debugging informations data on the web server log file.
void wa_callback_debug(const char * ,int, wa_request *, const char *, ...);
// Allocate memory while processing a request.
void *wa_callback_alloc(wa_request *, int);
// Read part of the request content.
int wa_callback_read(wa_request *, char *, int);
// Set the HTTP response status code.
boolean wa_callback_setstatus(wa_request *, int);
// Set the HTTP response mime content type.
boolean wa_callback_settype(wa_request *, char *);
// Set an HTTP mime header.
boolean wa_callback_setheader(wa_request *, char *, char *);
// Commit the first part of the response (status and headers).
boolean wa_callback_commit(wa_request *);
// Write part of the response data back to the client.
int wa_callback_write(wa_request *, char *, int);
// Flush any unwritten response data to the client.
boolean wa_callback_flush(wa_request *);
// Print a message back to the client using the printf standard.
int wa_callback_printf(wa_request *, const char *, ...);

#endif // ifdef _WA_CALLBACK_H_
