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
wa_host *wa_hosts=NULL;

/**
 * Create configuration for a new host.
 *
 * @param name The host primary name.
 * @param port The host primary port.
 * @return NULL or an error message.
 */
const char *wa_host_create(char *name, int port) {
    wa_host *host=NULL;
    wa_host *curr=NULL;

    // Check supplied parameters
    if (name==NULL) return("Host name unspecified");
    if ((port<1)||(port>65535)) return("Invalid port number");

    // Allocate wa_host structure and fill it
    host=(wa_host *)malloc(sizeof(wa_host));
    host->name=strdup(name);
    host->port=port;
    host->apps=NULL;
    host->next=NULL;

    // This is the first host we configure
    if (wa_hosts==NULL) {
        wa_hosts=host;
        return(NULL);
    }

    // We need to check for duplicate hosts
    curr=wa_hosts;
    while(curr!=NULL) {
        // Check for duplicate hosts definitions
        if((strcasecmp(curr->name, name)==0) && (curr->port==port)) {
            return("Host already configured");
        }
        // If this is the last configured host we found our way out
        if(curr->next==NULL) {
            curr->next=host;
            return(NULL);
        }
        // Process the next host in the chain
        curr=curr->next;
    }

    // Why are we here?
    return("Unknown error creating host configuration");
}

/**
 * Get the host configuration.
 *
 * @param name The host primary name.
 * @param port The host primary port.
 * @return The wa_host associated with the host or NULL.
 */
wa_host *wa_host_get(char *name, int port) {
    wa_host *curr=wa_hosts;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        if((strcasecmp(curr->name, name)==0) && (curr->port==port))
            return(curr);
        else curr=curr->next;
    }

    // No host found, sorry!
    return(NULL);
}

/**
 * Configure a web application for a specific host.
 *
 * @param host The wa_host structure of the host.
 * @param name The web application name.
 * @param path The web application root URI path.
 * @return NULL or an error message.
 */
const char *wa_host_setapp(wa_host *host, char *name, char *path,
                           wa_connection *conn) {
    wa_application *appl=NULL;
    wa_application *curr=NULL;
    int slashes=0;
    int pathlen=0;

    // Check the supplied parameters
    if(host==NULL) return("Host not specified");
    if(name==NULL) return("Web application name not specified");
    if(conn==NULL) return("Connection not specified");
    if(strlen(name)==0) return("Invalid web application name");
    if(path==NULL) return("Web application root path not specified");
    if((pathlen=strlen(path))==0) return("Invalid web application root path");

    // Create a new structure and put the name
    appl=(wa_application *)malloc(sizeof(wa_application));
    appl->name=strdup(name);
    appl->conn=conn;

    // Check for leading/trailing slashes. Set slashes to 1 if the leading
    // slash is missing, to 2 if the trailing one is missing or to 3 in case
    // both leading and trailing slashes are missing
    if(path[0]!='/') slashes+=1;
    if(path[pathlen-1]!='/') slashes+=2;

    // Copy the root path
    if (slashes==0) appl->path=strdup(path);
    if (slashes==1) {
        appl->path=(char *)malloc((pathlen+2)*sizeof(char));
        appl->path[0]='/';
        strncpy(appl->path+1,path,pathlen);
        appl->path[pathlen+1]='\0';
    }
    if (slashes==2) {
        appl->path=(char *)malloc((pathlen+2)*sizeof(char));
        strncpy(appl->path,path,pathlen);
        appl->path[pathlen]='/';
        appl->path[pathlen+1]='\0';
    }
    if (slashes==3) {
        appl->path=(char *)malloc((pathlen+3)*sizeof(char));
        appl->path[0]='/';
        strncpy(appl->path+1,path,pathlen);
        appl->path[pathlen+1]='/';
        appl->path[pathlen+2]='\0';
    }

    // Check if this is the first web application we configure
    if (host->apps==NULL) {
        host->apps=appl;
        return(NULL);
    }

    // We need to check all other webapps
    curr=host->apps;
    while(curr!=NULL) {
        // We don't check for web application names as the same web application
        // can be mounted under two different root paths. But we need to check
        // for different root paths.
        char *cpath=curr->path;
        char *npath=appl->path;
        if ((strstr(cpath,npath)==cpath)||(strstr(npath,cpath)==npath))
            return("Another web application uses the same root path");
        // If this is the last configured web application we found our way out
        if (curr->next==NULL) {
            curr->next=appl;
            return(NULL);
        }
        // Process the next web application
        curr=curr->next;
    }
    // Why are we here?
    return("Unknown error creating webapp configuration");
}


/**
 * Configure a web application for a specific host.
 *
 * @param h The host primary name.
 * @param p The host primary port.
 * @param name The web application name.
 * @param path The web application root URI path.
 * @return NULL or an error message.
 */
const char *wa_host_setapp_byname(char *h, int p, char *name, char *path,
                           wa_connection *conn) {
    wa_host *host=wa_host_get(h, p);

    if (host==NULL) return("Host not configured");
    return(wa_host_setapp(host, name, path, conn));
}

/**
 * Retrieve a web application for a specific host.
 *
 * @param host The wa_host structure of the host.
 * @param uri The URI to be me matched against web application root paths.
 * @return A wa_application structure pointer or NULL.
 */
wa_application *wa_host_findapp(wa_host *host, char *uri) {
    wa_application *appl=NULL;

    if (host==NULL) return(NULL);

    // Iterate thru the host web applications
    appl=host->apps;
    while(appl!=NULL) {
        if(strstr(uri,appl->path)==uri) return(appl);
        appl=appl->next;
    }

    // Nope, not found!
    return(NULL);
}

/**
 * Retrieve a web application for a specific host.
 *
 * @param h The host primary name.
 * @param p The host primary port.
 * @param uri The URI to be me matched against web application root paths.
 * @return A wa_application structure pointer or NULL.
 */
wa_application *wa_host_findapp_byname(char *h, int p, char *uri) {
    wa_host *host=wa_host_get(h, p);

    if (host==NULL) return(NULL);
    return(wa_host_findapp(host, uri));
}
