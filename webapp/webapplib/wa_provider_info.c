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

static const char *wa_info_configure(wa_connection *conn, char *param) {
    if(conn==NULL) return("Connection not specified");
    if (param==NULL) conn->conf=strdup("[No data supplied]");
    else conn->conf=strdup(param);
    return(NULL);
}

static char *wa_info_describe(wa_connection *conn) {
    char buf[1024];

    if(conn==NULL) return("Null connection specified");
    sprintf(buf, "Extra parameters: %s", (char *)conn->conf);
    return(strdup(buf));
}

void wa_info_init(wa_connection *conn) {
}

void wa_info_destroy(wa_connection *conn) {
}

void wa_info_printf(wa_request *req, wa_callbacks *cb, const char *fmt, ...) {
    va_list ap;
    char buf[1024];
    int ret;
    int tmp;

    va_start(ap,fmt);
    ret=vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    if (ret<0) return;

    if ((tmp=(*cb->write)(req->data,buf,ret))!=ret) {
        (*cb->log)(req->data,WA_LOG,"Returned %d transmitted %d",tmp,ret);
    }
}

void wa_info_handle(wa_request *req, wa_callbacks *cb) {
    (*cb->setstatus)(req->data,200);
    (*cb->settype)(req->data,"text/html");
    (*cb->commit)(req->data);
    (*cb->write)(req->data,"<HTML><BODY>TEST</BODY></HTML>",30);
    (*cb->flush)(req->data);
}

wa_provider wa_provider_info = {
    "info",
    wa_info_configure,
    wa_info_init,
    wa_info_destroy,
    wa_info_describe,
    wa_info_handle,
};
