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
 * 4. The names  "The  Jakarta  Project",  "Jk",  and  "Apache  Software     *
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
 * Description: AJP14 Discovery handler
 * Author:      Henri Gomez <hgomez@slib.fr>
 * Version:     $Revision$                                          
 */

#include "jk_global.h"
#include "jk_pool.h"
#include "jk_msg_buff.h"
#include "jk_logger.h"
#include "jk_service.h"

#define CBASE_INC_SIZE   (8)    /* Allocate memory by step of 8 URIs : ie 8 URI by context */
#define URI_INC_SIZE (8)        /* Allocate memory by step of 8 CONTEXTs : ie 8 contexts by worker */

/** XXX XXX MERGE into jk_uriMap / jk_uriEnv */

typedef struct {
    char *      cbase;
    int         status;
    int         size;
    int         capacity;
    char **     uris;
} jk_context_item_t;


typedef struct {

    /*
     * Memory Pool
     */

    jk_pool_t       p;
    jk_pool_atom_t  buf[SMALL_POOL_SIZE];

	/*
	 * Virtual Server (if use)
	 */

	char *		            virtual;

    /*
     * Num of context handled (ie: examples, admin...)
     */

    int                     size;

    /*
     * Capacity
     */

    int                     capacity; 

    /*
     * Context list, context / URIs
     */

    jk_context_item_t **    contexts;
} 
jk_context_t;


/*
 * functions defined here 
 */

int context_set_virtual(jk_context_t *c, char *virtual);

int context_open(jk_context_t *c, char *virtual);

int context_free(jk_context_t **c);

jk_context_item_t *context_find_base(jk_context_t *c, char *cbase);

char *context_item_find_uri(jk_context_item_t *ci, char *uri);

void context_dump_uris(jk_context_t *c, char *cbase, FILE *f);

jk_context_item_t *context_add_base(jk_context_t *c, char *cbase);

int context_add_uri(jk_context_t *c, char *cbase, char *uri);

/*
 * Context Query (web server -> servlet engine), which URI are handled by servlet engine ?
 */
#define AJP14_CONTEXT_QRY_CMD	(unsigned char)0x15

/*
 * Context Info (servlet engine -> web server), URI handled response
 */
#define AJP14_CONTEXT_INFO_CMD	(unsigned char)0x16

/* 
 * Context Update (servlet engine -> web server), status of context changed
 */
#define AJP14_CONTEXT_UPDATE_CMD (unsigned char)0x17

/*
 * Context Status (web server -> servlet engine), what's
 * the status of the context ?
 */
#define AJP14_CONTEXT_STATE_CMD		(unsigned char)0x1C

/*
 * Context Status Reply (servlet engine -> web server), status of context
 */
#define AJP14_CONTEXT_STATE_REP_CMD	(unsigned char)0x1D

static int context_realloc(jk_context_t *c)
{
    if (c->size == c->capacity) {
        jk_context_item_t **contexts;
        int  capacity = c->capacity + CBASE_INC_SIZE;
        
        contexts = (jk_context_item_t **)jk_pool_alloc(&c->p, sizeof(jk_context_item_t *) * capacity);

        if (! contexts)
            return JK_FALSE;

        if (c->capacity && c->contexts)
            memcpy(contexts, c->contexts, sizeof(jk_context_item_t *) * c->capacity);

        c->contexts = contexts;
        c->capacity = capacity;
    }

    return JK_TRUE;
}


static int context_item_realloc(jk_context_t *c, jk_context_item_t *ci)
{
    if (ci->size == ci->capacity) {
            char **uris;
            int capacity = ci->capacity + URI_INC_SIZE;

            uris = (char **)jk_pool_alloc(&c->p, sizeof(char *) * capacity);

        if (! uris)
            return JK_FALSE;

        memcpy(uris, ci->uris, sizeof(char *) * ci->capacity);

        ci->uris     = uris;
        ci->capacity = capacity;
    }
    
    return JK_TRUE;
}

/*
 * Init the context info struct
 */

int context_open(jk_context_t *c, char *virtual)
{
    if (c) {
        jk_open_pool(&c->p, c->buf, sizeof(jk_pool_atom_t) * SMALL_POOL_SIZE);
        c->size  	= 0;
        c->capacity = 0;
        c->contexts = NULL;

        if( virtual ) {
            c->virtual=jk_pool_strdup(&c->p, virtual);
        }
        return JK_TRUE;
    }

    return JK_FALSE;
}


/*
 * AJP14 Autoconf Phase
 *
 * CONTEXT QUERY / REPLY
 */

#define MAX_URI_SIZE    512

static int handle_discovery(jk_endpoint_t  *ae,
                            jk_workerEnv_t *we,
                            jk_msg_buf_t    *msg,
                            jk_logger_t     *l)
{
    int                 cmd;
    int                 i,j;
    jk_login_service_t  *jl = ae->worker->login;
    jk_context_item_t   *ci;
    jk_context_t        *c;  
    char                *buf;

#ifndef TESTME

    ajp14_marshal_context_query_into_msgb(msg, we->virtual, l);
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery - send query\n");

    if (ajp_connection_tcp_send_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery - wait context reply\n");
    
    jk_b_reset(msg);
    
    if (ajp_connection_tcp_get_message(ae, msg, l) != JK_TRUE)
        return JK_FALSE;
    
    if ((cmd = jk_b_get_byte(msg)) != AJP14_CONTEXT_INFO_CMD) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14:discovery - awaited command %d, received %d\n",
               AJP14_CONTEXT_INFO_CMD, cmd);
        return JK_FALSE;
    }

    c=(jk_context_t *)malloc( sizeof(jk_context_t));
    if (context_open(c, we->virtual) != JK_TRUE) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14:discovery - can't allocate context room\n");
        return JK_FALSE;
    }
 
    if (ajp14_unmarshal_context_info(msg, c, l) != JK_TRUE) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14:discovery - can't get context reply\n");
        return JK_FALSE;
    }

    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery - received context\n");

    buf = malloc(MAX_URI_SIZE);      /* Really a very long URI */
    
    if (! buf) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp14:discovery - can't alloc buf\n");
        return JK_FALSE;
    }
    
    for (i = 0; i < c->size; i++) {
        ci = c->contexts[i];
        for (j = 0; j < ci->size; j++) {
            
#ifndef USE_SPRINTF
            snprintf(buf, MAX_URI_SIZE - 1, "/%s/%s", ci->cbase, ci->uris[j]);
#else
            sprintf(buf, "/%s/%s", ci->cbase, ci->uris[j]);
#endif
            
            l->jkLog(l, JK_LOG_INFO,
                   "Into ajp14:discovery "
                   "- worker %s will handle uri %s in context %s [%s]\n",
                   ae->worker->name, ci->uris[j], ci->cbase, buf);
            
/* XXX UPDATE             uri_worker_map_add(we->uri_to_worker, buf, ae->worker->name, l); */
        }
    }
    
    free(buf);
    jk_close_pool(&c->p);
    free(c);
    c = NULL;
#else 
    
    uri_worker_map_add(we->uri_to_worker,
                       "/examples/servlet/*",
                       ae->worker->name, l);
    uri_worker_map_add(we->uri_to_worker,
                       "/examples/*.jsp",
                       ae->worker->name, l);
    uri_worker_map_add(we->uri_to_worker,
                       "/examples/*.gif",
                       ae->worker->name, l);

#endif 
    return JK_TRUE;
}
 
int discovery(jk_endpoint_t *ae,
              jk_workerEnv_t *we,
              jk_logger_t    *l)
{
    jk_pool_t     *p = &ae->pool;
    jk_msg_buf_t  *msg;
    int           rc;

    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14:discovery\n");
    
    msg = jk_b_new(p);
    jk_b_set_buffer_size(msg, DEF_BUFFER_SZ);
    
    if ((rc = handle_discovery(ae, we, msg, l)) == JK_FALSE)
        ajp_close_endpoint(ae, l);

    return rc;
}

/* -------------------- private utils/marshaling -------------------- */

/*
 * Build the Context Query Cmd (autoconf)
 *
 * +--------------------------+---------------------------------+
 * | CONTEXT QRY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) |
 * +--------------------------+---------------------------------+
 *
 */
int ajp14_marshal_context_query_into_msgb(jk_msg_buf_t *msg,
                                          char         *virtual,
                                          jk_logger_t  *l)
{
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_marshal_context_query_into_msgb\n");
    
    /* To be on the safe side */
    jk_b_reset(msg);
    
    /*
     * CONTEXT QUERY CMD
     */
    if (jk_b_append_byte(msg, AJP14_CONTEXT_QRY_CMD))
        return JK_FALSE;
    
    /*
     * VIRTUAL HOST CSTRING
     */
    if (jk_b_append_string(msg, virtual)) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_marshal_context_query_into_msgb "
               "- Error appending the virtual host string\n");
        return JK_FALSE;
    }
    
    return JK_TRUE;
}


/*
 * Decode the Context Info Cmd (Autoconf)
 *
 * The Autoconf feature of AJP14, let us know which URL/URI could
 * be handled by the servlet-engine
 *
 * +---------------------------+---------------------------------+---------
 * | CONTEXT INFO CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | 
 * +---------------------------+---------------------------------+---------
 *
 *-------------------+-------------------------------+-----------+
 *CONTEXT NAME (CString (*)) | URL1 [\n] URL2 [\n] URL3 [\n] | NEXT CTX. |
 *-------------------+-------------------------------+-----------+
 */

int ajp14_unmarshal_context_info(jk_msg_buf_t *msg,
                                 jk_context_t *c,
                                 jk_logger_t  *l)
{
    char *vname;
    char *cname;
    char *uri;
    
    vname  = (char *)jk_b_get_string(msg);

    l->jkLog(l, JK_LOG_DEBUG,
           "ajp14_unmarshal_context_info - get virtual %s for virtual %s\n",
             vname, c->virtual);

    if (! vname) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_context_info "
               "- can't get virtual hostname\n");
        return JK_FALSE;
    }
    
    /* Check if we get the correct virtual host */
    if (c->virtual != NULL && 
	vname != NULL &&
	strcmp(c->virtual, vname)) {
        /* set the virtual name, better to add to a virtual list ? */

        if( vname != NULL ) {
            c->virtual=  jk_pool_strdup(&c->p, vname);
        }
    }
    
    for (;;) {
        
        cname  = (char *)jk_b_get_string(msg); 

        if (! cname) {
            l->jkLog(l, JK_LOG_ERROR,
                   "Error ajp14_unmarshal_context_info - can't get context\n");
            return JK_FALSE;
        }   
        
        l->jkLog(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_info "
               "- get context %s for virtual %s\n", cname, vname);
        
        /* grab all contexts up to empty one which indicate end of contexts */
        if (! strlen(cname)) 
            break;

        /* create new context base (if needed) */
        
        if (context_add_base(c, cname) == JK_FALSE) {
            l->jkLog(l, JK_LOG_ERROR, "Error ajp14_unmarshal_context_info"
                   "- can't add/set context %s\n", cname);
            return JK_FALSE;
        }
        
        for (;;) {
            
            uri  = (char *)jk_b_get_string(msg);
            
            if (!uri) {
                l->jkLog(l, JK_LOG_ERROR,
                       "Error ajp14_unmarshal_context_info - can't get URI\n");
                return JK_FALSE;
            }
            
            if (! strlen(uri)) {
                l->jkLog(l, JK_LOG_DEBUG, "No more URI for context %s", cname);
                break;
            }
            
            l->jkLog(l, JK_LOG_INFO,
                   "Got URI (%s) for virtualhost %s and context %s\n",
                   uri, vname, cname);
            
            if (context_add_uri(c, cname, uri) == JK_FALSE) {
                l->jkLog(l, JK_LOG_ERROR,
                       "Error ajp14_unmarshal_context_info - "
                       "can't add/set uri (%s) for context %s\n", uri, cname);
                return JK_FALSE;
            } 
        }
    }
    return JK_TRUE;
}


/*
 * Build the Context State Query Cmd
 *
 * We send the list of contexts where we want to know state,
 * empty string end context list*
 * If cname is set, only ask about THIS context
 *
 * +----------------------------+----------------------------------+----------
 * | CONTEXT STATE CMD (1 byte) |  VIRTUAL HOST NAME  | CONTEXT NAME
 *                              |   (CString (*))     |  (CString (*)) 
 * +----------------------------+----------------------------------+----------
 *
 */
int ajp14_marshal_context_state_into_msgb(jk_msg_buf_t *msg,
                                          jk_context_t *c,
                                          char         *cname,
                                          jk_logger_t  *l)
{
    jk_context_item_t *ci;
    int                i;
    
    l->jkLog(l, JK_LOG_DEBUG, "Into ajp14_marshal_context_state_into_msgb\n");
    
    /* To be on the safe side */
    jk_b_reset(msg);
    
    /*
     * CONTEXT STATE CMD
     */
    if (jk_b_append_byte(msg, AJP14_CONTEXT_STATE_CMD))
        return JK_FALSE;
    
    /*
     * VIRTUAL HOST CSTRING
     */
     if (jk_b_append_string(msg, c->virtual)) {
        l->jkLog(l, JK_LOG_ERROR, "Error ajp14_marshal_context_state_into_msgb"
               "- Error appending the virtual host string\n");
        return JK_FALSE;
     }
     
     if (cname) {
         
         ci = context_find_base(c, cname);
         
         if (! ci) {
             l->jkLog(l, JK_LOG_ERROR,
                    "Warning ajp14_marshal_context_state_into_msgb"
                    "- unknown context %s\n", cname);
            return JK_FALSE;
         }
         
         /*
          * CONTEXT CSTRING
          */
         
         if (jk_b_append_string(msg, cname )) {
            l->jkLog(l, JK_LOG_ERROR,
                   "Error ajp14_marshal_context_state_into_msgb"
                   "- Error appending the context string %s\n", cname);
            return JK_FALSE;
         }
     } else { /* Grab all contexts name */
         for (i = 0; i < c->size; i++) {
            /*
             * CONTEXT CSTRING
             */
             if (jk_b_append_string(msg, c->contexts[i]->cbase )) {
                 l->jkLog(l, JK_LOG_ERROR,
                        "Error ajp14_marshal_context_state_into_msgb "
                        "- Error appending the context string\n");
                 return JK_FALSE;
             }
         }
     }
     
     /* End of context list, an empty string */ 
     
     if (jk_b_append_string(msg, "")) {
         l->jkLog(l, JK_LOG_ERROR,
                "Error ajp14_marshal_context_state_into_msgb "
                "- Error appending end of contexts\n");
         return JK_FALSE;
     }
     
     return JK_TRUE;
}


/*
 * Decode the Context State Reply Cmd
 *
 * We get update of contexts list, empty string end context list*
 *
 * +----------------------------------+---------------------------------+----
 * | CONTEXT STATE REPLY CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | 
 * +----------------------------------+---------------------------------+----
 *
 *------------------------+------------------+----+
 *CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) | .. |
 * ------------------------+------------------+----+
 */
int ajp14_unmarshal_context_state_reply(jk_msg_buf_t *msg,
                                        jk_context_t *c,
                                        jk_logger_t  *l)
{
    char                *vname;
    char                *cname;
    jk_context_item_t   *ci;

    /* get virtual name */
    vname  = (char *)jk_b_get_string(msg);
    
    if (! vname) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_context_state_reply "
               "- can't get virtual hostname\n");
        return JK_FALSE;
    }
    
    /* Check if we speak about the correct virtual */
    if (strcmp(c->virtual, vname)) {
        l->jkLog(l, JK_LOG_ERROR,
               "Error ajp14_unmarshal_context_state_reply"
               "- incorrect virtual %s instead of %s\n",
               vname, c->virtual);
        return JK_FALSE;
    }
    
    for (;;) {
        /* get context name */
        cname  = (char *)jk_b_get_string(msg);
        
        if (! cname) {
            l->jkLog(l, JK_LOG_ERROR,
                   "Error ajp14_unmarshal_context_state_reply"
                   "- can't get context\n");
            return JK_FALSE;
        }	
        
        if (! strlen(cname))
            break;
        
        ci = context_find_base(c, cname);
        
        if (! ci) {
            l->jkLog(l, JK_LOG_ERROR,
                   "Error ajp14_unmarshal_context_state_reply "
                   "- unknow context %s for virtual %s\n", 
                   cname, vname);
            return JK_FALSE;
        }
        
        ci->status = jk_b_get_int(msg);
        
        l->jkLog(l, JK_LOG_DEBUG, "ajp14_unmarshal_context_state_reply "
               "- updated context %s to state %d\n", cname, ci->status);
    }
    
    return JK_TRUE;
}

/*
 * Decode the Context Update Cmd
 * 
 * +-----------------------------+---------------------------------+------
 * | CONTEXT UPDATE CMD (1 byte) | VIRTUAL HOST NAME (CString (*)) | 
 * +-----------------------------+---------------------------------+------
 *
 * ----------------------+------------------+
 * CONTEXT NAME (CString (*)) | UP/DOWN (1 byte) |
 * ----------------------+------------------+
 * 
 */
int ajp14_unmarshal_context_update_cmd(jk_msg_buf_t *msg,
                                       jk_context_t *c,
                                       jk_logger_t  *l)
{
    return (ajp14_unmarshal_context_state_reply(msg, c, l));
}

 /*
 * Set the virtual name of the context 
 */

int context_set_virtual(jk_context_t *c, char *virtual)
{
    if (c) {

        if (virtual) {
            c->virtual = jk_pool_strdup(&c->p, virtual);

            if (! c->virtual)
                return JK_FALSE;
        }

        return JK_TRUE;
    }

    return JK_FALSE;
}

/*
 * Locate a context base in context list
 */

jk_context_item_t * context_find_base(jk_context_t *c, char *cbase)
{
    int                 i;
    jk_context_item_t * ci;

    if (! c || ! cbase)
        return NULL;

    for (i = 0 ; i < c->size ; i++) {
        
        ci = c->contexts[i];

        if (! ci)
            continue;

        if (! strcmp(ci->cbase, cbase))
            return ci;
    }

    return NULL;
}

/*
 * Locate an URI in a context item
 */

char * context_item_find_uri(jk_context_item_t *ci, char *uri)
{
    int i;

    if (! ci || ! uri)
        return NULL;

    for (i = 0 ; i < ci->size ; i++) {
        if (! strcmp(ci->uris[i], uri))
            return ci->uris[i];
    }

    return NULL;
}

void context_dump_uris(jk_context_t *c, char *cbase, FILE * f)
{
    jk_context_item_t * ci;
    int                 i;

    ci = context_find_base(c, cbase);

    if (! ci)
        return;

    for (i = 0; i < ci->size; i++)
        fprintf(f, "/%s/%s\n", ci->cbase, ci->uris[i]);

    fflush(f); 
}


/*
 * Add a new context item to context
 */

jk_context_item_t * context_add_base(jk_context_t *c, char *cbase)
{
    jk_context_item_t *  ci;

    if (! c || !cbase)
        return NULL;

    /* Check if the context base was not allready created */
    ci = context_find_base(c, cbase);

    if (ci)
        return ci;

    if (context_realloc(c) != JK_TRUE)
        return NULL;

    ci = (jk_context_item_t *)jk_pool_alloc(&c->p, sizeof(jk_context_item_t));

    if (! ci)
        return NULL;

    c->contexts[c->size] = ci;
    c->size++;
    ci->cbase       = jk_pool_strdup(&c->p, cbase);
    ci->status      = 0;
    ci->size        = 0;
    ci->capacity    = 0;
    ci->uris        = NULL;

    return ci;
}

/*
 * Add a new URI to a context item
 */

int context_add_uri(jk_context_t *c, char *cbase, char * uri)
{
    jk_context_item_t *  ci;

    if (! uri)
        return JK_FALSE;

    /* Get/Create the context base */
    ci = context_add_base(c, cbase);
        
    if (! ci)
        return JK_FALSE;

    if (context_item_find_uri(ci, uri) != NULL)
        return JK_TRUE;

    if (context_item_realloc(c, ci) == JK_FALSE)
        return JK_FALSE;

    ci->uris[ci->size] = jk_pool_strdup(&c->p, uri);

    if (ci->uris[ci->size] == NULL)
        return JK_FALSE;

    ci->size++;
    return JK_TRUE;
}

