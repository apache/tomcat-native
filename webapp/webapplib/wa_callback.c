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



/* The list of configured hosts */

wa_callback *wa_callbacks=NULL;



/**

 * Add a component to the the current SERVER_SOFTWARE string and return the

 * new value.

 *

 * @param component The new component to add to the SERVER_SOFTWARE string

 *                  or NULL if no modification is necessary.

 * @return The updated SERVER_SOFTWARE string.

 */

const char *wa_callback_serverinfo(const char *component) {

    if (wa_callbacks==NULL) return("Unknown");

    return((*wa_callbacks->serverinfo)(component));

}



/**

 * Log data on the web server log file.

 *

 * @param f The source file of this log entry.

 * @param l The line number within the source file of this log entry.

 * @param r The wa_request structure associated with the current request,

 *          or NULL.

 * @param fmt The format string (printf style).

 * @param ... All other parameters (if any) depending on the format.

 */

void wa_callback_log(const char *f,int l,wa_request *r,const char *fmt,...) {

    va_list ap;

    char buf[1024];

    int ret;



    if (wa_callbacks==NULL) return;



    va_start(ap,fmt);

    ret=vsnprintf(buf,1024,fmt,ap);

    if (ret<1) {

        (*wa_callbacks->log)(WA_LOG,r,"Cannot format log message");

    } else if (ret>1023) {

        (*wa_callbacks->log)(WA_LOG,r,"Log message too long");

    } else {

        (*wa_callbacks->log)(f,l,r,buf);

    }

    va_end(ap);

}



/**

 * Log debugging informations data on the web server log file.

 *

 * @param f The source file of this log entry.

 * @param l The line number within the source file of this log entry.

 * @param r The wa_request structure associated with the current request,

 *          or NULL.

 * @param fmt The format string (printf style).

 * @param ... All other parameters (if any) depending on the format.

 */

void wa_callback_debug(const char *f,int l,wa_request *r,const char *fmt,...) {

#ifdef DEBUG

    va_list ap;

    char buf[1024];

    int ret;



    if (wa_callbacks==NULL) return;



    va_start(ap,fmt);

    ret=vsnprintf(buf,1024,fmt,ap);

    if (ret<1) {

        wa_callback_log(WA_LOG,r,"[%s:%d] Cannot format log message",WA_LOG);

    } else if (ret>1023) {

        wa_callback_log(WA_LOG,r,"[%s:%d] Log message too long",WA_LOG);

    } else {

        wa_callback_log(f,l,r,"[%s:%d] %s",f,l,buf);

    }

    va_end(ap);

#endif

}



/**

 * Allocate memory while processing a request. (The memory allocated by

 * this fuction must be released after the request is processed).

 *

 * @param r The request member associated with this call.

 * @param size The size in bytes of the memory to allocate.

 * @return A pointer to the allocated memory or NULL.

 */

void *wa_callback_alloc(wa_request *r, int size) {

    if (wa_callbacks==NULL) return(NULL);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(NULL);

    }

    return((*wa_callbacks->alloc)(r, size));

}



/**

 * Read part of the request content.

 *

 * @param r The request member associated with this call.

 * @param buf The buffer that will hold the data.

 * @param size The buffer length.

 * @return The number of bytes read, 0 on end of file or -1 on error.

 */

int wa_callback_read(wa_request *r, char *buf, int size) {

    if (wa_callbacks==NULL) return(-1);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(-1);

    }

    return((*wa_callbacks->read)(r,buf,size));

}



/**

 * Set the HTTP response status code.

 *

 * @param r The request member associated with this call.

 * @param status The HTTP status code for the response.

 * @return TRUE on success, FALSE otherwise

 */

boolean wa_callback_setstatus(wa_request *r, int status) {

    if (wa_callbacks==NULL) return(FALSE);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }

    return((*wa_callbacks->setstatus)(r,status));

}



/**

 * Set the HTTP response mime content type.

 *

 * @param r The request member associated with this call.

 * @param type The mime content type of the HTTP response.

 * @return TRUE on success, FALSE otherwise

 */

boolean wa_callback_settype(wa_request *r, char *type) {

    if (wa_callbacks==NULL) return(FALSE);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }

    return((*wa_callbacks->settype)(r,type));

}



/**

 * Set an HTTP mime header.

 *

 * @param r The request member associated with this call.

 * @param n The mime header name.

 * @param v The mime header value.

 * @return TRUE on success, FALSE otherwise

 */

boolean wa_callback_setheader(wa_request *r, char *n, char *v) {

    if (wa_callbacks==NULL) return(FALSE);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }

    return((*wa_callbacks->setheader)(r,n,v));

}





/**

 * Commit the first part of the response (status and headers).

 *

 * @param r The request member associated with this call.

 * @return TRUE on success, FALSE otherwise

 */

boolean wa_callback_commit(wa_request *r) {

    if (wa_callbacks==NULL) return(FALSE);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }

    return((*wa_callbacks->commit)(r));

}



/**

 * Write part of the response data back to the client.

 *

 * @param r The request member associated with this call.

 * @param buf The buffer containing the data to be written.

 * @param len The number of characters to be written.

 * @return The number of characters written to the client or -1 on error.

 */

int wa_callback_write(wa_request *r, char *buf, int size) {

    if (wa_callbacks==NULL) return(-1);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(-1);

    }

    return((*wa_callbacks->write)(r,buf,size));

}



/**

 * Flush any unwritten response data to the client.

 *

 * @param r The request member associated with this call.

 * @return TRUE on success, FALSE otherwise

 */

boolean wa_callback_flush(wa_request *r) {

    if (wa_callbacks==NULL) return(FALSE);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }

    return((*wa_callbacks->flush)(r));

}



/**

 * Print a message back to the client using the printf standard.

 *

 * @param r The request member associated with this call.

 * @param fmt The format string (printf style).

 * @param ... All other parameters (if any) depending on the format.

 * @return The number of characters written to the client or -1 on error.

 */

int wa_callback_printf(wa_request *r, const char *fmt, ...) {

    va_list ap;

    char *buf=NULL;

    int ret;



    if (wa_callbacks==NULL) return(-1);

    if (r==NULL) {

        wa_callback_debug(WA_LOG,r,"Null wa_request member specified");

        return(FALSE);

    }



    // Allocate some data for the buffer

    buf=(char *)(*wa_callbacks->alloc)(r,1024*sizeof(char));

    if (buf==NULL) {

        wa_callback_debug(WA_LOG,r,"Cannot allocate buffer");

        return(-1);

    }



    // Check if we can write some data.

    if (wa_callbacks==NULL) {

        return(-1);

    }



    // Try to fill our buffer with printf data

    va_start(ap,fmt);

    ret=vsnprintf(buf,1024,fmt,ap);



    // If vsnprintf returned null, we weren't able to format the message

    if (ret<0) {

        wa_callback_log(WA_LOG,r,"Cannot format message");

        va_end(ap);

        return(-1);



    // If vsnprintf is greater than 1024 the buffer was too small

    } else if (ret>1024) {

        // Reallocate the buffer for the bigger message and check if we were

        // able to print the message.

        buf=(char *)(*wa_callbacks->alloc)(r,(ret+1)*sizeof(char));

        if (buf==NULL) {

            va_end(ap);

            wa_callback_debug(WA_LOG,r,"Cannot allocate buffer");

            return(-1);

        }

        ret=vsnprintf(buf,ret+1,fmt,ap);

        if (ret<0) {

            wa_callback_log(WA_LOG,r,"Cannot format message");

            va_end(ap);

            return(-1);

        }

    }



    va_end(ap);

    return(wa_callback_write(r,buf,ret));

}

