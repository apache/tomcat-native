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

/* The list of all available connections */
wa_connection *wa_connections=NULL;

/**
 * Create a new connection.
 *
 * @param name The connection unique name.
 * @param prov The provider name (must be compiled in).
 * @param parm An extra configuration parameter (might be null).
 * @return NULL or a string describing te error message.
 */
const char *wa_connection_create(char *name, char *prov, char *parm) {
    const char *mesg=NULL;
    wa_connection *conn=NULL;
    wa_connection *curr=NULL;
    wa_provider *p=NULL;

    // Check basic parameters
    if (name==NULL) return("Connection name not specified");
    if (prov==NULL) return("Connection provider not specified");

    // Try to retrieve the provider by name
    if ((p=wa_provider_get(prov))==NULL) return("Provider not found");

    // Allocate connection structure and set basic values
    conn=(wa_connection *)malloc(sizeof(struct wa_connection));
    conn->name=strdup(name);
    conn->prov=p;
    conn->next=NULL;

    // Call the provider specific configuration function
    mesg=((*p->configure)(conn,parm));
    if (mesg!=NULL) return(mesg);

    // Store this connection in our configurations
    if (wa_connections==NULL) {
        wa_connections=conn;
        return(NULL);
    }

    // Iterate thru the list of connections
    curr=wa_connections;
    while(curr!=NULL) {
        if (strcasecmp(curr->name,name)==0)
            return("Duplicate connection name");
        if (curr->next==NULL) {
            curr->next=conn;
            return(NULL);
        }
        curr=curr->next;
    }

    // Why the hack are we here?
    return("Unknown error trying to create connection");
}

/**
 * Get a specific webapp connection.
 *
 * @param name The connection name.
 * @return The wa_connection associated with the name or NULL.
 */
wa_connection *wa_connection_get(char *name) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        if (strcasecmp(curr->name,name)==0) return(curr);
        else curr=curr->next;
    }

    // No host found, sorry!
    return(NULL);
}

/**
 * Initialize all configured connections.
 */
void wa_connection_init(void) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        (*curr->prov->init)(curr);
        curr=curr->next;
    }
}

/**
 * Initialize all configured connections.
 */
void wa_connection_destroy(void) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        (*curr->prov->destroy)(curr);
        curr=curr->next;
    }
}

