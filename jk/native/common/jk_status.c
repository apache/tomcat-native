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

/***************************************************************************
 * Description: Status worker, display and manages JK workers              *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_status.h"
#include "jk_mt.h"
#include "jk_shm.h"
#include "jk_ajp_common.h"
#include "jk_lb_worker.h"
#include "jk_ajp13_worker.h"
#include "jk_ajp14_worker.h"
#include "jk_connect.h"
#include "jk_uri_worker_map.h"

#define HUGE_BUFFER_SIZE (8*1024)

#define JK_STATUS_HEAD "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                       "DTD HTML 3.2 Final//EN\">\n"      \
                       "<html><head><title>JK Status Manager</title>"

#define JK_STATUS_HEND "</head>\n<body>\n"
#define JK_STATUS_BEND "</body>\n</html>\n"

#define JK_STATUS_XMLH "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"          \
                       "<jk:status xmlns:jk=\"http://jakarta.apache.org\">\n"

#define JK_STATUS_XMLE "</jk:status>\n"

#define JK_STATUS_TEXTUPDATE_RESPONCE "OK - jk status worker updated\n"

typedef struct status_worker status_worker_t;

struct status_endpoint
{
    jk_endpoint_t   *e;
    status_worker_t *s_worker;
    jk_endpoint_t   endpoint;
};

typedef struct status_endpoint status_endpoint_t;

struct status_worker
{
    jk_pool_t         p;
    jk_pool_atom_t    buf[TINY_POOL_SIZE];
    const char        *name;
    const char        *css;
    jk_worker_t       worker;
    status_endpoint_t ep;
    jk_worker_env_t   *we;
};

static const char *worker_type[] = {
    "unknown",
    "ajp12",
    "ajp13",
    "ajp14",
    "jni",
    "lb",
    "status",
    NULL
};

static const char *headers_names[] = {
    "Content-Type",
    "Cache-Control",
    "Pragma",
    NULL
};

#define HEADERS_NO_CACHE "no-cache", "no-cache", NULL

static const char *headers_vhtml[] = {
    "text/html",
    HEADERS_NO_CACHE
};

static const char *headers_vxml[] = {
    "text/xml",
    HEADERS_NO_CACHE
};

static const char *headers_vtxt[] = {
    "text/plain",
    HEADERS_NO_CACHE
};

#if !defined(HAVE_VSNPRINTF) && !defined(HAVE_APR)
static FILE *f = NULL;
static int vsnprintf(char *str, size_t n, const char *fmt, va_list ap)
{
    int res;

    if (f == NULL)
        f = fopen("/dev/null", "w");
    if (f == NULL)
        return -1;

    setvbuf(f, str, _IOFBF, n);

    res = vfprintf(f, fmt, ap);

    if (res > 0 && res < n) {
        res = vsprintf(str, fmt, ap);
    }
    return res;
}
#endif

int jk_printf(jk_ws_service_t *s, const char *fmt, ...)
{
    int rc = 0;
    va_list args;
#ifdef NETWARE
/* On NetWare, this can get called on a thread that has a limited stack so */
/* we will allocate and free the temporary buffer in this function         */
        char *buf;
#else
        char buf[HUGE_BUFFER_SIZE];
#endif

    if (!s || !fmt) {
        return -1;
    }
    va_start(args, fmt);

#ifdef NETWARE
        buf = (char *)malloc(HUGE_BUFFER_SIZE);
        if (NULL == buf)
            return -1;
#endif
#ifdef USE_VSPRINTF             /* until we get a vsnprintf function */
    rc = vsprintf(buf, fmt, args);
#else
    rc = vsnprintf(buf, HUGE_BUFFER_SIZE, fmt, args);
#endif
    va_end(args);
    if (rc > 0)
        s->write(s, buf, rc);
#ifdef NETWARE
        free(buf);
#endif
    return rc;
}

/* Actually APR's apr_strfsize */
char *status_strfsize(size_t size, char *buf)
{
    const char ord[] = "KMGTPE";
    const char *o = ord;
    int remain;

    if (size < 0) {
        return strcpy(buf, "  - ");
    }
    if (size < 973) {
        if (sprintf(buf, "%3d ", (int) size) < 0)
            return strcpy(buf, "****");
        return buf;
    }
    do {
        remain = (int)(size & 1023);
        size >>= 10;
        if (size >= 973) {
            ++o;
            continue;
        }
        if (size < 9 || (size == 9 && remain < 973)) {
            if ((remain = ((remain * 5) + 256) / 512) >= 10)
                ++size, remain = 0;
            if (sprintf(buf, "%d.%d%c", (int) size, remain, *o) < 0)
                return strcpy(buf, "****");
            return buf;
        }
        if (remain >= 512)
            ++size;
        if (sprintf(buf, "%3d%c", (int) size, *o) < 0)
            return strcpy(buf, "****");
        return buf;
    } while (1);
}

static const char *status_worker_type(int t)
{
    if (t < 0 || t > 6)
        t = 0;
    return worker_type[t];
}


static const char *status_val_bool(int v)
{
    if (v == 0)
        return "False";
    else
        return "True";
}

static const char *status_val_status(int d, int e, int r, int b)
{
    if (d)
        return "Disabled";
    else if (r)
        return "Recovering";
    else if (e)
        return "Error";
    else if (b)
        return "Busy";
    else
        return "OK";
}

static const char *status_val_match(unsigned int match)
{
    if (match & MATCH_TYPE_DISABLED)
        return "Disabled";
    else if (match & MATCH_TYPE_NO_MATCH)
        return "Unmount";
    else if (match & MATCH_TYPE_EXACT)
        return "Exact";
    else if (match & MATCH_TYPE_CONTEXT)
        return "Context";
    else if (match & MATCH_TYPE_CONTEXT_PATH)
        return "Context Path";
    else if (match & MATCH_TYPE_SUFFIX)
        return "Suffix";
    else if (match & MATCH_TYPE_GENERAL_SUFFIX)
        return "General Suffix";
    else if (match & MATCH_TYPE_WILDCHAR_PATH)
        return "Wildchar";
    else
        return "Error";
}

static void jk_puts(jk_ws_service_t *s, const char *str)
{
    if (str)
        s->write(s, str, strlen(str));
    else
        s->write(s, "(null)", 6);
}

static void jk_putv(jk_ws_service_t *s, ...)
{
    va_list va;
    const char *str;

    va_start(va, s);
    while (1) {
        str = va_arg(va, const char *);
        if (str == NULL)
            break;
        s->write(s, str, strlen(str));
    }
    va_end(va);
}

static const char *status_cmd(const char *param, const char *req, char *buf, size_t len)
{
    char ps[32];
    char *p;
    size_t l = 0;

    *buf = '\0';
    if (!req)
        return NULL;
    sprintf(ps, "&%s=", param);
    p = strstr(req, ps);
    if (p) {
        p += strlen(ps);
        while (*p) {
            if (*p != '&')
                buf[l++] = *p;
            else
                break;
            if (l + 2 > len)
                break;
            p++;
        }
        buf[l] = '\0';
        if (l)
            return buf;
        else
            return NULL;
    }
    else
        return NULL;
}

static int status_int(const char *param, const char *req, int def)
{
    const char *v;
    char buf[32];
    int rv = def;

    if ((v = status_cmd(param, req, buf, sizeof(buf) -1))) {
        rv = atoi(v);
    }
    return rv;
}

static int status_bool(const char *param, const char *req)
{
    const char *v;
    char buf[32];
    int rv = 0;

    if ((v = status_cmd(param, req, buf, sizeof(buf)))) {
        if (strcasecmp(v, "on") == 0 ||
            strcasecmp(v, "true") == 0)
            rv = 1;
    }
    return rv;
}

static void display_maps(jk_ws_service_t *s, status_worker_t *sw,
                         jk_uri_worker_map_t *uwmap,
                         const char *worker, jk_logger_t *l)
{
    unsigned int i;

    jk_puts(s, "<br/>Uri Mappings:\n");
    jk_puts(s, "<table>\n<tr><th>Match Type</th><th>Uri</th>"
               "<th>Context</th><th>Suffix</th></tr>\n");
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (strcmp(uwr->worker_name, worker)) {
            continue;
        }
        jk_putv(s, "<tr><td>",
                status_val_match(uwr->match_type),
                "</td><td>", NULL);
        jk_puts(s, uwr->uri);
        jk_putv(s, "</td><td>", uwr->context, NULL);
        if (uwr->suffix)
            jk_putv(s, "</td><td>", uwr->suffix, NULL);
        else
            jk_putv(s, "</td><td>", "&nbsp;", NULL);

        jk_puts(s, "</td></tr>\n");
    }
    jk_puts(s, "</table>\n");
}

static void dump_maps(jk_ws_service_t *s, status_worker_t *sw,
                      jk_uri_worker_map_t *uwmap,
                      const char *worker, jk_logger_t *l)
{
    unsigned int i;

    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (strcmp(uwr->worker_name, worker)) {
            continue;
        }
        jk_printf(s, "    <jk:map type=\"%s\" uri=\"%s\" context=\"%s\"",
              status_val_match(uwr->match_type),
              uwr->uri,
              uwr->context) ;
        
        if (uwr->suffix)
            jk_putv(s, " suffix=\"",
                    uwr->suffix,
                    "\"", NULL);
        jk_puts(s, " />\n");
    }
}


/**
 * Command line reference:
 * cmd=list (default) display configuration
 * cmd=show display detailed configuration
 * cmd=update update configuration
 * cmd=add  add new uri map.
 * w=worker display detailed configuration for worker
 *
 * Worker parameters:
 * r=string redirect route name
 *
 */


static void display_workers(jk_ws_service_t *s, status_worker_t *sw,
                            const char *dworker, jk_logger_t *l)
{
    unsigned int i;
    char buf[32];

    for (i = 0; i < sw->we->num_of_workers; i++) {
        jk_worker_t *w = wc_get_worker_for_name(sw->we->worker_list[i], l);
        ajp_worker_t *aw = NULL;
        lb_worker_t *lb = NULL;
        if (w == NULL)
            continue;
        if (w->type == JK_LB_WORKER_TYPE) {
            lb = (lb_worker_t *)w->worker_private;
        }
        else if (w->type == JK_AJP13_WORKER_TYPE ||
                 w->type == JK_AJP14_WORKER_TYPE) {
            aw = (ajp_worker_t *)w->worker_private;
        }
        else {
            /* Skip status, jni and ajp12 worker */
            continue;
        }
        jk_puts(s, "<hr/>\n<h3>Worker Status for ");
        if (dworker && strcmp(dworker, sw->we->worker_list[i]) == 0) {
            /* Next click will colapse the editor */
            jk_putv(s, "<a href=\"", s->req_uri, "?cmd=show\">", NULL);
        }
        else
            jk_putv(s, "<a href=\"", s->req_uri, "?cmd=show&w=",
                    sw->we->worker_list[i], "\">", NULL);
        jk_putv(s, sw->we->worker_list[i], "</a></h3>\n", NULL);
        if (lb != NULL) {
            unsigned int j;
            int selected = -1;
            jk_puts(s, "<table><tr>"
                    "<th>Type</th><th>Sticky session</th>"
                    "<th>Force Sticky session</th>"
                    "<th>Retries</th>"
                    "</tr>\n<tr>");
            jk_putv(s, "<td>", status_worker_type(w->type), "</td>", NULL);
            jk_putv(s, "<td>", status_val_bool(lb->s->sticky_session),
                    "</td>", NULL);
            jk_putv(s, "<td>", status_val_bool(lb->s->sticky_session_force),
                    "</td>", NULL);
            jk_printf(s, "<td>%d</td>", lb->s->retries);
            jk_puts(s, "</tr>\n</table>\n<br/>\n");
            jk_puts(s, "<table><tr>"
                    "<th>Name</th><th>Type</th><th>Host</th><th>Addr</th>"
                    "<th>Stat</th><th>F</th><th>V</th><th>Acc</th><th>Err</th>"
                    "<th>Wr</th><th>Rd</th><th>Busy</th><th>RR</th><th>Cd</th></tr>\n");
            for (j = 0; j < lb->num_of_workers; j++) {
                worker_record_t *wr = &(lb->lb_workers[j]);
                ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
                jk_putv(s, "<tr>\n<td><a href=\"", s->req_uri,
                        "?cmd=show&w=",
                        wr->s->name, "\">",
                        wr->s->name, "</a></td>", NULL);
                if (dworker && strcmp(dworker, wr->s->name) == 0)
                    selected = j;
                jk_putv(s, "<td>", status_worker_type(wr->w->type), "</td>", NULL);
                jk_printf(s, "<td>%s:%d</td>", a->host, a->port);
                jk_putv(s, "<td>", jk_dump_hinfo(&a->worker_inet_addr, buf),
                        "</td>", NULL);
                /* TODO: descriptive status */
                jk_putv(s, "<td>",
                        status_val_status(wr->s->is_disabled,
                                          wr->s->in_error_state,
                                          wr->s->in_recovering,
                                          wr->s->is_busy),
                        "</td>", NULL);
                jk_printf(s, "<td>%d</td>", wr->s->lb_factor);
                jk_printf(s, "<td>%d</td>", wr->s->lb_value);
                jk_printf(s, "<td>%u</td>", wr->s->elected);
                jk_printf(s, "<td>%u</td>", wr->s->errors);
                jk_putv(s, "<td>", status_strfsize(wr->s->transferred, buf),
                        "</td>", NULL);
                jk_putv(s, "<td>", status_strfsize(wr->s->readed, buf),
                        "</td>", NULL);
                jk_printf(s, "<td>%u</td><td>", wr->s->busy);
                if (wr->s->redirect && *wr->s->redirect)
                    jk_puts(s, wr->s->redirect);
                else
                    jk_puts(s,"&nbsp;");
                jk_puts(s, "</td><td>\n");
                if (wr->s->domain && *wr->s->domain)
                    jk_puts(s, wr->s->domain);
                else
                    jk_puts(s,"&nbsp;");
                jk_puts(s, "</td>\n</tr>\n");
            }
            jk_puts(s, "</table><br/>\n");
            if (selected >= 0) {
                worker_record_t *wr = &(lb->lb_workers[selected]);
                jk_putv(s, "<hr/><h3>Edit worker settings for ",
                        wr->s->name, "</h3>\n", NULL);
                jk_putv(s, "<form method=\"GET\" action=\"",
                        s->req_uri, "\">\n", NULL);
                jk_puts(s, "<table>\n<input type=\"hidden\" name=\"cmd\" ");
                jk_puts(s, "value=\"update\">\n");
                jk_puts(s, "<input type=\"hidden\" name=\"w\" ");
                jk_putv(s, "value=\"", wr->s->name, "\">\n", NULL);
                jk_puts(s, "<input type=\"hidden\" name=\"id\" ");
                jk_printf(s, "value=\"%u\">\n</table>\n", selected);
                jk_puts(s, "<input type=\"hidden\" name=\"lb\" ");
                jk_printf(s, "value=\"%u\">\n</table>\n", i);

                jk_puts(s, "<table>\n<tr><td>Load factor:</td><td><input name=\"wf\" type=\"text\" ");
                jk_printf(s, "value=\"%d\"/></td><tr>\n", wr->s->lb_factor);
                jk_puts(s, "<tr><td>Route Redirect:</td><td><input name=\"wr\" type=\"text\" ");
                jk_putv(s, "value=\"", wr->s->redirect, NULL);
                jk_puts(s, "\"/></td></tr>\n");
                jk_puts(s, "<tr><td>Cluster Domain:</td><td><input name=\"wc\" type=\"text\" ");
                jk_putv(s, "value=\"", wr->s->domain, NULL);
                jk_puts(s, "\"/></td></tr>\n");
                jk_puts(s, "<tr><td>Disabled:</td><td><input name=\"wd\" type=\"checkbox\"");
                if (wr->s->is_disabled)
                    jk_puts(s, "  checked=\"checked\"");
                jk_puts(s, "/></td></tr>\n");
                jk_puts(s, "</td></tr>\n</table>\n");
                jk_puts(s, "<br/><input type=\"submit\" value=\"Update Worker\"/>\n</form>\n");

            }
            else if (dworker && strcmp(dworker, sw->we->worker_list[i]) == 0) {
                /* Edit Load balancer settings */
                jk_putv(s, "<hr/><h3>Edit Load balancer settings for ",
                        dworker, "</h3>\n", NULL);
                jk_putv(s, "<form method=\"GET\" action=\"",
                        s->req_uri, "\">\n", NULL);
                jk_puts(s, "<table>\n<input type=\"hidden\" name=\"cmd\" ");
                jk_puts(s, "value=\"update\"/>\n");
                jk_puts(s, "<input type=\"hidden\" name=\"w\" ");
                jk_putv(s, "value=\"", dworker, "\"/>\n", NULL);
                jk_puts(s, "<input type=\"hidden\" name=\"id\" ");
                jk_printf(s, "value=\"%u\"/>\n</table>\n", i);

                jk_puts(s, "<table>\n<tr><td>Retries:</td><td><input name=\"lr\" type=\"text\" ");
                jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->s->retries);
                jk_puts(s, "<tr><td>Recover time:</td><td><input name=\"lt\" type=\"text\" ");
                jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->s->recover_wait_time);
                jk_puts(s, "<tr><td>Sticky session:</td><td><input name=\"ls\" type=\"checkbox\"");
                if (lb->s->sticky_session)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/></td></tr>\n");
                jk_puts(s, "<tr><td>Force Sticky session:</td><td><input name=\"lf\" type=\"checkbox\"");
                if (lb->s->sticky_session_force)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/></td></tr>\n");
                jk_puts(s, "</table>\n");

                display_maps(s, sw, s->uw_map, dworker, l);
                jk_puts(s, "<br/><input type=\"submit\" value=\"Update Balancer\"/></form>\n");
            }
        }
        else {
            jk_puts(s, "\n\n<table><tr>"
                    "<th>Type</th><th>Host</th><th>Addr</th>"
                    "</tr>\n<tr>");
            jk_putv(s, "<td>", status_worker_type(w->type), "</td>", NULL);
            jk_puts(s, "</tr>\n</table>\n");
            jk_printf(s, "<td>%s:%d</td>", aw->host, aw->port);
            jk_putv(s, "<td>", jk_dump_hinfo(&aw->worker_inet_addr, buf),
                    "</td>\n</tr>\n", NULL);
            jk_puts(s, "</table>\n");

        }
    }
    /* Display legend */
    jk_puts(s, "<hr/><table>\n"
            "<tr><th>Name</th><td>Worker route name</td></tr>\n"
            "<tr><th>Type</th><td>Worker type</td></tr>\n"
            "<tr><th>Addr</th><td>Backend Address info</td></tr>\n"
            "<tr><th>Stat</th><td>Worker status</td></tr>\n"
            "<tr><th>F</th><td>Load Balancer Factor</td></tr>\n"
            "<tr><th>V</th><td>Load Balancer Value</td></tr>\n"
            "<tr><th>Acc</th><td>Number of requests</td></tr>\n"
            "<tr><th>Err</th><td>Number of failed requests</td></tr>\n"
            "<tr><th>Wr</th><td>Number of bytes transferred</td></tr>\n"
            "<tr><th>Rd</th><td>Number of bytes read</td></tr>\n"
            "<tr><th>RR</th><td>Route redirect</td></tr>\n"
            "<tr><th>Cd</th><td>Cluster domain</td></tr>\n"
            "</table>");
}

static void dump_config(jk_ws_service_t *s, status_worker_t *sw,
                        jk_logger_t *l)
{
    unsigned int i;
    char buf[32];
    int has_lb = 0;

    for (i = 0; i < sw->we->num_of_workers; i++) {
        jk_worker_t *w = wc_get_worker_for_name(sw->we->worker_list[i], l);
        if (w == NULL)
            continue;
        if (w->type == JK_LB_WORKER_TYPE) {
            has_lb = 1;
            break;
        }
    }

    jk_printf(s, "  <jk:server name=\"%s\" port=\"%d\" software=\"%s\" version=\"%s\" />\n",
              s->server_name, s->server_port, s->server_software,  JK_VERSTRING);
    if (has_lb)
        jk_puts(s, "  <jk:balancers>\n");
    for (i = 0; i < sw->we->num_of_workers; i++) {
        jk_worker_t *w = wc_get_worker_for_name(sw->we->worker_list[i], l);
        lb_worker_t *lb = NULL;
        unsigned int j;

        if (w == NULL)
            continue;
        if (w->type == JK_LB_WORKER_TYPE) {
            lb = (lb_worker_t *)w->worker_private;
        }
        else {
            /* Skip non lb workers */
            continue;
        }
        jk_printf(s, "  <jk:balancer id=\"%d\" name=\"%s\" type=\"%s\" sticky=\"$s\" stickyforce=\"%s\" retries=\"%d\" recover=\"%d\" >\n", 
             i,
             lb->s->name,
             status_worker_type(w->type), 
             status_val_bool(lb->s->sticky_session),
             status_val_bool(lb->s->sticky_session_force),
             lb->s->retries,
             lb->s->recover_wait_time);
        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
            /* TODO: descriptive status */
            jk_printf(s, "      <jk:member id=\"%d\" name=\"%s\" type=\"%s\" host=\"%s\" port=\"%d\" address=\"%s\" status=\"%s\"", 
                j,
                wr->s->name,
                status_worker_type(wr->w->type),
                a->host,
                a->port,
                jk_dump_hinfo(&a->worker_inet_addr, buf),
                status_val_status(wr->s->is_disabled,
                                  wr->s->in_error_state,
                                  wr->s->in_recovering,
                                  wr->s->is_busy) );
                                      
            jk_printf(s, " lbfactor=\"%d\"", wr->s->lb_factor);
            jk_printf(s, " lbvalue=\"%d\"", wr->s->lb_value);
            jk_printf(s, " elected=\"%u\"", wr->s->elected);
            jk_printf(s, " readed=\"%u\"", wr->s->readed);
            jk_printf(s, " transferred=\"%u\"", wr->s->transferred);
            jk_printf(s, " errors=\"%u\"", wr->s->errors);
            jk_printf(s, " busy=\"%u\"", wr->s->busy);
            if (wr->s->redirect && *wr->s->redirect)
                jk_printf(s, " redirect=\"%s\"", wr->s->redirect);
            if (wr->s->domain && *wr->s->domain)
                jk_printf(s, " domain=\"%s\"", wr->s->domain);
            jk_puts(s, " />\n");
        }
        dump_maps(s, sw, s->uw_map, lb->s->name, l);
        jk_puts(s, "  </jk:balancer>\n");

    }
    if (has_lb)
        jk_puts(s, "  </jk:balancers>\n");

}

static void update_worker(jk_ws_service_t *s, status_worker_t *sw,
                          const char *dworker, jk_logger_t *l)
{
    int i;
    char buf[1024];
    const char *b;
    lb_worker_t *lb;
    jk_worker_t *w = wc_get_worker_for_name(dworker, l);

    if (w && w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        i = status_int("lr", s->query_string, lb->s->retries);
        if (i > 0)
            lb->s->retries = i;
        i = status_int("lt", s->query_string, lb->s->recover_wait_time);
        if (i > 59)
            lb->s->recover_wait_time = i;
        lb->s->sticky_session = status_bool("ls", s->query_string);
        lb->s->sticky_session_force = status_bool("lf", s->query_string);
    }
    else  {
        int n = status_int("lb", s->query_string, -1);
        worker_record_t *wr = NULL;
        ajp_worker_t *a;
        if (n >= 0 && n < (int)sw->we->num_of_workers)
            w = wc_get_worker_for_name(sw->we->worker_list[n], l);
        else {
            if (!(b = status_cmd("l", s->query_string, buf, sizeof(buf))))
                return;
            w = wc_get_worker_for_name(b, l);
        }
        if (!w || w->type != JK_LB_WORKER_TYPE)
            return;
        lb = (lb_worker_t *)w->worker_private;
        i = status_int("id", s->query_string, -1);
        if (i >= 0 && i < (int)lb->num_of_workers)
            wr = &(lb->lb_workers[i]);
        else {
            for (i = 0; i < (int)lb->num_of_workers; i++) {
                if (strcmp(dworker, lb->lb_workers[i].s->name) == 0) {
                    wr = &(lb->lb_workers[i]);
                    break;
                }
            }
        }
        if (!wr)
            return;
        a  = (ajp_worker_t *)wr->w->worker_private;

        if ((b = status_cmd("wr", s->query_string, buf, sizeof(buf))))
            strncpy(wr->s->redirect, b, JK_SHM_STR_SIZ);
        else
            memset(wr->s->redirect, 0, JK_SHM_STR_SIZ);
        if ((b = status_cmd("wc", s->query_string, buf, sizeof(buf))))
            strncpy(wr->s->domain, b, JK_SHM_STR_SIZ);
        else
            memset(wr->s->domain, 0, JK_SHM_STR_SIZ);
        wr->s->is_disabled = status_bool("wd", s->query_string);
        i = status_int("wf", s->query_string, wr->s->lb_factor);
        if (i > 0)
            wr->s->lb_factor = i;
    }
}

static int status_cmd_type(const char *req)
{
    if (!req)
        return 0;
    else if (!strncmp(req, "cmd=list", 8))
        return 0;
    else if (!strncmp(req, "cmd=show", 8))
        return 1;
    else if (!strncmp(req, "cmd=update", 10))
        return 2;
    else
        return 0;
}

static int status_mime_type(const char *req)
{
    int ret = 0 ;
    if (req) {
        char mimetype[32];
        if (status_cmd("mime", req, mimetype, sizeof(mimetype)) != NULL) {
            if (!strcmp(mimetype, "xml"))
                ret = 1;
            else if (!strcmp(mimetype, "txt"))
                ret = 2;
        }
    }
    return ret ;
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_recoverable_error)
{
    JK_TRACE_ENTER(l);

    if (e && e->endpoint_private && s) {
        char buf[128];
        char *worker = NULL;
        int cmd;
        int mime;
        status_endpoint_t *p = e->endpoint_private;

        *is_recoverable_error = JK_FALSE;

        /* Step 1: Process GET params and update configuration */
        cmd = status_cmd_type(s->query_string);
        mime = status_mime_type(s->query_string);
        if (cmd > 0 && (status_cmd("w", s->query_string, buf, sizeof(buf)) != NULL))
            worker = strdup(buf);
        if ((cmd == 2) && worker) {
            /* lock shared memory */
            jk_shm_lock();
            update_worker(s, p->s_worker, worker, l);
            /* update modification time to reflect the current config */
            jk_shm_set_workers_time(time(NULL));
            /* Since we updated the config no need to reload
             * on the next request
             */
            jk_shm_sync_access_time();
            /* unlock the shared memory */
            jk_shm_unlock();
        }
        if(mime == 0) {
            s->start_response(s, 200, "OK", headers_names, headers_vhtml, 3);
            s->write(s, JK_STATUS_HEAD, sizeof(JK_STATUS_HEAD) - 1);
            if (p->s_worker->css) {
                jk_putv(s, "\n<link rel=\"stylesheet\" type=\"text/css\" href=\"",
                        p->s_worker->css, "\" />\n", NULL);
            }
            s->write(s, JK_STATUS_HEND, sizeof(JK_STATUS_HEND) - 1);
            jk_puts(s, "<h1>JK Status Manager for ");
            jk_puts(s, s->server_name);
            jk_puts(s, "</h1>\n\n");
            jk_putv(s, "<dl><dt>Server Version: ",
                    s->server_software, "</dt>\n", NULL);
            jk_putv(s, "<dt>JK Version: ",
                    JK_VERSTRING, "\n</dt></dl>\n", NULL);
            /* Step 2: Display configuration */
            display_workers(s, p->s_worker, worker, l);


            s->write(s, JK_STATUS_BEND, sizeof(JK_STATUS_BEND) - 1);

        }
        else if (mime == 1) {
            s->start_response(s, 200, "OK", headers_names, headers_vxml, 3);
            s->write(s, JK_STATUS_XMLH, sizeof(JK_STATUS_XMLH) - 1);
            dump_config(s, p->s_worker, l);
            s->write(s, JK_STATUS_XMLE, sizeof(JK_STATUS_XMLE) - 1);
        }
        else {
            s->start_response(s, 200, "OK", headers_names, headers_vtxt, 3);
            s->write(s,  JK_STATUS_TEXTUPDATE_RESPONCE,
                     sizeof(JK_STATUS_TEXTUPDATE_RESPONCE) - 1);
        }
        if (worker)
            free(worker);
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "status: end of service with error");
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD done(jk_endpoint_t **e, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (e && *e && (*e)->endpoint_private) {
        *e = NULL;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private) {
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we, jk_logger_t *log)
{
    JK_TRACE_ENTER(log);
    if (pThis && pThis->worker_private) {
        status_worker_t *p = pThis->worker_private;
        p->we = we;
        if (!jk_get_worker_str_prop(props, p->name, "css", &(p->css)))
            p->css = NULL;
    }
    JK_TRACE_EXIT(log);
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private && pend) {
        status_worker_t *p = (status_worker_t *)pThis->worker_private;
        *pend = p->ep.e;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && *pThis && (*pThis)->worker_private) {
        status_worker_t *private_data = (*pThis)->worker_private;

        jk_close_pool(&private_data->p);
        free(private_data);

        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

int JK_METHOD status_worker_factory(jk_worker_t **w,
                                    const char *name, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (NULL != name && NULL != w) {
        status_worker_t *private_data =
            (status_worker_t *) calloc(1, sizeof(status_worker_t));

        jk_open_pool(&private_data->p,
                        private_data->buf,
                        sizeof(jk_pool_atom_t) * TINY_POOL_SIZE);

        private_data->name = name;

        private_data->worker.worker_private = private_data;
        private_data->worker.validate = validate;
        private_data->worker.init = init;
        private_data->worker.get_endpoint = get_endpoint;
        private_data->worker.destroy = destroy;
        private_data->worker.retries = 1;

        /* Status worker has single static endpoint. */
        private_data->ep.endpoint.done = done;
        private_data->ep.endpoint.service = service;
        private_data->ep.endpoint.endpoint_private = &private_data->ep;
        private_data->ep.e = &(private_data->ep.endpoint);
        private_data->ep.s_worker = private_data;
        *w = &private_data->worker;
        JK_TRACE_EXIT(l);
        return JK_STATUS_WORKER_TYPE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return 0;
}
