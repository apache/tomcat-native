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
struct wa_callbacks {
    /**
     * Log data on the web server log file.
     *
     * @param file The source file of this log entry.
     * @param line The line number within the source file of this log entry.
     * @param data The web-server specific data (wa_request->data).
     * @param fmt The format string (printf style).
     * @param ... All other parameters (if any) depending on the format.
     */
    void (*log)(void *data, const char *file, int line, const char *fmt, ...);

    /**
     * Allocate memory while processing a request.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param size The size in bytes of the memory to allocate.
     * @return A pointer to the allocated memory or NULL.
     */
    void *(*alloc)(void *data, int size);

    /**
     * Read part of the request content.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param buf The buffer that will hold the data.
     * @param len The buffer length.
     * @return The number of bytes read, 0 on end of file or -1 on error.
     */
    int (*read)(void *data, char *buf, int len);

    /**
     * Set the HTTP response status code.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param status The HTTP status code for the response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setstatus)(void *data, int status);

    /**
     * Set the HTTP response mime content type.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param type The mime content type of the HTTP response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*settype)(void *data, char *type);

    /**
     * Set an HTTP mime header.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param name The mime header name.
     * @param value The mime header value.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setheader)(void *data, char *name, char *value);

    /**
     * Commit the first part of the response (status and headers).
     *
     * @param data The web-server specific data (wa_request->data).
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*commit)(void *data);

    /**
     * Write part of the response data back to the client.
     *
     * @param buf The buffer holding the data to be written.
     * @param len The number of characters to be written.
     * @return The number of characters written to the client or -1 on error.
     */
    int (*write)(void *data, char *buf, int len);

    /**
     * Flush any unwritten response data to the client.
     *
     * @param data The web-server specific data (wa_request->data).
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*flush)(void *);
};

/* Function prototype declaration */
// Allocate memory while processing a request.
void *wa_callback_alloc(wa_callbacks *, wa_request *, int);
// Read part of the request content.
int wa_callback_read(wa_callbacks *, wa_request *, char *, int);
// Set the HTTP response status code.
boolean wa_callback_setstatus(wa_callbacks *, wa_request *, int);
// Set the HTTP response mime content type.
boolean wa_callback_settype(wa_callbacks *, wa_request *, char *);
// Set an HTTP mime header.
boolean wa_callback_setheader(wa_callbacks *, wa_request *, char *, char *);
// Commit the first part of the response (status and headers).
boolean wa_callback_commit(wa_callbacks *, wa_request *);
// Write part of the response data back to the client.
int wa_callback_write(wa_callbacks *, wa_request *, char *, int);
// Write part of the response data back to the client.
int wa_callback_printf(wa_callbacks *, wa_request *, const char *, ...);
// Flush any unwritten response data to the client.
boolean wa_callback_flush(wa_callbacks *, wa_request *);

#endif // ifdef _WA_CALLBACK_H_
