/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/* @version $Id$ */
#include "pr_warp.h"

wa_boolean c_check(wa_connection *conn, warp_packet *pack, apr_socket_t * sock) {
    warp_config *conf=(warp_config *)conn->conf;
    int maj=-1;
    int min=-1;
    int sid=-1;

    if (n_recv(sock,pack)!=wa_true) {
        wa_log(WA_MARK,"Cannot receive handshake WARP packet");
        return(wa_false);
    }

    if (pack->type!=TYPE_CONF_WELCOME) {
        wa_log(WA_MARK,"Invalid WARP packet %d (WELCOME)",pack->type);
        return(wa_false);
    }

    if (p_read_ushort(pack,&maj)!=wa_true) {
        wa_log(WA_MARK,"Cannot read major version");
        return(wa_false);
    }

    if (p_read_ushort(pack,&min)!=wa_true) {
        wa_log(WA_MARK,"Cannot read minor version");
        return(wa_false);
    }

    if ((maj!=VERS_MAJOR)||(min!=VERS_MINOR)) {
        wa_log(WA_MARK,"Invalid WARP protocol version %d.%d",maj,min);
        return(wa_false);
    }

    if (p_read_int(pack,&sid)!=wa_true) {
        wa_log(WA_MARK,"Cannot read server id");
        return(wa_false);
    }

#if APR_HAS_THREADS
    apr_atomic_set(&conf->serv, (unsigned) sid);
#else
    conf->serv = (unsigned) sid;
#endif
    
    wa_debug(WA_MARK,"Connection \"%s\" checked WARP/%d.%d (SERVER ID=%d)",
        conn->name,maj,min,conf->serv);
    return(wa_true);
}

wa_boolean c_configure(wa_connection *conn, apr_socket_t * sock) {
    wa_chain *elem=warp_applications;
    apr_pool_t *pool=NULL;
    wa_boolean ret=wa_false;
    warp_packet *pack=NULL;
    char *temp=NULL;

    if (apr_pool_create(&pool,wa_pool)!=APR_SUCCESS) {
        wa_log(WA_MARK,"Cannot create WARP temporary configuration pool");
        n_disconnect(conn, sock);
        return(wa_false);
    }

    if ((pack=p_create(wa_pool))==NULL) {
        wa_log(WA_MARK,"Cannot create WARP configuration packet");
        n_disconnect(conn, sock);
        apr_pool_destroy(pool);
        return(wa_false);
    }

    if ((ret=c_check(conn,pack, sock))==wa_false) n_disconnect(conn, sock);

    while (elem!=NULL) {
        wa_application *appl=(wa_application *)elem->curr;

        /* Check that the application really belongs to that connection */
        if (strcmp(appl->conn->name,conn->name)!=0) {
            elem=elem->next;
            continue;
        }

        wa_debug(WA_MARK,"Deploying \"%s\" via \"%s\" in \"http://%s:%d%s\"",
                appl->name,conn->name,appl->host->name,appl->host->port,
                appl->rpth);
        p_reset(pack);
        pack->type=TYPE_CONF_DEPLOY;
        p_write_string(pack,appl->name);
        p_write_string(pack,appl->host->name);
        p_write_ushort(pack,appl->host->port);
        p_write_string(pack,appl->rpth);
        n_send(sock,pack);

        if (n_recv(sock,pack)!=wa_true) {
            wa_log(WA_MARK,"Cannot read packet (%s:%d)",WA_MARK);
            n_disconnect(conn, sock);
            return(wa_false);
        }
        if (pack->type==TYPE_ERROR) {
            wa_log(WA_MARK,"Cannot deploy application %s",appl->name);
            elem=elem->next;
            continue;
        }
        if (pack->type!=TYPE_CONF_APPLIC) {
            wa_log(WA_MARK,"Unknown packet received (%d)",pack->type);
            p_reset(pack);
            pack->type=TYPE_FATAL;
            p_write_string(pack,"Invalid packet received");
            n_send(sock,pack);
            n_disconnect(conn, sock);
        }
        p_read_int(pack,(int *)&appl->conf);
        p_read_string(pack,&temp);
        appl->lpth=apr_pstrdup(wa_pool,temp);
        
        /* Check if this web-application is local or not by checking if its
           WEB-INF directory can be opened. */
        if (appl->lpth!=NULL) {
            apr_dir_t *dir=NULL;
            char *webinf=apr_pstrcat(wa_pool,appl->lpth,"/WEB-INF",NULL);
            if (apr_dir_open(&dir,webinf,wa_pool)==APR_SUCCESS) {
                if (dir!=NULL) apr_dir_close(dir);
                else appl->lpth=NULL;
            } else {
                appl->lpth=NULL;
            }
        }
        
        /* If this application is local, we want to retrieve the allowed and
           denied mapping list. */
        if (appl->lpth!=NULL) {
            p_reset(pack);
            pack->type=TYPE_CONF_MAP;
            p_write_int(pack,(int)appl->conf);
            n_send(sock,pack);
            
            while(1) {
                if (n_recv(sock,pack)!=wa_true) {
                    wa_log(WA_MARK,"Cannot read packet (%s:%d)",WA_MARK);
                    n_disconnect(conn,sock);
                    return(wa_false);
                }
                if (pack->type==TYPE_CONF_MAP_DONE) {
                    wa_debug(WA_MARK,"Done mapping URLs");
                    break;
                } else if (pack->type==TYPE_CONF_MAP_ALLOW) {
                    char *map=NULL;
                    p_read_string(pack,&map);
                    wa_debug(WA_MARK,"Allow URL mapping \"%s\"",map);
                } else if (pack->type==TYPE_CONF_MAP_DENY) {
                    char *map=NULL;
                    p_read_string(pack,&map);
                    wa_debug(WA_MARK,"Deny URL mapping \"%s\"",map);
                }
            }
        }

        if (appl->lpth==NULL) {
            wa_debug(WA_MARK,"Application \"%s\" deployed with id=%d (%s)",
                appl->name,appl->conf,"remote");
        } else {
            wa_debug(WA_MARK,"Application \"%s\" deployed with id=%d (%s)",
                appl->name,appl->conf,appl->lpth);
        }

        appl->depl=wa_true;
        elem=elem->next;
    }

    p_reset(pack);
    pack->type=TYPE_CONF_DONE;
    n_send(sock,pack);

    if (n_recv(sock,pack)!=wa_true) {
        wa_log(WA_MARK,"Cannot read packet (%s:%d)",WA_MARK);
        n_disconnect(conn,sock);
        return(wa_false);
    }
    if (pack->type!=TYPE_CONF_PROCEED) {
        wa_log(WA_MARK,"Cannot proceed on this connection");
        p_reset(pack);
        pack->type=TYPE_FATAL;
        p_write_string(pack,"Expected PROCEED packet not received");
        n_send(sock,pack);
        n_disconnect(conn,sock);
        return(wa_false);
    }

    apr_pool_destroy(pool);
    return(ret);
}
