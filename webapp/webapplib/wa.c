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



/**

 * Initialize the webapp library and all connections.

 *

 * @param cb The wa_callback structure specified by the web server.

 */

void wa_init(wa_callback *cb) {

    wa_connection *c=wa_connections;



    // Set our callbacks structure

    if (cb==NULL) return;

    wa_callbacks=cb;



    // Set the SERVER_SOFTWARE string.

    wa_callback_debug(WA_LOG,NULL,"Initializing WebApp library");

    wa_callback_serverinfo(WA_NAME "/" WA_VERSION);



    // Iterate thru our connections chain

    while(c!=NULL) {

        wa_callback_debug(WA_LOG,NULL,"Initializing connection \"%s\"",c->name);

        (*c->prov->init)(c);

        wa_callback_debug(WA_LOG,NULL,"Connection \"%s\" initialized",c->name);

        c=c->next;

    }

    wa_callback_debug(WA_LOG,NULL,"WebApp library initialized");

    return;

}



/**

 * Reset the webapp library and all connections.

 */

void wa_destroy(void) {

    wa_connection *c=wa_connections;



    // Iterate thru our hosts chain

    while(c!=NULL) {

        (*c->prov->destroy)(c);

        wa_callback_debug(WA_LOG,NULL,"Connection \"%s\" destroyed",c->name);

        c=c->next;

    }



    wa_callbacks=NULL;

}

