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

#include <wa.h>

// Allocate memory while processing a request.
void *wa_callback_alloc(wa_callbacks *c, wa_request *r, int size) {
    return((*c->alloc)(r->data, size));
}

// Read part of the request content.
int wa_callback_read(wa_callbacks *c, wa_request *r, char *buf, int size) {
    return((*c->read)(r->data,buf,size));
}

// Set the HTTP response status code.
boolean wa_callback_setstatus(wa_callbacks *c, wa_request *r, int status) {
    return((*c->setstatus)(r->data,status));
}

// Set the HTTP response mime content type.
boolean wa_callback_settype(wa_callbacks *c, wa_request *r, char *type) {
    return((*c->settype)(r->data,type));
}

// Set an HTTP mime header.
boolean wa_callback_setheader(wa_callbacks *c,wa_request *r,char *n,char *v) {
    return((*c->setheader)(r->data,n,v));
}


// Commit the first part of the response (status and headers).
boolean wa_callback_commit(wa_callbacks *c, wa_request *r) {
    return((*c->commit)(r->data));
}

// Write part of the response data back to the client.
int wa_callback_write(wa_callbacks *c, wa_request *r, char *buf, int size) {
    return((*c->write)(r->data,buf,size));
}

// Write part of the response data back to the client.
int wa_callback_printf(wa_callbacks *c, wa_request *r, const char *fmt, ...) {
    va_list ap;
    char buf[1024];
    int ret;

    va_start(ap,fmt);
    ret=vsnprintf(buf,1024,fmt,ap);
    va_end(ap);

    if (ret<0) return(-1);

    return(wa_callback_write(c,r,buf,ret));
}

// Flush any unwritten response data to the client.
boolean wa_callback_flush(wa_callbacks *c, wa_request *r) {
    return((*c->flush)(r->data));
}

