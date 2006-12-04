/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision$                                        *
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

/**
 * Command line reference:
 * cmd=list (default) display configuration
 * cmd=show display detailed configuration
 * cmd=edit form to change configuration
 * cmd=update commit update configuration
 * cmd=reset reset lb runtime states, or lb member runtime states
 * cmd=version show only software version
 * Query arguments:
 * re=n (refresh time in seconds, n=0: disabled)
 * w=worker (cmd should be executed for worker "worker")
 * sw=sub_worker (cmd should be executed for "sub_worker" of worker "worker")
 * from=lastcmd (the last viewing command was "lastcmd")
 * opt=option (changes meaning of edit and list/show)
 */

#define JK_STATUS_ARG_CMD                  ("cmd")
#define JK_STATUS_ARG_MIME                 ("mime")
#define JK_STATUS_ARG_FROM                 ("from")
#define JK_STATUS_ARG_REFRESH              ("re")
#define JK_STATUS_ARG_WORKER               ("w")
#define JK_STATUS_ARG_WORKER_MEMBER        ("sw")
#define JK_STATUS_ARG_OPTION               ("opt")

#define JK_STATUS_ARG_LB_RETRIES           ("lr")
#define JK_STATUS_ARG_LB_RECOVER_TIME      ("lt")
#define JK_STATUS_ARG_LB_STICKY            ("ls")
#define JK_STATUS_ARG_LB_STICKY_FORCE      ("lf")
#define JK_STATUS_ARG_LB_METHOD            ("lm")
#define JK_STATUS_ARG_LB_LOCK              ("ll")

#define JK_STATUS_ARG_LB_TEXT_RETRIES      "Retries"
#define JK_STATUS_ARG_LB_TEXT_RECOVER_TIME "Recover Wait Time"
#define JK_STATUS_ARG_LB_TEXT_STICKY       "Sticky Sessions"
#define JK_STATUS_ARG_LB_TEXT_STICKY_FORCE "Force Sticky Sessions"
#define JK_STATUS_ARG_LB_TEXT_METHOD       "LB Method"
#define JK_STATUS_ARG_LB_TEXT_LOCK         "Locking"

#define JK_STATUS_ARG_LBM_ACTIVATION       ("wa")
#define JK_STATUS_ARG_LBM_FACTOR           ("wf")
#define JK_STATUS_ARG_LBM_ROUTE            ("wn")
#define JK_STATUS_ARG_LBM_REDIRECT         ("wr")
#define JK_STATUS_ARG_LBM_DOMAIN           ("wc")
#define JK_STATUS_ARG_LBM_DISTANCE         ("wd")

#define JK_STATUS_ARG_LBM_TEXT_ACTIVATION  "Activation"
#define JK_STATUS_ARG_LBM_TEXT_FACTOR      "LB Factor"
#define JK_STATUS_ARG_LBM_TEXT_ROUTE       "Route"
#define JK_STATUS_ARG_LBM_TEXT_REDIRECT    "Redirect Route"
#define JK_STATUS_ARG_LBM_TEXT_DOMAIN      "Cluster Domain"
#define JK_STATUS_ARG_LBM_TEXT_DISTANCE    "Distance"

#define JK_STATUS_CMD_UNKNOWN              (0)
#define JK_STATUS_CMD_LIST                 (1)
#define JK_STATUS_CMD_SHOW                 (2)
#define JK_STATUS_CMD_EDIT                 (3)
#define JK_STATUS_CMD_UPDATE               (4)
#define JK_STATUS_CMD_RESET                (5)
#define JK_STATUS_CMD_VERSION              (6)
#define JK_STATUS_CMD_DEF                  (JK_STATUS_CMD_UNKNOWN)
#define JK_STATUS_CMD_MAX                  (JK_STATUS_CMD_VERSION)
#define JK_STATUS_CMD_TEXT_UNKNOWN         ("unknown")
#define JK_STATUS_CMD_TEXT_LIST            ("list")
#define JK_STATUS_CMD_TEXT_SHOW            ("show")
#define JK_STATUS_CMD_TEXT_EDIT            ("edit")
#define JK_STATUS_CMD_TEXT_UPDATE          ("update")
#define JK_STATUS_CMD_TEXT_RESET           ("reset")
#define JK_STATUS_CMD_TEXT_VERSION         ("version")
#define JK_STATUS_CMD_TEXT_DEF             (JK_STATUS_CMD_TEXT_UNKNOWN)

#define JK_STATUS_MIME_UNKNOWN             (0)
#define JK_STATUS_MIME_HTML                (1)
#define JK_STATUS_MIME_XML                 (2)
#define JK_STATUS_MIME_TXT                 (3)
#define JK_STATUS_MIME_PROP                (4)
#define JK_STATUS_MIME_DEF                 (JK_STATUS_MIME_UNKNOWN)
#define JK_STATUS_MIME_MAX                 (JK_STATUS_MIME_PROP)
#define JK_STATUS_MIME_TEXT_UNKNOWN        ("unknown")
#define JK_STATUS_MIME_TEXT_HTML           ("html")
#define JK_STATUS_MIME_TEXT_XML            ("xml")
#define JK_STATUS_MIME_TEXT_TXT            ("txt")
#define JK_STATUS_MIME_TEXT_PROP           ("prop")
#define JK_STATUS_MIME_TEXT_DEF            (JK_STATUS_MIME_TEXT_UNKNOWN)

#define JK_STATUS_ATT_NO_MEMBERS           ("nosw")

#define JK_STATUS_MASK_ACTIVE              0x000000FF
#define JK_STATUS_MASK_DISABLED            0x0000FF00
#define JK_STATUS_MASK_STOPPED             0x00FF0000
#define JK_STATUS_MASK_OK                  0x00010101
#define JK_STATUS_MASK_NA                  0x00020202
#define JK_STATUS_MASK_BUSY                0x00040404
#define JK_STATUS_MASK_RECOVER             0x00080808
#define JK_STATUS_MASK_ERROR               0x00101010
#define JK_STATUS_MASK_GOOD_DEF            0x0000000F
#define JK_STATUS_MASK_BAD_DEF             0x00FF1010

#define JK_STATUS_REFRESH                  "3"
#define JK_STATUS_ESC_CHARS                ("<>?&")

#define JK_STATUS_HEAD                     "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                                           "DTD HTML 3.2 Final//EN\">\n"      \
                                           "<html><head><title>JK Status Manager</title>"

#define JK_STATUS_COPYRIGHT                "Copyright &#169; 1999-2006, The Apache Software Foundation<br />" \
                                           "Licensed under the <a href=\"http://www.apache.org/licenses/LICENSE-2.0\">" \
                                           "Apache License, Version 2.0</a>."

#define JK_STATUS_HEND                     "</head>\n<body>\n"
#define JK_STATUS_BEND                     "</body>\n</html>\n"

#define JK_STATUS_XMLH                     "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"

#define JK_STATUS_NS_DEF                   "jk:"
#define JK_STATUS_XMLNS_DEF                "xmlns:jk=\"http://tomcat.apache.org\""
#define JK_STATUS_PREFIX_DEF               "worker"

#define JK_STATUS_FORM_START               "<form method=\"%s\" action=\"%s\">\n"
#define JK_STATUS_FORM_HIDDEN_INT          "<input type=\"hidden\" name=\"%s\" value=\"%d\"/>\n"
#define JK_STATUS_FORM_HIDDEN_STRING       "<input type=\"hidden\" name=\"%s\" value=\"%s\"/>\n"
#define JK_STATUS_TABLE_HEAD_3_STRING      "<tr><th>%s</th><th>%s</th><th>%s</th></tr>\n"
#define JK_STATUS_TABLE_ROW_3_STRING       "<tr><td>%s</td><td>%s</td><td>%s</td></tr>\n"
#define JK_STATUS_SHOW_AJP_HEAD            "<tr>" \
                                           "<th>Type</th>" \
                                           "<th>Host</th>" \
                                           "<th>Addr</th>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_AJP_ROW             "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%s:%d</td>" \
                                           "<td>%s</td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_LB_HEAD             "<tr>" \
                                           "<th>Type</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_STICKY "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_STICKY_FORCE "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_RETRIES "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_METHOD "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_LOCK "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_RECOVER_TIME "</th>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_LB_ROW              "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_MEMBER_HEAD         "<tr>" \
                                           "<th>&nbsp;</th><th>Name</th><th>Type</th>" \
                                           "<th>Host</th><th>Addr</th>" \
                                           "<th>Act</th><th>Stat</th>" \
                                           "<th>D</th><th>F</th><th>M</th>" \
                                           "<th>V</th><th>Acc</th>" \
                                           "<th>Err</th><th>CE</th>" \
                                           "<th>Wr</th><th>Rd</th><th>Busy</th><th>Max</th>" \
                                           "<th>" JK_STATUS_ARG_LBM_TEXT_ROUTE "</th>" \
                                           "<th>RR</th><th>Cd</th><th>Rs</th>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_MEMBER_ROW          "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s:%d</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%" JK_UINT64_T_FMT "</td>" \
                                           "<td>%" JK_UINT64_T_FMT "</td>" \
                                           "<td>%" JK_UINT64_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%u</td>" \
                                           "<td>%u</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%u</td>" \
                                           "</tr>\n"

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
    const char        *ns;
    const char        *xmlns;
    const char        *doctype;
    const char        *prefix;
    int               read_only;
    char              **user_names;
    unsigned int      num_of_users;
    jk_uint32_t       good_mask;
    jk_uint32_t       bad_mask;
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

static const char *cmd_type[] = {
    JK_STATUS_CMD_TEXT_UNKNOWN,
    JK_STATUS_CMD_TEXT_LIST,
    JK_STATUS_CMD_TEXT_SHOW,
    JK_STATUS_CMD_TEXT_EDIT,
    JK_STATUS_CMD_TEXT_UPDATE,
    JK_STATUS_CMD_TEXT_RESET,
    JK_STATUS_CMD_TEXT_VERSION,
    NULL
};

static const char *mime_type[] = {
    JK_STATUS_MIME_TEXT_UNKNOWN,
    JK_STATUS_MIME_TEXT_HTML,
    JK_STATUS_MIME_TEXT_XML,
    JK_STATUS_MIME_TEXT_TXT,
    JK_STATUS_MIME_TEXT_PROP,
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

static void jk_puts(jk_ws_service_t *s, const char *str)
{
    if (str)
        s->write(s, str, (unsigned int)strlen(str));
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
        s->write(s, str, (unsigned int)strlen(str));
    }
    va_end(va);
}

static int jk_printf(jk_ws_service_t *s, const char *fmt, ...)
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

static void jk_print_xml_start_elt(jk_ws_service_t *s, status_worker_t *sw,
                                   int indentation, int close_tag,
                                   const char *name)
{
    if (close_tag) {
        jk_printf(s, "%*s<%s%s>\n", indentation, "", sw->ns, name);
    }
    else {
        jk_printf(s, "%*s<%s%s\n", indentation, "", sw->ns, name);
    }
}

static void jk_print_xml_close_elt(jk_ws_service_t *s, status_worker_t *sw,
                                   int indentation,
                                   const char *name)
{
    jk_printf(s, "%*s</%s%s>\n", indentation, "", sw->ns, name);
}

static void jk_print_xml_stop_elt(jk_ws_service_t *s,
                                  int indentation, int close_tag)
{
    if (close_tag) {
        jk_printf(s, "%*s/>\n", indentation, "");
    }
    else {
        jk_printf(s, "%*s>\n", indentation, "");
    }
}

static void jk_print_xml_att_string(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, const char *value)
{
    jk_printf(s, "%*s%s=\"%s\"\n", indentation, "", key, value ? value : "");
}

static void jk_print_xml_att_int(jk_ws_service_t *s,
                                 int indentation,
                                 const char *key, int value)
{
    jk_printf(s, "%*s%s=\"%d\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint(jk_ws_service_t *s,
                                  int indentation,
                                  const char *key, unsigned value)
{
    jk_printf(s, "%*s%s=\"%u\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint32(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, jk_uint32_t value)
{
    jk_printf(s, "%*s%s=\"%" JK_UINT32_T_FMT "\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint64(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, jk_uint64_t value)
{
    jk_printf(s, "%*s%s=\"%" JK_UINT64_T_FMT "\"\n", indentation, "", key, value);
}

static void jk_print_prop_att_string(jk_ws_service_t *s, status_worker_t *sw,
                                     const char *name,
                                     const char *key, const char *value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%s\n", sw->prefix, name, key, value ? value : "");
    }
    else {
        jk_printf(s, "%s.%s=%s\n", sw->prefix, key, value ? value : "");
    }
}

static void jk_print_prop_att_int(jk_ws_service_t *s, status_worker_t *sw,
                                  const char *name,
                                  const char *key, int value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%d\n", sw->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%d\n", sw->prefix, key, value);
    }
}

static void jk_print_prop_att_uint(jk_ws_service_t *s, status_worker_t *sw,
                                   const char *name,
                                   const char *key, unsigned value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%u\n", sw->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%u\n", sw->prefix, key, value);
    }
}

static void jk_print_prop_att_uint32(jk_ws_service_t *s, status_worker_t *sw,
                                     const char *name,
                                     const char *key, jk_uint32_t value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%" JK_UINT32_T_FMT "\n", sw->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%" JK_UINT32_T_FMT "\n", sw->prefix, key, value);
    }
}

static void jk_print_prop_att_uint64(jk_ws_service_t *s, status_worker_t *sw,
                                     const char *name,
                                     const char *key, jk_uint64_t value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%" JK_UINT64_T_FMT "\n", sw->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%" JK_UINT64_T_FMT "\n", sw->prefix, key, value);
    }
}

static void jk_print_prop_item_string(jk_ws_service_t *s, status_worker_t *sw,
                                      const char *name, const char *list, int num,
                                      const char *key, const char *value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s.%d.%s=%s\n", sw->prefix, name, list, num, key, value ? value : "");
    }
    else {
        jk_printf(s, "%s.%s.%d.%s=%s\n", sw->prefix, list, num, key, value ? value : "");
    }
}

/* Actually APR's apr_strfsize */
static char *status_strfsize(jk_uint64_t size, char *buf)
{
    const char ord[] = "KMGTPE";
    const char *o = ord;
    unsigned int remain, siz;

    if (size < 973) {
        if (sprintf(buf, "%3d ", (int) size) < 0)
            return strcpy(buf, "****");
        return buf;
    }
    do {
        remain = (unsigned int)(size & 0x03FF);
        size >>= 10;
        if (size >= 973) {
            ++o;
            continue;
        }
        siz = (unsigned int)(size & 0xFFFF);
        if (siz < 9 || (siz == 9 && remain < 973)) {
            if ((remain = ((remain * 5) + 256) / 512) >= 10)
                ++siz, remain = 0;
            if (sprintf(buf, "%d.%d%c", siz, remain, *o) < 0)
                return strcpy(buf, "****");
            return buf;
        }
        if (remain >= 512)
            ++siz;
        if (sprintf(buf, "%3d%c", siz, *o) < 0)
            return strcpy(buf, "****");
        return buf;
    } while (1);
}

static int status_rate(worker_record_t *wr, status_worker_t *sw,
                       jk_logger_t *l)
{
    jk_uint32_t mask = 0;
    int activation = wr->s->activation;
    int state = wr->s->state;
    jk_uint32_t good = sw->good_mask;
    jk_uint32_t bad = sw->bad_mask;
    int rv = 0;

    switch (activation)
    {
    case JK_LB_ACTIVATION_ACTIVE:
        mask = JK_STATUS_MASK_ACTIVE;
        break;
    case JK_LB_ACTIVATION_DISABLED:
        mask = JK_STATUS_MASK_DISABLED;
        break;
    case JK_LB_ACTIVATION_STOPPED:
        mask = JK_STATUS_MASK_STOPPED;
        break;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Unknown activation type '%d'",
               activation);
    }
    switch (state)
    {
    case JK_LB_STATE_OK:
        mask &= JK_STATUS_MASK_OK;
        break;
    case JK_LB_STATE_NA:
        mask &= JK_STATUS_MASK_NA;
        break;
    case JK_LB_STATE_BUSY:
        mask &= JK_STATUS_MASK_BUSY;
        break;
    case JK_LB_STATE_ERROR:
        mask &= JK_STATUS_MASK_ERROR;
        break;
    case JK_LB_STATE_RECOVER:
        mask &= JK_STATUS_MASK_RECOVER;
        break;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Unknown state type '%d'",
               state);
    }
    if (mask&bad)
        rv = -1;
    else if (mask&good)
        rv = 1;
    else
        rv = 0;
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "rating of activation '%s' and state '%s' for good '%08" JK_UINT32_T_HEX_FMT
               "' and bad '%08" JK_UINT32_T_HEX_FMT "' is %d",
               jk_lb_get_activation(wr, l), jk_lb_get_state(wr, l),
               good, bad, rv);
    return rv;
}

static jk_uint32_t status_get_single_rating(const char rating, jk_logger_t *l)
{
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "rating retrieval for '%c'",
               rating);
    switch (rating)
    {
    case 'A':
    case 'a':
        return JK_STATUS_MASK_ACTIVE;
    case 'D':
    case 'd':
        return JK_STATUS_MASK_DISABLED;
    case 'S':
    case 's':
        return JK_STATUS_MASK_STOPPED;
    case 'O':
    case 'o':
        return JK_STATUS_MASK_OK;
    case 'N':
    case 'n':
        return JK_STATUS_MASK_NA;
    case 'B':
    case 'b':
        return JK_STATUS_MASK_BUSY;
    case 'R':
    case 'r':
        return JK_STATUS_MASK_RECOVER;
    case 'E':
    case 'e':
        return JK_STATUS_MASK_ERROR;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Unknown rating type '%c'",
               rating);
        return 0;
    }
}

static jk_uint32_t status_get_rating(const char *rating,
                                     jk_logger_t *l)
{
    int off = 0;
    jk_uint32_t mask = 0;

    while (rating[off] == ' ' || rating[off] == '\t' || rating[off] == '.') {
        off++;
    }
    mask = status_get_single_rating(rating[off], l);
    while (rating[off] != '\0' && rating[off] != '.') {
        off++;
    }
    if (rating[off] == '.') {
        off++;
    }
    if (rating[off] != '\0') {
        mask &= status_get_single_rating(rating[off], l);
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "rating for '%s' is '%08" JK_UINT32_T_HEX_FMT "'",
               rating, mask);
    return mask;
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

static int status_get_arg_raw(const char *param,
                              const char *req, char *buf, size_t len,
                              jk_logger_t *l)
{
    char ps[32];
    char *p;
    size_t pos = 0;
    int rv = 0;

    buf[0] = '\0';
    if (!req)
        return JK_FALSE;
    if (!param)
        return JK_FALSE;
    sprintf(ps, "&%s=", param);
    p = strstr(req, ps);
    if (!p) {
        sprintf(ps, "%s=", param);
        if (!strncmp(req, ps, strlen(ps)))
            p = (char *)req;
    }
    if (p) {
        p += strlen(ps);
        while (*p) {
            if (*p != '&')
                buf[pos++] = *p;
            else
                break;
            if (pos >= len-1)
                break;
            p++;
        }
        buf[pos] = '\0';
        if (pos)
            rv = JK_TRUE;
        else
            rv = JK_UNSET;
    }
    else
        rv = JK_FALSE;
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "result for '%s' in '%s' is %d (%s)",
               param, req, rv, buf);
    return rv;
}

static int status_get_arg(const char *param,
                          const char *req, char *buf, size_t len,
                          jk_logger_t *l)
{
    int rv;

    rv = status_get_arg_raw(param, req, buf, len, l);
    if (rv == JK_TRUE) {
        char *off = buf;
        while ((off = strpbrk(off, JK_STATUS_ESC_CHARS)))
            off[0] = '@';
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "result for '%s' in '%s' is %d (%s)",
               param, req, rv, buf);
    return rv;
}

static int status_get_int(const char *param,
                          const char *req, int def,
                          jk_logger_t *l)
{
    char buf[32];
    int rv = def;

    if (status_get_arg_raw(param, req, buf, sizeof(buf), l) == JK_TRUE) {
        rv = atoi(buf);
    }
    return rv;
}

static int status_get_bool(const char *param,
                           const char *req, int def,
                           jk_logger_t *l)
{
    char buf[32];
    int rv = def;

    if (status_get_arg_raw(param, req, buf, sizeof(buf), l) == JK_TRUE) {
        if (strcasecmp(buf, "on") == 0 ||
            strcasecmp(buf, "true") == 0 ||
            strcasecmp(buf, "1") == 0)
            rv = 1;
        else if (strcasecmp(buf, "off") == 0 ||
            strcasecmp(buf, "false") == 0 ||
            strcasecmp(buf, "0") == 0)
            rv = 0;
    }
    return rv;
}

const char *status_cmd_text(int cmd)
{
    return cmd_type[cmd];
}

static int status_cmd_int(const char *cmd)
{
    if (!cmd)
        return JK_STATUS_CMD_DEF;
    if (!strcmp(cmd, JK_STATUS_CMD_TEXT_LIST))
        return JK_STATUS_CMD_LIST;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_SHOW))
        return JK_STATUS_CMD_SHOW;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_EDIT))
        return JK_STATUS_CMD_EDIT;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_UPDATE))
        return JK_STATUS_CMD_UPDATE;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_RESET))
        return JK_STATUS_CMD_RESET;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_VERSION))
        return JK_STATUS_CMD_VERSION;
    return JK_STATUS_CMD_DEF;
}

const char *status_mime_text(int mime)
{
    return mime_type[mime];
}

static int status_mime_int(const char *mime)
{
    if (!mime)
        return JK_STATUS_MIME_DEF;
    if (!strcmp(mime, JK_STATUS_MIME_TEXT_HTML))
        return JK_STATUS_MIME_HTML;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_XML))
        return JK_STATUS_MIME_XML;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_TXT))
        return JK_STATUS_MIME_TXT;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_PROP))
        return JK_STATUS_MIME_PROP;
    return JK_STATUS_MIME_DEF;
}

static void status_start_form(jk_ws_service_t *s, const char *method,
                              int cmd, int mime,
                              int from, int refresh,
                              const char *worker, const char *sub_worker,
                              const char *option)
{
    if (method)
        jk_printf(s, JK_STATUS_FORM_START, method, s->req_uri);
    else
        return;
    if (cmd) {
        jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                  JK_STATUS_ARG_CMD, status_cmd_text(cmd));
        if (mime) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                      JK_STATUS_ARG_MIME, status_mime_text(mime));
        }
        if (from) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                      JK_STATUS_ARG_FROM, status_cmd_text(from));
        }
        if (refresh) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_INT,
                      JK_STATUS_ARG_REFRESH, refresh);
        }
        if (worker) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                      JK_STATUS_ARG_WORKER, worker);
        }
        if (sub_worker) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                      JK_STATUS_ARG_WORKER_MEMBER, sub_worker);
        }
        if (option) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                      JK_STATUS_ARG_OPTION, option);
        }
    }
}

static void status_write_uri(jk_ws_service_t *s, const char *text,
                             int cmd, int mime,
                             int from, int refresh,
                             const char *worker, const char *sub_worker,
                             const char *option)
{
    if (text)
        jk_puts(s, "<a href=\"");
    if (cmd) {
        jk_putv(s, s->req_uri, "?", JK_STATUS_ARG_CMD, "=",
                status_cmd_text(cmd), NULL);
        if (mime)
            jk_putv(s, "&amp;", JK_STATUS_ARG_MIME, "=",
                    status_mime_text(mime), NULL);
        if (from)
            jk_putv(s, "&amp;", JK_STATUS_ARG_FROM, "=",
                    status_cmd_text(from), NULL);
        if (refresh)
            jk_printf(s, "&amp;%s=%d", JK_STATUS_ARG_REFRESH, refresh);
        if (worker)
            jk_putv(s, "&amp;", JK_STATUS_ARG_WORKER, "=",
                    worker, NULL);
        if (sub_worker)
            jk_putv(s, "&amp;", JK_STATUS_ARG_WORKER_MEMBER, "=",
                    sub_worker, NULL);
        if (option)
            jk_putv(s, "&amp;", JK_STATUS_ARG_OPTION, "=",
                    option, NULL);
    }
    if (text)
        jk_putv(s, "\">", text, "</a>", NULL);
}

static void status_parse_uri(jk_ws_service_t *s,
                             int *cmd, int *mime,
                             int *from, int *refresh,
                             char *worker, char *sub_worker,
                             char *option,
                             jk_logger_t *l)
{
    char buf[JK_SHM_STR_SIZ+1];

    JK_TRACE_ENTER(l);
    *cmd = JK_STATUS_CMD_LIST;
    *mime = JK_STATUS_MIME_HTML;
    *from = JK_STATUS_CMD_DEF;
    worker[0] = '\0';
    sub_worker[0] = '\0';
    option[0] = '\0';
    *refresh = 0;
    if (!s->query_string)
        return;
    if (status_get_arg_raw(JK_STATUS_ARG_CMD, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        *cmd = status_cmd_int(buf);
    if (status_get_arg_raw(JK_STATUS_ARG_MIME, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        *mime = status_mime_int(buf);
    if (status_get_arg_raw(JK_STATUS_ARG_FROM, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        *from = status_cmd_int(buf);
    *refresh = status_get_int(JK_STATUS_ARG_REFRESH, s->query_string, 0, l);
    if (status_get_arg(JK_STATUS_ARG_WORKER, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        strncpy(worker, buf, JK_SHM_STR_SIZ);
    if (status_get_arg(JK_STATUS_ARG_WORKER_MEMBER, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        strncpy(sub_worker, buf, JK_SHM_STR_SIZ);
    if (status_get_arg(JK_STATUS_ARG_OPTION, s->query_string, buf, sizeof(buf), l) == JK_TRUE)
        strncpy(option, buf, JK_SHM_STR_SIZ);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "standard request params cmd='%s' mime='%s' from='%s' refresh=%d "
               "worker='%s' sub worker='%s' option='%s'",
               status_cmd_text(*cmd), status_mime_text(*mime),
               status_cmd_text(*from), *refresh, worker, sub_worker, option);
    JK_TRACE_EXIT(l);
}

static void display_maps(jk_ws_service_t *s,
                         jk_uri_worker_map_t *uwmap,
                         const char *worker, jk_logger_t *l)
{
    char buf[64];
    unsigned int i;
    int count=0;

    JK_TRACE_ENTER(l);
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
    }
    if (!count) {
        jk_putv(s, "<hr/><h3>Warning: No URI Mappings defined for ",
                worker, " !</h3>\n", NULL);
        JK_TRACE_EXIT(l);
        return;    
    }
    count = 0;
    jk_putv(s, "<hr/><h3>URI Mappings for ", worker, "</h3>\n", NULL);
    jk_puts(s, "<table>\n");
    jk_printf(s, JK_STATUS_TABLE_HEAD_3_STRING,
              "Match Type", "Uri", "Source");
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
        jk_printf(s, JK_STATUS_TABLE_ROW_3_STRING,
                  uri_worker_map_get_match(uwr, buf, l), uwr->uri,
                  uri_worker_map_get_source(uwr, l));
    }
    jk_puts(s, "</table>\n");
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "displayed %d maps for worker '%s'",
               count, worker);
    JK_TRACE_EXIT(l);
}

static void display_maps_xml(jk_ws_service_t *s,
                             jk_uri_worker_map_t *uwmap,
                             status_worker_t *sw,
                             const char *worker, jk_logger_t *l)
{
    char buf[64];
    unsigned int i;
    int count=0;

    JK_TRACE_ENTER(l);
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
    }
    if (count) {
        jk_print_xml_start_elt(s, sw, 6, 0, "maps");
        jk_print_xml_att_int(s, 8, "size", count);
        jk_print_xml_stop_elt(s, 6, 0);
    }
    count = 0;
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
        jk_print_xml_start_elt(s, sw, 8, 0, "map");
        jk_print_xml_att_int(s, 10, "id", count);
        jk_print_xml_att_string(s, 10, "type", uri_worker_map_get_match(uwr, buf, l));
        jk_print_xml_att_string(s, 10, "uri", uwr->uri);
        jk_print_xml_att_string(s, 10, "source", uri_worker_map_get_source(uwr, l));
        jk_print_xml_stop_elt(s, 8, 1);
    }
    if (count)
        jk_print_xml_close_elt(s, sw, 6, "maps");
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "dumped %d maps for worker '%s'",
               count, worker);
    JK_TRACE_EXIT(l);
}

static void display_maps_txt(jk_ws_service_t *s,
                             jk_uri_worker_map_t *uwmap,
                             status_worker_t *sw,
                             const char *worker, jk_logger_t *l)
{
    char buf[64];
    unsigned int i;
    int count=0;

    JK_TRACE_ENTER(l);
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
    }
    if (count) {
        jk_printf(s, "Maps: size=%d\n", count);
    }
    count = 0;
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
        jk_puts(s, "Map:");
        jk_printf(s, " id=%d", count);
        jk_printf(s, " type=\"%s\"", uri_worker_map_get_match(uwr, buf, l));
        jk_printf(s, " uri=\"%s\"", uwr->uri);
        jk_printf(s, " source=\"%s\"", uri_worker_map_get_source(uwr, l));
        jk_puts(s, "\n");
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "dumped %d maps for worker '%s'",
               count, worker);
    JK_TRACE_EXIT(l);
}

static void display_maps_prop(jk_ws_service_t *s,
                              jk_uri_worker_map_t *uwmap,
                              status_worker_t *sw,
                              const char *worker, jk_logger_t *l)
{
    char buf[64];
    char *mount;
    char *off;
    unsigned int i;
    int count=0;

    JK_TRACE_ENTER(l);
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
    }
    if (count) {
        jk_print_prop_att_int(s, sw, worker, "maps.size", count);
    }
    count = 0;
    for (i = 0; i < uwmap->size; i++) {
        uri_worker_record_t *uwr = uwmap->maps[i];
        if (!worker || strcmp(uwr->worker_name, worker)) {
            continue;
        }
        count++;
        mount = jk_pool_alloc(s->pool, sizeof(char *) * (strlen(uwr->uri)+3));
        off = mount;
        if (uwr->match_type & MATCH_TYPE_DISABLED) {
            *off = '-';
            off++;
        }
        if (uwr->match_type & MATCH_TYPE_NO_MATCH) {
            *off = '!';
            off++;
        }
        strcpy(off, uwr->uri);
        jk_print_prop_att_string(s, sw, worker, "mount", mount);
        jk_print_prop_item_string(s, sw, worker, "maps.map", count, "type", uri_worker_map_get_match(uwr, buf, l));
        jk_print_prop_item_string(s, sw, worker, "maps.map", count, "uri", uwr->uri);
        jk_print_prop_item_string(s, sw, worker, "maps.map", count, "source", uri_worker_map_get_source(uwr, l));
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "dumped %d maps for worker '%s'",
               count, worker);
    JK_TRACE_EXIT(l);
}

static void display_worker(jk_ws_service_t *s, jk_worker_t *w,
                           status_worker_t *sw,
                           int show_members, int refresh, int single,
                           jk_logger_t *l)
{
    char buf[32];
    const char *name = NULL;
    int from = JK_STATUS_CMD_LIST;
    ajp_worker_t *aw = NULL;
    lb_worker_t *lb = NULL;

    JK_TRACE_ENTER(l);
    if (single) {
        from = JK_STATUS_CMD_SHOW;
    }
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s lb worker '%s'",
                   single ? "showing" : "listing", name);
    }
    else if (w->type == JK_AJP13_WORKER_TYPE ||
             w->type == JK_AJP14_WORKER_TYPE) {
        aw = (ajp_worker_t *)w->worker_private;
        name = aw->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s ajp worker '%s'",
                   single ? "showing" : "listing", name);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        time_t now = time(NULL);
        unsigned int good = 0;
        unsigned int degraded = 0;
        unsigned int bad = 0;
        unsigned int j;

        jk_shm_lock();
        if (lb->sequence != lb->s->sequence)
            jk_lb_pull(lb, l);
        jk_shm_unlock();

        jk_puts(s, "<hr/><h3>[");
        if (single) {
            jk_puts(s, "S");
        }
        else {
            if (show_members) {
                status_write_uri(s, "S", JK_STATUS_CMD_SHOW,
                                 0, 0, refresh, name, NULL, NULL);
            }
            else {
                status_write_uri(s, "S", JK_STATUS_CMD_SHOW,
                                 0, 0, refresh, name, NULL, JK_STATUS_ATT_NO_MEMBERS);
            }
        }
        jk_puts(s, "|");
        status_write_uri(s, "E", JK_STATUS_CMD_EDIT,
                         0, from, refresh, name, NULL, NULL);
        jk_puts(s, "|");
        status_write_uri(s, "R", JK_STATUS_CMD_RESET,
                         0, from, refresh, name, NULL, NULL);
        jk_puts(s, "]&nbsp;&nbsp;");
        jk_putv(s, "Worker Status for ", name, "</h3>\n", NULL);
        jk_puts(s, "<table>" JK_STATUS_SHOW_LB_HEAD);
        jk_printf(s, JK_STATUS_SHOW_LB_ROW,
                  status_worker_type(w->type),
                  status_val_bool(lb->sticky_session),
                  status_val_bool(lb->sticky_session_force),
                  lb->retries,
                  jk_lb_get_method(lb, l),
                  jk_lb_get_lock(lb, l),
                  lb->recover_wait_time);
        jk_puts(s, "</table>\n<br/>\n");

        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            int rate;
            rate = status_rate(wr, sw, l);
            if (rate > 0 )
               good++;
            else if (rate < 0 )
               bad++;
            else
               degraded++;
        }

        jk_puts(s, "<table><tr>"
                "<th>Good</th><th>Degraded</th><th>Bad/Stopped</th><th>Busy</th><th>Max Busy</th>"
                "</tr>\n<tr>");
        jk_printf(s, "<td>%d</td>", good);
        jk_printf(s, "<td>%d</td>", degraded);
        jk_printf(s, "<td>%d</td>", bad);
        jk_printf(s, "<td>%d</td>", lb->s->busy);
        jk_printf(s, "<td>%d</td>", lb->s->max_busy);
        jk_puts(s, "</tr>\n</table>\n\n");
        if (show_members) {
            jk_puts(s, "<h4>Balancer Members [");
            if (single) {
                status_write_uri(s, "Hide", JK_STATUS_CMD_SHOW,
                                 JK_STATUS_MIME_HTML, from, refresh, name, NULL, JK_STATUS_ATT_NO_MEMBERS);
            }
            else {
                status_write_uri(s, "Hide", JK_STATUS_CMD_LIST,
                                 JK_STATUS_MIME_HTML, from, refresh, name, NULL, JK_STATUS_ATT_NO_MEMBERS);
            }
            jk_puts(s, "]</h4>\n");
            jk_puts(s, "<table>" JK_STATUS_SHOW_MEMBER_HEAD);
            for (j = 0; j < lb->num_of_workers; j++) {
                worker_record_t *wr = &(lb->lb_workers[j]);
                ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
                int rs = 0;
                if (wr->s->state == JK_LB_STATE_ERROR) {
                    rs = lb->maintain_time - (int)difftime(now, lb->s->last_maintain_time);
                    if (rs < lb->recover_wait_time - (int)difftime(now, wr->s->error_time))
                        rs += lb->maintain_time;
                }
                jk_puts(s, "<tr>\n<td>[");
                status_write_uri(s, "E", JK_STATUS_CMD_EDIT,
                                 0, from, refresh, name, wr->s->name, NULL);
                jk_puts(s, "|");
                status_write_uri(s, "R", JK_STATUS_CMD_RESET,
                                 0, from, refresh, name, wr->s->name, NULL);
                jk_puts(s, "]&nbsp;</td>");
                jk_printf(s, JK_STATUS_SHOW_MEMBER_ROW,
                          wr->s->name,
                          status_worker_type(wr->w->type),
                          a->host, a->port,
                          jk_dump_hinfo(&a->worker_inet_addr, buf),
                          jk_lb_get_activation(wr, l),
                          jk_lb_get_state(wr, l),
                          wr->s->distance,
                          wr->s->lb_factor,
                          wr->s->lb_mult,
                          wr->s->lb_value,
                          wr->s->elected,
                          wr->s->errors,
                          wr->s->client_errors,
                          status_strfsize(wr->s->transferred, buf),
                          status_strfsize(wr->s->readed, buf),
                          wr->s->busy,
                          wr->s->max_busy,
                          wr->s->route,
                          wr->s->redirect ? (*wr->s->redirect ? wr->s->redirect : "&nbsp;") : "&nbsp",
                          wr->s->domain ? (*wr->s->domain ? wr->s->domain : "&nbsp;") : "&nbsp",
                          rs);
            }
            jk_puts(s, "</table><br/>\n");
            jk_puts(s, "<b>E</b>dit one attribute for all members: [");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_ACTIVATION, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_ACTIVATION);
            jk_puts(s, "\n|");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_FACTOR, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_FACTOR);
            jk_puts(s, "\n|");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_ROUTE, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_ROUTE);
            jk_puts(s, "\n|");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_REDIRECT, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_REDIRECT);
            jk_puts(s, "\n|");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_DOMAIN, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_DOMAIN);
            jk_puts(s, "\n|");
            status_write_uri(s, JK_STATUS_ARG_LBM_TEXT_DISTANCE, JK_STATUS_CMD_EDIT,
                             0, from, refresh, name, NULL, JK_STATUS_ARG_LBM_DISTANCE);
            jk_puts(s, "\n]<br/>\n");
        }
        else {
            jk_puts(s, "<p>\n");
            if (single) {
                status_write_uri(s, "Show Balancer Members", JK_STATUS_CMD_SHOW,
                                 JK_STATUS_MIME_HTML, from, refresh, name, NULL, NULL);
            }
            else {
                status_write_uri(s, "Show Balancer Members", JK_STATUS_CMD_LIST,
                                 JK_STATUS_MIME_HTML, from, refresh, name, NULL, NULL);
            }
            jk_puts(s, "</p>\n");
        }

    }
    else if (aw) {
        jk_puts(s, "<hr/><h3>[");
        if (single)
            jk_puts(s, "S");
        else
            status_write_uri(s, "S", JK_STATUS_CMD_SHOW,
                             0, from, refresh, name, NULL, NULL);
        jk_puts(s, "]&nbsp;&nbsp;");
        jk_putv(s, "Worker Status for ", name, "<h3/>\n", NULL);
        jk_puts(s, "<table>" JK_STATUS_SHOW_AJP_HEAD);
        jk_printf(s, JK_STATUS_SHOW_AJP_ROW,
                  status_worker_type(w->type),
                  aw->host, aw->port,
                  jk_dump_hinfo(&aw->worker_inet_addr, buf));
        jk_puts(s, "</table>\n");
    }
    if (name)
        display_maps(s, s->uw_map, name, l);
    JK_TRACE_EXIT(l);
}

static void display_worker_xml(jk_ws_service_t *s, jk_worker_t *w,
                               status_worker_t *sw, int show_members, int single,
                               jk_logger_t *l)
{
    char buf[32];
    const char *name = NULL;
    ajp_worker_t *aw = NULL;
    lb_worker_t *lb = NULL;

    JK_TRACE_ENTER(l);
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s lb worker '%s' as xml",
                   single ? "showing" : "listing", name);
    }
    else if (w->type == JK_AJP13_WORKER_TYPE ||
             w->type == JK_AJP14_WORKER_TYPE) {
        aw = (ajp_worker_t *)w->worker_private;
        name = aw->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s ajp worker '%s' as xml",
                   single ? "showing" : "listing", name);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        time_t now = time(NULL);
        unsigned int good = 0;
        unsigned int degraded = 0;
        unsigned int bad = 0;
        unsigned int j;

        jk_shm_lock();
        if (lb->sequence != lb->s->sequence)
            jk_lb_pull(lb, l);
        jk_shm_unlock();

        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            int rate;
            rate = status_rate(wr, sw, l);
            if (rate > 0 )
               good++;
            else if (rate < 0 )
               bad++;
            else
               degraded++;
        }

        jk_print_xml_start_elt(s, sw, 2, 0, "balancer");
        jk_print_xml_att_string(s, 4, "name", name);
        jk_print_xml_att_string(s, 4, "type", status_worker_type(w->type));
        jk_print_xml_att_string(s, 4, "sticky_session", status_val_bool(lb->sticky_session));
        jk_print_xml_att_string(s, 4, "sticky_session_force", status_val_bool(lb->sticky_session_force));
        jk_print_xml_att_int(s, 4, "retries", lb->retries);
        jk_print_xml_att_int(s, 4, "recover_time", lb->recover_wait_time);
        jk_print_xml_att_string(s, 4, "method", jk_lb_get_method(lb, l));
        jk_print_xml_att_string(s, 4, "lock", jk_lb_get_lock(lb, l));
        jk_print_xml_att_int(s, 4, "member_count", lb->num_of_workers);
        jk_print_xml_att_int(s, 4, "good", good);
        jk_print_xml_att_int(s, 4, "degraded", degraded);
        jk_print_xml_att_int(s, 4, "bad", bad);
        jk_print_xml_att_int(s, 4, "busy", lb->s->busy);
        jk_print_xml_att_int(s, 4, "max_busy", lb->s->max_busy);
        jk_print_xml_stop_elt(s, 2, 0);

        if (show_members) {
            for (j = 0; j < lb->num_of_workers; j++) {
                worker_record_t *wr = &(lb->lb_workers[j]);
                ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
                int rs = 0;
                if (wr->s->state == JK_LB_STATE_ERROR) {
                    rs = lb->maintain_time - (int)difftime(now, lb->s->last_maintain_time);
                    if (rs < lb->recover_wait_time - (int)difftime(now, wr->s->error_time))
                        rs += lb->maintain_time;
                }
                /* TODO: descriptive status */
                jk_print_xml_start_elt(s, sw, 6, 0, "member");
                jk_print_xml_att_string(s, 8, "name", wr->s->name);
                jk_print_xml_att_string(s, 8, "type", status_worker_type(wr->w->type));
                jk_print_xml_att_string(s, 8, "host", a->host);
                jk_print_xml_att_int(s, 8, "port", a->port);
                jk_print_xml_att_string(s, 8, "address", jk_dump_hinfo(&a->worker_inet_addr, buf));
                jk_print_xml_att_string(s, 8, "activation", jk_lb_get_activation(wr, l));
                jk_print_xml_att_int(s, 8, "lbfactor", wr->s->lb_factor);
                jk_print_xml_att_string(s, 8, "route", wr->s->route);
                jk_print_xml_att_string(s, 8, "redirect", wr->s->redirect);
                jk_print_xml_att_string(s, 8, "domain", wr->s->domain);
                jk_print_xml_att_int(s, 8, "distance", wr->s->distance);
                jk_print_xml_att_string(s, 8, "state", jk_lb_get_state(wr, l));
                jk_print_xml_att_uint64(s, 8, "lbmult", wr->s->lb_mult);
                jk_print_xml_att_uint64(s, 8, "lbvalue", wr->s->lb_value);
                jk_print_xml_att_uint64(s, 8, "elected", wr->s->elected);
                jk_print_xml_att_uint32(s, 8, "errors", wr->s->errors);
                jk_print_xml_att_uint32(s, 8, "client_errors", wr->s->client_errors);
                jk_print_xml_att_uint64(s, 8, "transferred", wr->s->transferred);
                jk_print_xml_att_uint64(s, 8, "read", wr->s->readed);
                jk_print_xml_att_uint(s, 8, "busy", wr->s->busy);
                jk_print_xml_att_uint(s, 8, "max_busy", wr->s->max_busy);
                jk_print_xml_att_uint(s, 8, "time_to_recover", rs < 0 ? 0 : rs);
                /* Terminate the tag */
                jk_print_xml_stop_elt(s, 6, 1);
            }
        }
        if (name)
            display_maps_xml(s, s->uw_map, sw, name, l);
        jk_print_xml_close_elt(s, sw, 2, "balancer");
    }
    else if (aw) {
        jk_print_xml_start_elt(s, sw, 0, 0, "ajp");
        jk_print_xml_att_string(s, 2, "name", name);
        jk_print_xml_att_string(s, 2, "type", status_worker_type(w->type));
        jk_print_xml_att_string(s, 2, "host", aw->host);
        jk_print_xml_att_int(s, 2, "port", aw->port);
        jk_print_xml_att_string(s, 2, "address", jk_dump_hinfo(&aw->worker_inet_addr, buf));
        /* Terminate the tag */
        jk_print_xml_stop_elt(s, 0, 0);
        if (name)
            display_maps_xml(s, s->uw_map, sw, name, l);
        jk_print_xml_close_elt(s, sw, 0, "ajp");
    }
    JK_TRACE_EXIT(l);
}

static void display_worker_txt(jk_ws_service_t *s, jk_worker_t *w,
                               status_worker_t *sw, int show_members, int single,
                               jk_logger_t *l)
{
    char buf[32];
    const char *name = NULL;
    ajp_worker_t *aw = NULL;
    lb_worker_t *lb = NULL;

    JK_TRACE_ENTER(l);
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s lb worker '%s' as txt",
                   single ? "showing" : "listing", name);
    }
    else if (w->type == JK_AJP13_WORKER_TYPE ||
             w->type == JK_AJP14_WORKER_TYPE) {
        aw = (ajp_worker_t *)w->worker_private;
        name = aw->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s ajp worker '%s' as txt",
                   single ? "showing" : "listing", name);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        time_t now = time(NULL);
        unsigned int good = 0;
        unsigned int degraded = 0;
        unsigned int bad = 0;
        unsigned int j;

        jk_shm_lock();
        if (lb->sequence != lb->s->sequence)
            jk_lb_pull(lb, l);
        jk_shm_unlock();

        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            int rate;
            rate = status_rate(wr, sw, l);
            if (rate > 0 )
               good++;
            else if (rate < 0 )
               bad++;
            else
               degraded++;
        }

        jk_puts(s, "Balancer Worker:");
        jk_printf(s, " name=%s", name);
        jk_printf(s, " type=%s", status_worker_type(w->type));
        jk_printf(s, " sticky_session=%s", status_val_bool(lb->sticky_session));
        jk_printf(s, " sticky_session_force=%s", status_val_bool(lb->sticky_session_force));
        jk_printf(s, " retries=%d", lb->retries);
        jk_printf(s, " recover_time=%d", lb->recover_wait_time);
        jk_printf(s, " method=%s", jk_lb_get_method(lb, l));
        jk_printf(s, " lock=%s", jk_lb_get_lock(lb, l));
        jk_printf(s, " member_count=%d", lb->num_of_workers);
        jk_printf(s, " good=%d", good);
        jk_printf(s, " degraded=%d", degraded);
        jk_printf(s, " bad=%d", bad);
        jk_printf(s, " busy=%d", lb->s->busy);
        jk_printf(s, " max_busy=%d", lb->s->max_busy);
        jk_puts(s, "\n");

        if (show_members) {
            for (j = 0; j < lb->num_of_workers; j++) {
                worker_record_t *wr = &(lb->lb_workers[j]);
                ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
                int rs = 0;
                if (wr->s->state == JK_LB_STATE_ERROR) {
                    rs = lb->maintain_time - (int)difftime(now, lb->s->last_maintain_time);
                    if (rs < lb->recover_wait_time - (int)difftime(now, wr->s->error_time))
                        rs += lb->maintain_time;
                }
                jk_puts(s, "Member:");
                jk_printf(s, " name=%s", wr->s->name);
                jk_printf(s, " type=%s", status_worker_type(wr->w->type));
                jk_printf(s, " host=%s", a->host);
                jk_printf(s, " port=%d", a->port);
                jk_printf(s, " address=%s", jk_dump_hinfo(&a->worker_inet_addr, buf));
                jk_printf(s, " activation=%s", jk_lb_get_activation(wr, l));
                jk_printf(s, " lbfactor=%d", wr->s->lb_factor);
                jk_printf(s, " route=\"%s\"", wr->s->route ? wr->s->route : "");
                jk_printf(s, " redirect=\"%s\"", wr->s->redirect ? wr->s->redirect : "");
                jk_printf(s, " domain=\"%s\"", wr->s->domain ? wr->s->domain : "");
                jk_printf(s, " distance=%d", wr->s->distance);
                jk_printf(s, " state=%s", jk_lb_get_state(wr, l));
                jk_printf(s, " lbmult=%" JK_UINT64_T_FMT, wr->s->lb_mult);
                jk_printf(s, " lbvalue=%" JK_UINT64_T_FMT, wr->s->lb_value);
                jk_printf(s, " elected=%" JK_UINT64_T_FMT, wr->s->elected);
                jk_printf(s, " errors=%" JK_UINT32_T_FMT, wr->s->errors);
                jk_printf(s, " client_errors=%" JK_UINT32_T_FMT, wr->s->client_errors);
                jk_printf(s, " transferred=%" JK_UINT64_T_FMT, wr->s->transferred);
                jk_printf(s, " read=%" JK_UINT64_T_FMT, wr->s->readed);
                jk_printf(s, " busy=%u", wr->s->busy);
                jk_printf(s, " max_busy=%u", wr->s->max_busy);
                jk_printf(s, " time_to_recover=%u", rs < 0 ? 0 : rs);
                jk_puts(s, "\n");
            }
        }
    }
    else if (aw) {
        jk_puts(s, "AJP Worker:");
        jk_printf(s, " name=%s", name);
        jk_printf(s, " type=%s", status_worker_type(w->type));
        jk_printf(s, " host=%s", aw->host);
        jk_printf(s, " port=%d", aw->port);
        jk_printf(s, " address=%s", jk_dump_hinfo(&aw->worker_inet_addr, buf));
        jk_puts(s, "\n");
    }
    if (name)
        display_maps_txt(s, s->uw_map, sw, name, l);
    JK_TRACE_EXIT(l);
}

static void display_worker_prop(jk_ws_service_t *s, jk_worker_t *w,
                                status_worker_t *sw, int show_members, int single,
                                jk_logger_t *l)
{
    char buf[32];
    const char *name = NULL;
    ajp_worker_t *aw = NULL;
    lb_worker_t *lb = NULL;

    JK_TRACE_ENTER(l);
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s lb worker '%s' as txt",
                   single ? "showing" : "listing", name);
    }
    else if (w->type == JK_AJP13_WORKER_TYPE ||
             w->type == JK_AJP14_WORKER_TYPE) {
        aw = (ajp_worker_t *)w->worker_private;
        name = aw->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "%s ajp worker '%s' as txt",
                   single ? "showing" : "listing", name);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    jk_print_prop_att_string(s, sw, NULL, "list", name);
    if (lb) {
        time_t now = time(NULL);
        unsigned int good = 0;
        unsigned int degraded = 0;
        unsigned int bad = 0;
        unsigned int j;

        jk_shm_lock();
        if (lb->sequence != lb->s->sequence)
            jk_lb_pull(lb, l);
        jk_shm_unlock();

        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            int rate;
            rate = status_rate(wr, sw, l);
            if (rate > 0 )
               good++;
            else if (rate < 0 )
               bad++;
            else
               degraded++;
        }

        jk_print_prop_att_string(s, sw, name, "type", status_worker_type(w->type));
        jk_print_prop_att_string(s, sw, name, "sticky_session", status_val_bool(lb->sticky_session));
        jk_print_prop_att_string(s, sw, name, "sticky_session_force", status_val_bool(lb->sticky_session_force));
        jk_print_prop_att_int(s, sw, name, "retries", lb->retries);
        jk_print_prop_att_int(s, sw, name, "recover_time", lb->recover_wait_time);
        jk_print_prop_att_string(s, sw, name, "method", jk_lb_get_method(lb, l));
        jk_print_prop_att_string(s, sw, name, "lock", jk_lb_get_lock(lb, l));
        jk_print_prop_att_int(s, sw, name, "member_count", lb->num_of_workers);
        jk_print_prop_att_int(s, sw, name, "good", good);
        jk_print_prop_att_int(s, sw, name, "degraded", degraded);
        jk_print_prop_att_int(s, sw, name, "bad", bad);
        jk_print_prop_att_int(s, sw, name, "busy", lb->s->busy);
        jk_print_prop_att_int(s, sw, name, "max_busy", lb->s->max_busy);

        if (show_members) {
            for (j = 0; j < lb->num_of_workers; j++) {
                worker_record_t *wr = &(lb->lb_workers[j]);
                ajp_worker_t *a = (ajp_worker_t *)wr->w->worker_private;
                int rs = 0;
                if (wr->s->state == JK_LB_STATE_ERROR) {
                    rs = lb->maintain_time - (int)difftime(now, lb->s->last_maintain_time);
                    if (rs < lb->recover_wait_time - (int)difftime(now, wr->s->error_time))
                        rs += lb->maintain_time;
                }
                jk_print_prop_att_string(s, sw, name, "balance_workers", wr->s->name);
                jk_print_prop_att_string(s, sw, wr->s->name, "type", status_worker_type(wr->w->type));
                jk_print_prop_att_string(s, sw, wr->s->name, "host", a->host);
                jk_print_prop_att_int(s, sw, wr->s->name, "port", a->port);
                jk_print_prop_att_string(s, sw, wr->s->name, "address", jk_dump_hinfo(&a->worker_inet_addr, buf));
                jk_print_prop_att_string(s, sw, wr->s->name, "activation", jk_lb_get_activation(wr, l));
                jk_print_prop_att_int(s, sw, wr->s->name, "lbfactor", wr->s->lb_factor);
                jk_print_prop_att_string(s, sw, wr->s->name, "route", wr->s->route);
                jk_print_prop_att_string(s, sw, wr->s->name, "redirect", wr->s->redirect);
                jk_print_prop_att_string(s, sw, wr->s->name, "domain", wr->s->domain);
                jk_print_prop_att_int(s, sw, wr->s->name, "distance", wr->s->distance);
                jk_print_prop_att_string(s, sw, wr->s->name, "state", jk_lb_get_state(wr, l));
                jk_print_prop_att_uint64(s, sw, wr->s->name, "lbmult", wr->s->lb_mult);
                jk_print_prop_att_uint64(s, sw, wr->s->name, "lbvalue", wr->s->lb_value);
                jk_print_prop_att_uint64(s, sw, wr->s->name, "elected", wr->s->elected);
                jk_print_prop_att_uint32(s, sw, wr->s->name, "errors", wr->s->errors);
                jk_print_prop_att_uint32(s, sw, wr->s->name, "client_errors", wr->s->client_errors);
                jk_print_prop_att_uint64(s, sw, wr->s->name, "transferred", wr->s->transferred);
                jk_print_prop_att_uint64(s, sw, wr->s->name, "read", wr->s->readed);
                jk_print_prop_att_uint(s, sw, wr->s->name, "busy", wr->s->busy);
                jk_print_prop_att_uint(s, sw, wr->s->name, "max_busy", wr->s->max_busy);
                jk_print_prop_att_uint(s, sw, wr->s->name, "time_to_recover", rs < 0 ? 0 : rs);
            }
        }
    }
    else if (aw) {
        jk_print_prop_att_string(s, sw, name, "type", status_worker_type(w->type));
        jk_print_prop_att_string(s, sw, name, "host", aw->host);
        jk_print_prop_att_int(s, sw, name, "port", aw->port);
        jk_print_prop_att_string(s, sw, name, "address", jk_dump_hinfo(&aw->worker_inet_addr, buf));
    }
    if (name)
        display_maps_prop(s, s->uw_map, sw, name, l);
    JK_TRACE_EXIT(l);
}

static void form_worker(jk_ws_service_t *s, jk_worker_t *w,
                        int from, int refresh,
                        jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;

    JK_TRACE_ENTER(l);
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "producing edit form for lb worker '%s'",
                   name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (!lb) {
        jk_log(l, JK_LOG_WARNING,
               "lb structure is (NULL)");
        JK_TRACE_EXIT(l);
        return;
    }

    jk_putv(s, "<hr/><h3>Edit load balancer settings for ",
            name, "</h3>\n", NULL);

    status_start_form(s, "GET", JK_STATUS_CMD_UPDATE,
                      0, from, refresh, name, NULL, NULL);

    jk_putv(s, "<table>\n<tr><td>", JK_STATUS_ARG_LB_TEXT_RETRIES,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_RETRIES, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->retries);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_RECOVER_TIME,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_RECOVER_TIME, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->recover_wait_time);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_STICKY,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_STICKY, "\" type=\"checkbox\"", NULL);
    if (lb->sticky_session)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_STICKY_FORCE,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_STICKY_FORCE, "\" type=\"checkbox\"", NULL);
    if (lb->sticky_session_force)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_METHOD,
            ":</td><td></td></tr>\n", NULL);
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Requests</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_REQUESTS);
    if (lb->lbmethod == JK_LB_METHOD_REQUESTS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Traffic</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_TRAFFIC);
    if (lb->lbmethod == JK_LB_METHOD_TRAFFIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Busyness</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_BUSYNESS);
    if (lb->lbmethod == JK_LB_METHOD_BUSYNESS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Sessions</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_SESSIONS);
    if (lb->lbmethod == JK_LB_METHOD_SESSIONS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_LOCK,
            ":</td><td></td></tr>\n", NULL);
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Optimistic</td><td><input name=\"",
            JK_STATUS_ARG_LB_LOCK, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_LOCK_OPTIMISTIC);
    if (lb->lblock == JK_LB_LOCK_OPTIMISTIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Pessimistic</td><td><input name=\"",
            JK_STATUS_ARG_LB_LOCK, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_LOCK_PESSIMISTIC);
    if (lb->lblock == JK_LB_LOCK_PESSIMISTIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_puts(s, "</table>\n");
    jk_puts(s, "<br/><input type=\"submit\" value=\"Update Balancer\"/></form>\n");

    JK_TRACE_EXIT(l);
}

static void form_member(jk_ws_service_t *s, worker_record_t *wr,
                        const char *worker,
                        int from, int refresh,
                        jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "producing edit form for sub worker '%s' of lb worker '%s'",
               wr->s->name, worker);

    jk_putv(s, "<hr/><h3>Edit worker settings for ",
            wr->s->name, "</h3>\n", NULL);
    status_start_form(s, "GET", JK_STATUS_CMD_UPDATE,
                      0, from, refresh, worker, wr->s->name, NULL);

    jk_puts(s, "<table>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_ACTIVATION,
            ":</td><td></td></tr>\n", NULL);
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Active</td><td><input name=\"",
            JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_ACTIVE);
    if (wr->s->activation == JK_LB_ACTIVATION_ACTIVE)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Disabled</td><td><input name=\"",
            JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_DISABLED);
    if (wr->s->activation == JK_LB_ACTIVATION_DISABLED)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Stopped</td><td><input name=\"",
            JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_STOPPED);
    if (wr->s->activation == JK_LB_ACTIVATION_STOPPED)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_FACTOR,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LBM_FACTOR, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", wr->s->lb_factor);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_ROUTE,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LBM_ROUTE, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%s\"/></td></tr>\n", wr->s->route);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_REDIRECT,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LBM_REDIRECT, "\" type=\"text\" ", NULL);
    jk_putv(s, "value=\"", wr->s->redirect, NULL);
    jk_puts(s, "\"/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_DOMAIN,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LBM_DOMAIN, "\" type=\"text\" ", NULL);
    jk_putv(s, "value=\"", wr->s->domain, NULL);
    jk_puts(s, "\"/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_DISTANCE,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LBM_DISTANCE, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", wr->s->distance);
    jk_puts(s, "</table>\n");
    jk_puts(s, "<br/><input type=\"submit\" value=\"Update Worker\"/>\n</form>\n");
    JK_TRACE_EXIT(l);
}

static void form_all_members(jk_ws_service_t *s, jk_worker_t *w,
                             const char *attribute,
                             int from, int refresh,
                             jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    const char *aname;
    unsigned int i;

    JK_TRACE_ENTER(l);
    if (!attribute) {
        jk_log(l, JK_LOG_WARNING,
               "missing request parameter '%s'",
               JK_STATUS_ARG_OPTION);
        JK_TRACE_EXIT(l);
        return;
    }
    else {
        if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION))
            aname=JK_STATUS_ARG_LBM_TEXT_ACTIVATION;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR))
            aname=JK_STATUS_ARG_LBM_TEXT_FACTOR;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE))
            aname=JK_STATUS_ARG_LBM_TEXT_ROUTE;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT))
            aname=JK_STATUS_ARG_LBM_TEXT_REDIRECT;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN))
            aname=JK_STATUS_ARG_LBM_TEXT_DOMAIN;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE))
            aname=JK_STATUS_ARG_LBM_TEXT_DISTANCE;
        else {
            jk_log(l, JK_LOG_WARNING,
                   "unknown attribute '%s'",
                   attribute);
            JK_TRACE_EXIT(l);
            return;
        }
    }
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "producing edit form for attribute '%s' [%s] of all members of lb worker '%s'",
                   attribute, aname, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        jk_putv(s, "<hr/><h3>Edit attribute '", aname,
                "' for all members of load balancer ",
                name, "</h3>\n", NULL);

        status_start_form(s, "GET", JK_STATUS_CMD_UPDATE,
                          0, from, refresh, name, NULL, attribute);

        jk_putv(s, "<table><tr>"
                "<th>Balanced Worker</th><th>", aname, "</th>"
                "</tr>", NULL);

        for (i = 0; i < lb->num_of_workers; i++) {
            worker_record_t *wr = &(lb->lb_workers[i]);

            jk_putv(s, "<tr><td>", wr->s->name, "</td><td>\n", NULL);

            if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION)) {

                jk_printf(s, "Active:&nbsp;<input name=\"val%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_ACTIVE);
                if (wr->s->activation == JK_LB_ACTIVATION_ACTIVE)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>&nbsp;|&nbsp;\n");
                jk_printf(s, "Disabled:&nbsp;<input name=\"val%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_DISABLED);
                if (wr->s->activation == JK_LB_ACTIVATION_DISABLED)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>&nbsp;|&nbsp;\n");
                jk_printf(s, "Stopped:&nbsp;<input name=\"val%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_STOPPED);
                if (wr->s->activation == JK_LB_ACTIVATION_STOPPED)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>\n");

            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR)) {
                jk_printf(s, "<input name=\"val%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", wr->s->lb_factor);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE)) {
                jk_printf(s, "<input name=\"val%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->s->route, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT)) {
                jk_printf(s, "<input name=\"val%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->s->redirect, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN)) {
                jk_printf(s, "<input name=\"val%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->s->domain, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE)) {
                jk_printf(s, "<input name=\"val%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", wr->s->distance);
            }

            jk_puts(s, "</td></tr>");
        }

        jk_puts(s, "</table>\n");
        jk_puts(s, "<br/><input type=\"submit\" value=\"Update Balancer\"/></form>\n");
    }
    JK_TRACE_EXIT(l);
}

static void commit_worker(jk_ws_service_t *s, jk_worker_t *w,
                          jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    int i;

    JK_TRACE_ENTER(l);
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "committing changes for lb worker '%s'",
                   name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (!lb) {
        jk_log(l, JK_LOG_WARNING,
               "lb structure is (NULL)");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb->sequence != lb->s->sequence)
        jk_lb_pull(lb, l);

    i = status_get_int(JK_STATUS_ARG_LB_RETRIES,
                       s->query_string, lb->retries, l);
    if (i != lb->retries && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'retries' for lb worker '%s' to '%i'",
               name, i);
        lb->retries = i;
    }
    i = status_get_int(JK_STATUS_ARG_LB_RECOVER_TIME,
                       s->query_string, lb->recover_wait_time, l);
    if (i != lb->recover_wait_time && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'recover_time' for lb worker '%s' to '%i'",
               name, i);
        lb->recover_wait_time = i;
    }
    i = status_get_bool(JK_STATUS_ARG_LB_STICKY,
                        s->query_string, 0, l);
    if (i != lb->sticky_session && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'sticky_session' for lb worker '%s' to '%i'",
               name, i);
        lb->sticky_session = i;
    }
    i = status_get_bool(JK_STATUS_ARG_LB_STICKY_FORCE,
                        s->query_string, 0, l);
    if (i != lb->sticky_session_force && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'sticky_session_force' for lb worker '%s' to '%i'",
               name, i);
        lb->sticky_session_force = i;
    }
    i = status_get_int(JK_STATUS_ARG_LB_METHOD,
                       s->query_string, lb->lbmethod, l);
    if (i != lb->lbmethod && i > 0 && i <= JK_LB_METHOD_MAX) {
        jk_log(l, JK_LOG_INFO,
               "setting 'method' for lb worker '%s' to '%i'",
               name, i);
        lb->lbmethod = i;
    }
    i = status_get_int(JK_STATUS_ARG_LB_LOCK,
                       s->query_string, lb->lblock, l);
    if (i != lb->lblock && i > 0 && i <= JK_LB_LOCK_MAX) {
        jk_log(l, JK_LOG_INFO,
               "setting 'lock' for lb worker '%s' to '%i'",
               name, i);
        lb->lblock = i;
    }
    lb->sequence++;
    jk_lb_push(lb, l);
}

static int commit_member(jk_ws_service_t *s, worker_record_t *wr,
                          const char *worker,
                          jk_logger_t *l)
{
    char buf[128];
    int rc = 0;
    int rv;
    int i;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "committing changes for sub worker '%s' of lb worker '%s'",
               wr->s->name, worker);

    i = status_get_int(JK_STATUS_ARG_LBM_ACTIVATION,
                       s->query_string, wr->s->activation, l);
    if (i != wr->s->activation && i > 0 && i<= JK_LB_ACTIVATION_MAX) {
        jk_log(l, JK_LOG_INFO,
               "setting 'activation' for sub worker '%s' of lb worker '%s' to '%s'",
               wr->s->name, worker, jk_lb_get_activation(wr, l));
        wr->s->activation = i;
        rc |= 1;
    }
    i = status_get_int(JK_STATUS_ARG_LBM_FACTOR,
                       s->query_string, wr->s->lb_factor, l);
    if (i != wr->s->lb_factor && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'lbfactor' for sub worker '%s' of lb worker '%s' to '%i'",
               wr->s->name, worker, i);
        wr->s->lb_factor = i;
        /* Recalculate the load multiplicators wrt. lb_factor */
        rc |= 2;
    }
    if ((rv = status_get_arg(JK_STATUS_ARG_LBM_ROUTE,
                             s->query_string, buf, sizeof(buf), l)) == JK_TRUE) {
        if (strncmp(wr->s->route, buf, JK_SHM_STR_SIZ)) {
            jk_log(l, JK_LOG_INFO,
                   "setting 'route' for sub worker '%s' of lb worker '%s' to '%s'",
                   wr->s->name, worker, buf);
            strncpy(wr->s->route, buf, JK_SHM_STR_SIZ);
            if (!wr->s->domain[0]) {
                char * id_domain = strchr(wr->s->route, '.');
                if (id_domain) {
                    *id_domain = '\0';
                    strcpy(wr->s->domain, wr->s->route);
                    *id_domain = '.';
                }
            }
        }
    }
    else if (rv == JK_UNSET) {
        jk_log(l, JK_LOG_INFO,
               "resetting 'route' for sub worker '%s' of lb worker '%s'",
               wr->s->name, worker);
        memset(wr->s->route, 0, JK_SHM_STR_SIZ);
    }
    if ((rv = status_get_arg(JK_STATUS_ARG_LBM_REDIRECT,
                             s->query_string, buf, sizeof(buf), l)) == JK_TRUE) {
        if (strncmp(wr->s->redirect, buf, JK_SHM_STR_SIZ)) {
            jk_log(l, JK_LOG_INFO,
                   "setting 'redirect' for sub worker '%s' of lb worker '%s' to '%s'",
                   wr->s->name, worker, buf);
            strncpy(wr->s->redirect, buf, JK_SHM_STR_SIZ);
        }
    }
    else if (rv == JK_UNSET) {
        jk_log(l, JK_LOG_INFO,
               "resetting 'redirect' for sub worker '%s' of lb worker '%s'",
               wr->s->name, worker);
        memset(wr->s->redirect, 0, JK_SHM_STR_SIZ);
    }
    if ((rv = status_get_arg(JK_STATUS_ARG_LBM_DOMAIN,
                             s->query_string, buf, sizeof(buf), l)) == JK_TRUE) {
        if (strncmp(wr->s->domain, buf, JK_SHM_STR_SIZ)) {
            jk_log(l, JK_LOG_INFO,
                   "setting 'domain' for sub worker '%s' of lb worker '%s' to '%s'",
                   wr->s->name, worker, buf);
            strncpy(wr->s->domain, buf, JK_SHM_STR_SIZ);
        }
    }
    else if (rv == JK_UNSET) {
        jk_log(l, JK_LOG_INFO,
               "resetting 'domain' for sub worker '%s' of lb worker '%s'",
               wr->s->name, worker);
        memset(wr->s->domain, 0, JK_SHM_STR_SIZ);
    }
    i = status_get_int(JK_STATUS_ARG_LBM_DISTANCE,
                       s->query_string, wr->s->distance, l);
    if (i != wr->s->distance && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "setting 'distance' for sub worker '%s' of lb worker '%s' to '%i'",
               wr->s->name, worker, i);
        wr->s->distance = i;
    }
    return rc;
}

static void commit_all_members(jk_ws_service_t *s, jk_worker_t *w,
                               const char *attribute,
                               jk_logger_t *l)
{
    char buf[128];
    char vname[32];
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    const char *aname;
    int i;
    int rc = 0;
    unsigned int j;

    JK_TRACE_ENTER(l);
    if (!attribute) {
        jk_log(l, JK_LOG_WARNING,
               "missing request parameter '%s'",
               JK_STATUS_ARG_OPTION);
        JK_TRACE_EXIT(l);
        return;
    }
    else {
        if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION))
            aname=JK_STATUS_ARG_LBM_TEXT_ACTIVATION;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR))
            aname=JK_STATUS_ARG_LBM_TEXT_FACTOR;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE))
            aname=JK_STATUS_ARG_LBM_TEXT_ROUTE;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT))
            aname=JK_STATUS_ARG_LBM_TEXT_REDIRECT;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN))
            aname=JK_STATUS_ARG_LBM_TEXT_DOMAIN;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE))
            aname=JK_STATUS_ARG_LBM_TEXT_DISTANCE;
        else {
            jk_log(l, JK_LOG_WARNING,
                   "unknown attribute '%s'",
                   attribute);
            JK_TRACE_EXIT(l);
            return;
        }
    }
    if (w->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)w->worker_private;
        name = lb->s->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "committing changes for attribute '%s' [%s] of all members of lb worker '%s'",
                   attribute, aname, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "worker type not implemented");
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        for (j = 0; j < lb->num_of_workers; j++) {
            worker_record_t *wr = &(lb->lb_workers[j]);
            snprintf(vname, 32-1, "val%d", j);

            if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION)) {
                i = status_get_int(vname, s->query_string, wr->s->activation, l);
                if (i != wr->s->activation && i > 0 && i<= JK_LB_ACTIVATION_MAX) {
                    jk_log(l, JK_LOG_INFO,
                           "setting 'activation' for sub worker '%s' of lb worker '%s' to '%s'",
                           wr->s->name, name, jk_lb_get_activation(wr, l));
                    wr->s->activation = i;
                    rc = 1;
                }
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR)) {
                i = status_get_int(vname, s->query_string, wr->s->lb_factor, l);
                if (i != wr->s->lb_factor && i > 0) {
                    jk_log(l, JK_LOG_INFO,
                           "setting 'lbfactor' for sub worker '%s' of lb worker '%s' to '%i'",
                           wr->s->name, name, i);
                    wr->s->lb_factor = i;
                    rc = 2;
                }
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE)) {
                i = status_get_int(vname, s->query_string, wr->s->distance, l);
                if (i != wr->s->distance && i > 0) {
                    jk_log(l, JK_LOG_INFO,
                           "setting 'distance' for sub worker '%s' of lb worker '%s' to '%i'",
                           wr->s->name, name, i);
                    wr->s->lb_factor = i;
                }
            }
            else {
                int rv = status_get_arg(vname, s->query_string, buf, sizeof(buf), l);
                if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->s->route, buf, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "setting 'route' for sub worker '%s' of lb worker '%s' to '%s'",
                                   wr->s->name, name, buf);
                            strncpy(wr->s->route, buf, JK_SHM_STR_SIZ);
                            if (!wr->s->domain[0]) {
                                char * id_domain = strchr(wr->s->route, '.');
                                if (id_domain) {
                                    *id_domain = '\0';
                                    strcpy(wr->s->domain, wr->s->route);
                                    *id_domain = '.';
                                }
                            }
                        }
                    }
                    else if (rv == JK_UNSET) {
                        jk_log(l, JK_LOG_INFO,
                               "resetting 'route' for sub worker '%s' of lb worker '%s'",
                               wr->s->name, name);
                        memset(wr->s->route, 0, JK_SHM_STR_SIZ);
                    }
                }
                else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->s->redirect, buf, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "setting 'redirect' for sub worker '%s' of lb worker '%s' to '%s'",
                                   wr->s->name, name, buf);
                            strncpy(wr->s->redirect, buf, JK_SHM_STR_SIZ);
                        }
                    }
                    else if (rv == JK_UNSET) {
                        jk_log(l, JK_LOG_INFO,
                               "resetting 'redirect' for sub worker '%s' of lb worker '%s'",
                               wr->s->name, name);
                        memset(wr->s->redirect, 0, JK_SHM_STR_SIZ);
                    }
                }
                else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->s->domain, buf, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "setting 'domain' for sub worker '%s' of lb worker '%s' to '%s'",
                                   wr->s->name, name, buf);
                            strncpy(wr->s->domain, buf, JK_SHM_STR_SIZ);
                        }
                    }
                    else if (rv == JK_UNSET) {
                        jk_log(l, JK_LOG_INFO,
                               "resetting 'domain' for sub worker '%s' of lb worker '%s'",
                               wr->s->name, name);
                        memset(wr->s->domain, 0, JK_SHM_STR_SIZ);
                    }

                }
            }
        }
        if (rc == 1)
            reset_lb_values(lb, l);
        else if (rc == 2)
            /* Recalculate the load multiplicators wrt. lb_factor */
            update_mult(lb, l);
    }
    JK_TRACE_EXIT(l);
}




static void display_legend(jk_ws_service_t *s, status_worker_t *sw, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    jk_puts(s, "<hr/><h2>Legend</h2><table>\n"
            "<tbody valign=\"baseline\">\n"
            "<tr><th>Name</th><td>Worker name</td></tr>\n"
            "<tr><th>Type</th><td>Worker type</td></tr>\n"
            "<tr><th>Route</th><td>Worker route</td></tr>\n"
            "<tr><th>Addr</th><td>Backend Address info</td></tr>\n"
            "<tr><th>Act</th><td>Worker activation configuration</br>\n"
            "ACT=Active, DIS=Disabled, STP=Stopped</td></tr>\n"
            "<tr><th>Stat</th><td>Worker error status</br>\n"
            "OK=OK, N/A=Unknown, ERR=Error, REC=Recovering, BSY=Busy</td></tr>\n"
            "<tr><th>D</th><td>Worker distance</td></tr>\n"
            "<tr><th>F</th><td>Load Balancer factor</td></tr>\n"
            "<tr><th>M</th><td>Load Balancer multiplicity</td></tr>\n"
            "<tr><th>V</th><td>Load Balancer value</td></tr>\n"
            "<tr><th>Acc</th><td>Number of requests</td></tr>\n"
            "<tr><th>Err</th><td>Number of failed requests</td></tr>\n"
            "<tr><th>CE</th><td>Number of client errors</td></tr>\n"
            "<tr><th>Wr</th><td>Number of bytes transferred/min</td></tr>\n"
            "<tr><th>Rd</th><td>Number of bytes read/min</td></tr>\n"
            "<tr><th>Busy</th><td>Current number of busy connections</td></tr>\n"
            "<tr><th>Max</th><td>Maximum number of busy connections</td></tr>\n"
            "<tr><th>RR</th><td>Route redirect</td></tr>\n"
            "<tr><th>Cd</th><td>Cluster domain</td></tr>\n"
            "<tr><th>Rs</th><td>Recovery scheduled</td></tr>\n"
            "</tbody>\n"
            "</table>\n");

    JK_TRACE_EXIT(l);
}

static int check_worker(jk_ws_service_t *s,
                        const char *worker, const char *sub_worker,
                        jk_logger_t *l)
{
    unsigned int i;
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "checking worker '%s' sub worker '%s'",
               worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "NULL or EMPTY worker param");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    w = wc_get_worker_for_name(worker, l);
    if (!w) {
        jk_log(l, JK_LOG_WARNING,
               "could not find worker '%s'",
               worker);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (sub_worker && sub_worker[0]) {
        lb_worker_t *lb = NULL;
        worker_record_t *wr = NULL;
        if (w->type != JK_LB_WORKER_TYPE) {
            jk_log(l, JK_LOG_WARNING,
                   "worker type has o sub worker");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        lb = (lb_worker_t *)w->worker_private;
        if (!lb) {
            jk_log(l, JK_LOG_WARNING,
                   "lb structure is (NULL)");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        for (i = 0; i < (int)lb->num_of_workers; i++) {
            wr = &(lb->lb_workers[i]);
            if (strcmp(sub_worker, wr->s->name) == 0)
                break;
        }
        if (!wr || i == (int)lb->num_of_workers) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sub_worker);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static void count_workers_xml(jk_ws_service_t *s, status_worker_t *sw,
                             int *lb_cnt, int *ajp_cnt,
                             jk_logger_t *l)
{
    unsigned int i;
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);
    *lb_cnt = 0;
    *ajp_cnt = 0;
    for (i = 0; i < sw->we->num_of_workers; i++) {
        w = wc_get_worker_for_name(sw->we->worker_list[i], l);
        if (!w) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sw->we->worker_list[i]);
            continue;
        }
        if (w->type == JK_LB_WORKER_TYPE) {
            (*lb_cnt)++;
        }
        else if (w->type == JK_AJP13_WORKER_TYPE ||
                 w->type == JK_AJP14_WORKER_TYPE) {
            (*ajp_cnt)++;
        }
    }
    JK_TRACE_EXIT(l);
}

static void list_workers_type_mime(jk_ws_service_t *s, status_worker_t *sw,
                                   int list_lb, int mime,
                                   int show_members, int count,
                                   int refresh, jk_logger_t *l)
{
    unsigned int i;
    int lb_cnt = 0;
    int ajp_cnt = 0;
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);

    if (list_lb) {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, sw, 0, 0, "balancers");
            jk_print_xml_att_int(s, 2, "count", count);
            jk_print_xml_stop_elt(s, 0, 0);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_printf(s, "Balancer Workers: count=%d\n", lb_cnt);
        }
        else if (mime == JK_STATUS_MIME_PROP) {
            jk_print_prop_att_int(s, sw, NULL, "lb_cnt", count);
        }
#if 0
        else {
            jk_printf(s, "<hr/><h2>Listing %d load balancing worker%s</h2>\n", count, count>1 ? "s" : "");
        }
#endif
    }
    else {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, sw, 0, 0, "ajp_workers");
            jk_print_xml_att_int(s, 2, "count", count);
            jk_print_xml_stop_elt(s, 0, 0);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_printf(s, "AJP Workers: count=%d\n", ajp_cnt);
        }
        else if (mime == JK_STATUS_MIME_PROP) {
            jk_print_prop_att_int(s, sw, NULL, "ajp_cnt", count);
        }
        else {
            jk_printf(s, "<hr/><h2>Listing %d ajp worker%s</h2>\n", count, count>1 ? "s" : "");
        }
    }

    for (i = 0; i < sw->we->num_of_workers; i++) {
        w = wc_get_worker_for_name(sw->we->worker_list[i], l);
        if (!w) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sw->we->worker_list[i]);
            continue;
        }
        if ((list_lb && w->type == JK_LB_WORKER_TYPE) ||
            (!list_lb && w->type != JK_LB_WORKER_TYPE)) {
            if (mime == JK_STATUS_MIME_XML) {
                display_worker_xml(s, w, sw, show_members, 0, l);
            }
            else if (mime == JK_STATUS_MIME_TXT) {
                display_worker_txt(s, w, sw, show_members, 0, l);
            }
            else if (mime == JK_STATUS_MIME_PROP) {
                display_worker_prop(s, w, sw, show_members, 0, l);
            }
            else {
                display_worker(s, w, sw, show_members, refresh, 0, l);
            }
        }
    }

    if (list_lb) {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_close_elt(s, sw, 0, "balancers");
        }
        else if (mime == JK_STATUS_MIME_TXT) {
        }
        else if (mime == JK_STATUS_MIME_PROP) {
        }
        else {
            display_legend(s, sw, l);
        }
    }
    else {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_close_elt(s, sw, 0, "ajp_workers");
        }
        else if (mime == JK_STATUS_MIME_TXT) {
        }
        else if (mime == JK_STATUS_MIME_PROP) {
        }
        else {
            display_legend(s, sw, l);
        }
    }

    JK_TRACE_EXIT(l);
}

static int list_workers(jk_ws_service_t *s, status_worker_t *sw,
                        int mime, int show_members,
                        int refresh, jk_logger_t *l)
{
    int lb_cnt = 0;
    int ajp_cnt = 0;

    JK_TRACE_ENTER(l);
    count_workers_xml(s, sw, &lb_cnt, &ajp_cnt, l);

    if (lb_cnt) {
        list_workers_type_mime(s, sw, 1, mime, show_members, lb_cnt, refresh, l);
    }

    if (ajp_cnt) {
        list_workers_type_mime(s, sw, 0, mime, show_members, ajp_cnt, refresh, l);
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int show_worker(jk_ws_service_t *s, status_worker_t *sw,
                       const char *worker, const char *sub_worker,
                       int mime, int show_members,
                       int refresh, jk_logger_t *l)
{
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "showing worker '%s' sub worker '%s'",
               worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "NULL or EMPTY worker param");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    w = wc_get_worker_for_name(worker, l);
    if (!w) {
        jk_log(l, JK_LOG_WARNING,
               "could not find worker '%s'",
               worker);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (mime == JK_STATUS_MIME_XML) {
        display_worker_xml(s, w, sw, show_members, 1, l);
    }
    else if (mime == JK_STATUS_MIME_TXT) {
        display_worker_txt(s, w, sw, show_members, 1, l);
    }
    else if (mime == JK_STATUS_MIME_PROP) {
        display_worker_prop(s, w, sw, show_members, 1, l);
    }
    else {
        display_worker(s, w, sw, show_members, refresh, 1, l);
        display_legend(s, sw, l);
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int edit_worker(jk_ws_service_t *s, status_worker_t *sw,
                       const char *worker, const char *sub_worker,
                       int from, int refresh,
                       jk_logger_t *l)
{
    unsigned int i;
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "editing worker '%s' sub worker '%s'",
               worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "NULL or EMPTY worker param");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    w = wc_get_worker_for_name(worker, l);
    if (!w) {
        jk_log(l, JK_LOG_WARNING,
               "could not find worker '%s'",
               worker);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (!sub_worker || !sub_worker[0]) {
        char buf[128];

        if (status_get_arg_raw(JK_STATUS_ARG_OPTION,
                               s->query_string, buf, sizeof(buf), l) == JK_TRUE)
            form_all_members(s, w, buf, from, refresh, l);
        else
            form_worker(s, w, from, refresh, l);
    }
    else  {
        lb_worker_t *lb = NULL;
        worker_record_t *wr = NULL;
        if (w->type != JK_LB_WORKER_TYPE) {
            jk_log(l, JK_LOG_WARNING,
                   "worker type not implemented");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        lb = (lb_worker_t *)w->worker_private;
        if (!lb) {
            jk_log(l, JK_LOG_WARNING,
                   "lb structure is (NULL)");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        for (i = 0; i < (int)lb->num_of_workers; i++) {
            wr = &(lb->lb_workers[i]);
            if (strcmp(sub_worker, wr->s->name) == 0)
                break;
        }
        if (!wr || i == (int)lb->num_of_workers) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sub_worker);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        form_member(s, wr, worker, from, refresh, l);
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int update_worker(jk_ws_service_t *s, status_worker_t *sw,
                         const char *worker, const char *sub_worker,
                         jk_logger_t *l)
{
    unsigned int i;
    jk_worker_t *w = NULL;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "updating worker '%s' sub worker '%s'",
               worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "NULL or EMPTY worker param");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    w = wc_get_worker_for_name(worker, l);
    if (!w) {
        jk_log(l, JK_LOG_WARNING,
               "could not find worker '%s'",
               worker);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (!sub_worker || !sub_worker[0]) {
        char buf[128];

        if (status_get_arg_raw(JK_STATUS_ARG_OPTION,
                               s->query_string, buf, sizeof(buf), l) == JK_TRUE)
            commit_all_members(s, w, buf, l);
        else
            commit_worker(s, w, l);
    }
    else  {
        lb_worker_t *lb = NULL;
        worker_record_t *wr = NULL;
        int rc = 0;
        if (w->type != JK_LB_WORKER_TYPE) {
            jk_log(l, JK_LOG_WARNING,
                   "worker type not implemented");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        lb = (lb_worker_t *)w->worker_private;
        if (!lb) {
            jk_log(l, JK_LOG_WARNING,
                   "lb structure is (NULL)");
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        for (i = 0; i < (int)lb->num_of_workers; i++) {
            wr = &(lb->lb_workers[i]);
            if (strcmp(sub_worker, wr->s->name) == 0)
                break;
        }
        if (!wr || i == (int)lb->num_of_workers) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sub_worker);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        rc = commit_member(s, wr, worker, l);
        if (rc & 1)
            reset_lb_values(lb, l);
        if (rc & 2)
            /* Recalculate the load multiplicators wrt. lb_factor */
            update_mult(lb, l);
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int reset_worker(jk_ws_service_t *s, status_worker_t *sw,
                        const char *worker, const char *sub_worker,
                        jk_logger_t *l)
{
    unsigned int i;
    lb_worker_t *lb;
    jk_worker_t *w = NULL;
    worker_record_t *wr = NULL;

    JK_TRACE_ENTER(l);
    jk_log(l, JK_LOG_INFO,
           "resetting worker '%s' sub worker '%s'",
           worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "NULL or EMPTY worker param");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    w = wc_get_worker_for_name(worker, l);
    if (!w) {
        jk_log(l, JK_LOG_WARNING,
               "could not find worker '%s'",
               worker);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    /* XXX Until now, we only have something to reset for lb workers or their members */
    if (w->type != JK_LB_WORKER_TYPE) {
        jk_log(l, JK_LOG_WARNING,
               "worker type not implemented");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    lb = (lb_worker_t *)w->worker_private;
    if (!lb) {
        jk_log(l, JK_LOG_WARNING,
               "lb structure is (NULL)");
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (!sub_worker || !sub_worker[0]) {
        lb->s->max_busy = 0;
        for (i = 0; i < lb->num_of_workers; i++) {
            wr = &(lb->lb_workers[i]);
            wr->s->client_errors    = 0;
            wr->s->elected          = 0;
            wr->s->elected_snapshot = 0;
            wr->s->error_time       = 0;
            wr->s->errors           = 0;
            wr->s->lb_value         = 0;
            wr->s->max_busy         = 0;
            wr->s->recoveries       = 0;
            wr->s->recovery_errors  = 0;
            wr->s->readed           = 0;
            wr->s->transferred      = 0;
            wr->s->state            = JK_LB_STATE_NA;
        }
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else  {
        for (i = 0; i < (int)lb->num_of_workers; i++) {
            wr = &(lb->lb_workers[i]);
            if (strcmp(sub_worker, wr->s->name) == 0)
                break;
        }
        if (!wr || i == (int)lb->num_of_workers) {
            jk_log(l, JK_LOG_WARNING,
                   "could not find worker '%s'",
                   sub_worker);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        wr->s->client_errors    = 0;
        wr->s->elected          = 0;
        wr->s->elected_snapshot = 0;
        wr->s->error_time       = 0;
        wr->s->errors           = 0;
        wr->s->lb_value         = 0;
        wr->s->max_busy         = 0;
        wr->s->recoveries       = 0;
        wr->s->recovery_errors  = 0;
        wr->s->readed           = 0;
        wr->s->transferred      = 0;
        wr->s->state            = JK_LB_STATE_NA;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_recoverable_error)
{
    int cmd;
    int mime;
    int from;
    int refresh;
    char worker[JK_SHM_STR_SIZ+1];
    char sub_worker[JK_SHM_STR_SIZ+1];
    char option[JK_SHM_STR_SIZ+1];
    int show_members = 1;
    char *err = NULL;
    status_endpoint_t *p;
    status_worker_t *w;
    int denied = 0;

    JK_TRACE_ENTER(l);

    if (is_recoverable_error)
        *is_recoverable_error = JK_FALSE;
    if (!e || !e->endpoint_private || !s || !is_recoverable_error) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    p = e->endpoint_private;
    w = p->s_worker;

    if (w->num_of_users) {
        if (s->remote_user) {
            unsigned int i;
            denied = 1;
            for (i = 0; i < w->num_of_users; i++) {
                if (!strcmp(s->remote_user, w->user_names[i])) {
                    denied = 0;
                    break;
                }
            }
        }
        else {
            denied = 2;
        }
    }

    /* Step 1: Process GET params and update configuration */
    status_parse_uri(s, &cmd, &mime, &from, &refresh, worker, sub_worker, option, l);

    if (mime == JK_STATUS_MIME_HTML) {
        s->start_response(s, 200, "OK", headers_names, headers_vhtml, 3);
        s->write(s, JK_STATUS_HEAD, sizeof(JK_STATUS_HEAD) - 1);
    }
    else if (mime == JK_STATUS_MIME_XML) {
        s->start_response(s, 200, "OK", headers_names, headers_vxml, 3);
        s->write(s, JK_STATUS_XMLH, sizeof(JK_STATUS_XMLH) - 1);
        if (w->doctype) {
            jk_putv(s, w->doctype, "\n", NULL);
        }
        jk_print_xml_start_elt(s, w, 0, 0, "status");
        if (w->xmlns && strlen(w->xmlns))
            jk_putv(s, "  ", w->xmlns, NULL);
        jk_print_xml_stop_elt(s, 0, 0);
    }
    else {
        s->start_response(s, 200, "OK", headers_names, headers_vtxt, 3);
    }

    if (denied == 0) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker service allowed for user '%s' [%s] from %s [%s]",
                   s->remote_user ? s->remote_user : "(null)",
                   s->auth_type ? s->auth_type : "(null)",
                   s->remote_addr ? s->remote_addr : "(null)",
                   s->remote_host ? s->remote_host : "(null)");
    }
    else if (denied == 1) {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker service denied for user '%s' [%s] from %s [%s]",
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }
    else if (denied == 2) {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker service denied (no user) [%s] from %s [%s]",
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }
    else {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker service denied (unknown reason) for user '%s' [%s] from %s [%s]",
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }

    if (!err) {
        if (w->read_only &&
            (cmd == JK_STATUS_CMD_EDIT ||
            cmd == JK_STATUS_CMD_UPDATE ||
            cmd == JK_STATUS_CMD_RESET)) {
            err = "This command is not allowed in read only mode.";
        }
    }

    if (!err) {
        if (!strcmp(option, JK_STATUS_ATT_NO_MEMBERS)) {
            show_members = 0;
        }
        if (cmd == JK_STATUS_CMD_UNKNOWN) {
            err = "Invalid command.";
        }
        else if (mime == JK_STATUS_MIME_UNKNOWN) {
            err = "Invalid mime type.";
        }
        else if (cmd != JK_STATUS_CMD_LIST &&
                 cmd != JK_STATUS_CMD_VERSION &&
                 (!worker || !(worker[0]))) {
            err = "This command needs a valid worker in the request.";
        }
        else if (cmd != JK_STATUS_CMD_LIST &&
                 cmd != JK_STATUS_CMD_VERSION &&
                 (check_worker(s, worker, sub_worker, l) != JK_TRUE)) {
                err = "This worker or sub worker does not exist.";
        }
    }

    if (!err) {
        if (cmd == JK_STATUS_CMD_UPDATE) {
            /* lock shared memory */
            jk_shm_lock();
            if (update_worker(s, w, worker, sub_worker, l) == JK_FALSE) {
                err = "Update failed";
            }
            /* unlock the shared memory */
            jk_shm_unlock();
            if (mime == JK_STATUS_MIME_HTML) {
                jk_puts(s, "\n<meta http-equiv=\"Refresh\" content=\""
                        JK_STATUS_REFRESH ";url=");
                status_write_uri(s, NULL, from, 0, 0, refresh,
                                worker, NULL, option);
                jk_puts(s, "\">");
                if (!err) {
                    jk_puts(s, "<p><b>Result: OK - You will be redirected in "
                            JK_STATUS_REFRESH " seconds.</b><p/>");
                }
            }
        }
        else if (cmd == JK_STATUS_CMD_RESET) {
            /* lock shared memory */
            jk_shm_lock();
            if (reset_worker(s, w, worker, sub_worker, l) == JK_FALSE) {
                err = "Reset failed";
            }
            /* unlock the shared memory */
            jk_shm_unlock();
            if (mime == JK_STATUS_MIME_HTML) {
                jk_puts(s, "\n<meta http-equiv=\"Refresh\" content=\""
                        JK_STATUS_REFRESH ";url=");
                status_write_uri(s, NULL, from, 0, 0, refresh,
                                 worker, NULL, option);
                jk_puts(s, "\">");
                if (!err) {
                    jk_puts(s, "<p><b>Result: OK - You will be redirected in "
                            JK_STATUS_REFRESH " seconds.</b><p/>");
                }
            }
        }
        else {
            if (mime == JK_STATUS_MIME_XML) {
                jk_print_xml_start_elt(s, w, 0, 0, "server");
                jk_print_xml_att_string(s, 2, "name", s->server_name);
                jk_print_xml_att_int(s, 2, "port", s->server_port);
                jk_print_xml_stop_elt(s, 0, 1);
                if ((cmd == JK_STATUS_CMD_LIST) ||
                    (cmd == JK_STATUS_CMD_VERSION)) {
                    jk_print_xml_start_elt(s, w, 0, 0, "software");
                    jk_print_xml_att_string(s, 2, "web_server", s->server_software);
                    jk_print_xml_att_string(s, 2, "jk_version", JK_EXPOSED_VERSION);
                    jk_print_xml_stop_elt(s, 0, 1);
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, w, JK_STATUS_MIME_XML, show_members, -1, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if(show_worker(s, w, worker, sub_worker, JK_STATUS_MIME_XML, show_members, -1, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_TXT) {
                jk_puts(s, "Server:");
                jk_printf(s, " name=%s", s->server_name);
                jk_printf(s, " port=%d", s->server_port);
                jk_puts(s, "\n");
                if ((cmd == JK_STATUS_CMD_LIST) ||
                    (cmd == JK_STATUS_CMD_VERSION)) {
                    jk_puts(s, "Software:");
                    jk_printf(s, " web_server=\"%s\"", s->server_software);
                    jk_printf(s, " jk_version=%s", JK_EXPOSED_VERSION);
                    jk_puts(s, "\n");
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, w, JK_STATUS_MIME_TXT, show_members, -1, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if(show_worker(s, w, worker, sub_worker, JK_STATUS_MIME_TXT, show_members, -1, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_PROP) {
                jk_print_prop_att_string(s, w, NULL, "server_name", s->server_name);
                jk_print_prop_att_int(s, w, NULL, "server_port", s->server_port);
                if ((cmd == JK_STATUS_CMD_LIST) ||
                    (cmd == JK_STATUS_CMD_VERSION)) {
                    jk_print_prop_att_string(s, w, NULL, "web_server", s->server_software);
                    jk_print_prop_att_string(s, w, NULL, "jk_version", JK_EXPOSED_VERSION);
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, w, JK_STATUS_MIME_PROP, show_members, -1, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if(show_worker(s, w, worker, sub_worker, JK_STATUS_MIME_PROP, show_members, -1, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_HTML) {
                if ((cmd == JK_STATUS_CMD_LIST ||
                    cmd == JK_STATUS_CMD_SHOW) &&
                    refresh > 0) {
                    jk_printf(s, "\n<meta http-equiv=\"Refresh\" content=\"%d;url=%s?%s\">",
                          refresh, s->req_uri, s->query_string);
                }
                if (w->css) {
                    jk_putv(s, "\n<link rel=\"stylesheet\" type=\"text/css\" href=\"",
                            w->css, "\" />\n", NULL);
                }
                s->write(s, JK_STATUS_HEND, sizeof(JK_STATUS_HEND) - 1);
                jk_puts(s, "<h1>JK Status Manager for ");
                jk_puts(s, s->server_name);
                jk_printf(s, ":%d", s->server_port);
                jk_puts(s, "</h1>\n\n");
                if ((cmd == JK_STATUS_CMD_LIST) ||
                    (cmd == JK_STATUS_CMD_SHOW) ||
                    (cmd == JK_STATUS_CMD_VERSION)) {
                    jk_putv(s, "<table><tr><td>Server Version:</td><td>",
                            s->server_software, "</td></tr>\n", NULL);
                    jk_putv(s, "<tr><td>JK Version:</td><td>",
                            JK_EXPOSED_VERSION, "</td></tr></table>\n", NULL);
                    jk_puts(s, "<hr/>\n");
                }
                if (cmd == JK_STATUS_CMD_LIST ||
                    cmd == JK_STATUS_CMD_SHOW) {
                    if (refresh > 0) {
                        const char *str = s->query_string;
                        char *buf = jk_pool_alloc(s->pool, sizeof(char *) * (strlen(str)+1));
                        int result = 0;
                        int scan = 0;
                        int len = strlen(JK_STATUS_ARG_REFRESH);

                        while (str[scan] != '\0') {
                            if (strncmp(&str[scan], JK_STATUS_ARG_REFRESH, len) == 0 &&
                                str[scan+len] == '=') {
                                scan += len + 1;
                                while (str[scan] != '\0' && str[scan] != '&')
                                    scan++;
                                if (str[scan] == '&')
                                    scan++;
                            }
                            else {
                                if (result > 0 && str[scan] != '\0' && str[scan] != '&') {
                                    buf[result] = '&';
                                    result++;
                                }
                                while (str[scan] != '\0' && str[scan] != '&') {
                                    buf[result] = str[scan];
                                    result++;
                                    scan++;
                                }
                                if (str[scan] == '&')
                                    scan++;
                            }
                        }
                        buf[result] = '\0';

                        jk_putv(s, "[<a href=\"", s->req_uri, NULL);
                        if (buf && buf[0])
                            jk_putv(s, "?", buf, NULL);
                        jk_puts(s, "\">Stop auto refresh</a>]&nbsp;&nbsp;");
                    }
                    else {
                        status_start_form(s, "GET", cmd,
                                          mime, from, 0, worker, sub_worker, option);
                        jk_puts(s, "<input type=\"submit\" value=\"Start auto refresh\"/>\n");
                        jk_putv(s, "(every "
                                "<input name=\"", JK_STATUS_ARG_REFRESH,
                                "\" type=\"text\" size=\"3\" value=\"10\"/> "
                                "seconds)", NULL);
                        jk_puts(s, "</form>\n");
                    }
                }
                if (cmd == JK_STATUS_CMD_SHOW ||
                    cmd == JK_STATUS_CMD_EDIT ||
                    cmd == JK_STATUS_CMD_VERSION) {
                    jk_puts(s, "[");
                    if (cmd == JK_STATUS_CMD_SHOW ||
                        cmd == JK_STATUS_CMD_VERSION ||
                        from == JK_STATUS_CMD_LIST) {
                        status_write_uri(s, "Back to worker list", JK_STATUS_CMD_LIST,
                                         JK_STATUS_MIME_HTML, 0, refresh, NULL, NULL, option);
                    }
                    else {
                        status_write_uri(s, "Back to worker view", JK_STATUS_CMD_SHOW,
                                         JK_STATUS_MIME_HTML, 0, refresh, worker, sub_worker, option);
                    }
                    jk_puts(s, "]&nbsp;&nbsp;");
                }
                if (cmd == JK_STATUS_CMD_LIST ||
                    cmd == JK_STATUS_CMD_SHOW ||
                    cmd == JK_STATUS_CMD_VERSION) {
                    jk_puts(s, "[Change&nbsp;Format: ");
                    status_write_uri(s, "XML", cmd,
                                     JK_STATUS_MIME_XML, 0, 0, worker, sub_worker, option);
                    jk_puts(s, "&nbsp;|&nbsp;");
                    status_write_uri(s, "Property", cmd,
                                     JK_STATUS_MIME_PROP, 0, 0, worker, sub_worker, option);
                    jk_puts(s, "&nbsp;|&nbsp;");
                    status_write_uri(s, "TXT", cmd,
                                     JK_STATUS_MIME_TXT, 0, 0, worker, sub_worker, option);
                    jk_puts(s, "]&nbsp;&nbsp;");
                }
                if (cmd == JK_STATUS_CMD_LIST || cmd == JK_STATUS_CMD_SHOW) {
                    jk_puts(s, "[");
                    if (cmd == JK_STATUS_CMD_LIST)
                        jk_puts(s, "<b>S</b>=Show only this worker");
                    if (!w->read_only && cmd == JK_STATUS_CMD_LIST)
                        jk_puts(s, ", ");
                    if (!w->read_only)
                        jk_puts(s, "<b>E</b>=Edit worker, <b>R</b>=Reset worker state");
                    jk_puts(s, "]\n");
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, w, JK_STATUS_MIME_HTML, show_members, refresh, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if(show_worker(s, w, worker, sub_worker, JK_STATUS_MIME_HTML, show_members, refresh, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_EDIT) {
                    /* Step 2: Display edit form */
                    if(edit_worker(s, w, worker, sub_worker, from, refresh, l) != JK_TRUE) {
                        err = "Error in generating this worker's configuration form.";
                    }
                }
            }
        }
    }
    if (err) {
        jk_log(l, JK_LOG_WARNING, "%s", err);
        if (mime == JK_STATUS_MIME_HTML) {
            jk_putv(s, "<p><b>Result: ERROR - ", err, "</b><br/>", NULL);
            jk_putv(s, "<a href=\"", s->req_uri, "\">JK Status Manager Start Page</a></p>", NULL);
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 2, 0, "result");
            jk_print_xml_att_string(s, 4, "type", "ERROR");
            jk_print_xml_att_string(s, 4, "msg", err);
            jk_print_xml_stop_elt(s, 2, 1);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Result:");
            jk_printf(s, " type=%s", "ERROR");
            jk_printf(s, " msg=\"%s\"", err);
            jk_puts(s, "\n");
        }
        else {
            jk_print_prop_att_string(s, w, "result", "type", "ERROR");
            jk_print_prop_att_string(s, w, "result", "msg", err);
        }
    }
    else {
        if (mime == JK_STATUS_MIME_HTML) {
            jk_putv(s, "<p><a href=\"", s->req_uri, "\">JK Status Manager Start Page</a></p>", NULL);
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 2, 0, "result");
            jk_print_xml_att_string(s, 4, "type", "OK");
            jk_print_xml_att_string(s, 4, "msg", "Action finished");
            jk_print_xml_stop_elt(s, 2, 1);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Result:");
            jk_printf(s, " type=%s", "OK");
            jk_printf(s, " msg=\"%s\"", "Action finished");
            jk_puts(s, "\n");
        }
        else {
            jk_print_prop_att_string(s, w, "result", "type", "OK");
            jk_print_prop_att_string(s, w, "result", "msg", "Action finished");
        }
    }
    if (mime == JK_STATUS_MIME_HTML) {
        if (w->css) {
            jk_putv(s, "<hr/><div class=\"footer\">", JK_STATUS_COPYRIGHT,
                    "</div>\n", NULL);
        }
        else {
            jk_putv(s, "<hr/><p align=\"center\"><small>", JK_STATUS_COPYRIGHT,
                    "</small></p>\n", NULL);
        }
        s->write(s, JK_STATUS_BEND, sizeof(JK_STATUS_BEND) - 1);
    }
    else if (mime == JK_STATUS_MIME_XML) {
        jk_print_xml_close_elt(s, w, 0, "status");
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
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
                          jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    if (pThis && pThis->worker_private) {
        status_worker_t *p = pThis->worker_private;
        char **good_rating;
        unsigned int num_of_good;
        char **bad_rating;
        unsigned int num_of_bad;
        unsigned int i;
        p->we = we;
        p->css = jk_get_worker_style_sheet(props, p->name, NULL);
        p->prefix = jk_get_worker_prop_prefix(props, p->name, JK_STATUS_PREFIX_DEF);
        p->ns = jk_get_worker_name_space(props, p->name, JK_STATUS_NS_DEF);
        p->xmlns = jk_get_worker_xmlns(props, p->name, JK_STATUS_XMLNS_DEF);
        p->doctype = jk_get_worker_xml_doctype(props, p->name, NULL);
        p->read_only = jk_get_is_read_only(props, p->name);
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' has css '%s' and read_only '%s'",
                   p->name,
                   p->css ? p->css : "(null)",
                   p->read_only ? "true" : "false");
        if (jk_get_worker_user_list(props, p->name,
                                    &(p->user_names),
                                    &(p->num_of_users)) && p->num_of_users) {
            for (i = 0; i < p->num_of_users; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "restricting access for status worker '%s' to user '%s'",
                            p->name, p->user_names[i]);
            }
        }
        if (jk_get_worker_good_rating(props, p->name,
                                      &good_rating,
                                      &num_of_good) && num_of_good) {
            p->good_mask = 0;
            for (i = 0; i < num_of_good; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "rating as good for '%s': '%s'",
                            p->name, good_rating[i]);
                p->good_mask |= status_get_rating(good_rating[i], l);
            }
        }
        else {
            p->good_mask = JK_STATUS_MASK_GOOD_DEF;
        }
        if (jk_get_worker_bad_rating(props, p->name,
                                      &bad_rating,
                                      &num_of_bad) && num_of_bad) {
            p->bad_mask = 0;
            for (i = 0; i < num_of_bad; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "rating as bad for '%s': '%s'",
                            p->name, bad_rating[i]);
                p->bad_mask |= status_get_rating(bad_rating[i], l);
            }
        }
        else {
            p->bad_mask = JK_STATUS_MASK_BAD_DEF;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' has good rating for '%08" JK_UINT32_T_HEX_FMT
                   "' and bad rating for '%08" JK_UINT32_T_HEX_FMT "'",
                   p->name,
                   p->good_mask,
                   p->bad_mask);
        if (p->good_mask & p->bad_mask)
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' has non empty intersection '%08" JK_UINT32_T_HEX_FMT
                   "' between good rating for '%08" JK_UINT32_T_HEX_FMT
                   "' and bad rating for '%08" JK_UINT32_T_HEX_FMT "'",
                   p->name,
                   p->good_mask & p->bad_mask,
                   p->good_mask,
                   p->bad_mask);
    }
    JK_TRACE_EXIT(l);
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
