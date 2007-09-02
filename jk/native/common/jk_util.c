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
 * Description: Utility functions (mainly configuration)                   *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision$                                          *
 ***************************************************************************/


#include "jk_util.h"
#include "jk_ajp12_worker.h"
#include "jk_ajp13_worker.h"
#include "jk_ajp14_worker.h"
#include "jk_lb_worker.h"
#include "jk_mt.h"

#define SYSPROPS_OF_WORKER          ("sysprops")
#define STDERR_OF_WORKER            ("stderr")
#define STDOUT_OF_WORKER            ("stdout")
#define SECRET_OF_WORKER            ("secret")
#define MX_OF_WORKER                ("mx")
#define MS_OF_WORKER                ("ms")
#define CP_OF_WORKER                ("class_path")
#define BRIDGE_OF_WORKER            ("bridge")
#define JVM_OF_WORKER               ("jvm_lib")
#define LIBPATH_OF_WORKER           ("ld_path")
#define CMD_LINE_OF_WORKER          ("cmd_line")
#define NATIVE_LIB_OF_WORKER        ("native_lib")
#define REFERENCE_OF_WORKER         ("reference")
#define HOST_OF_WORKER              ("host")
#define PORT_OF_WORKER              ("port")
#define TYPE_OF_WORKER              ("type")
#define CACHE_OF_WORKER_DEPRECATED  ("cachesize")
#define CACHE_OF_WORKER             ("connection_pool_size")
#define CACHE_OF_WORKER_MIN         ("connection_pool_minsize")
#define CACHE_TIMEOUT_DEPRECATED    ("cache_timeout")
#define CACHE_TIMEOUT_OF_WORKER     ("connection_pool_timeout")
#define RECOVERY_OPTS_OF_WORKER     ("recovery_options")
#define CONNECT_TIMEOUT_OF_WORKER   ("connect_timeout")
#define PREPOST_TIMEOUT_OF_WORKER   ("prepost_timeout")
#define REPLY_TIMEOUT_OF_WORKER     ("reply_timeout")
#define SOCKET_TIMEOUT_OF_WORKER    ("socket_timeout")
#define SOCKET_BUFFER_OF_WORKER     ("socket_buffer")
#define SOCKET_KEEPALIVE_OF_WORKER  ("socket_keepalive")
#define RECYCLE_TIMEOUT_DEPRECATED  ("recycle_timeout")
#define LOAD_FACTOR_OF_WORKER       ("lbfactor")
#define DISTANCE_OF_WORKER          ("distance")
#define BALANCED_WORKERS_DEPRECATED ("balanced_workers")
#define BALANCE_WORKERS             ("balance_workers")
#define STICKY_SESSION              ("sticky_session")
#define STICKY_SESSION_FORCE        ("sticky_session_force")
#define LOCAL_WORKER_DEPRECATED     ("local_worker")
#define LOCAL_WORKER_ONLY_DEPRECATED ("local_worker_only")
#define JVM_ROUTE_OF_WORKER_DEPRECATED ("jvm_route")
#define ROUTE_OF_WORKER             ("route")
#define DOMAIN_OF_WORKER            ("domain")
#define REDIRECT_OF_WORKER          ("redirect")
#define MOUNT_OF_WORKER             ("mount")
#define METHOD_OF_WORKER            ("method")
#define LOCK_OF_WORKER              ("lock")
#define IS_WORKER_DISABLED_DEPRECATED ("disabled")
#define IS_WORKER_STOPPED_DEPRECATED  ("stopped")
#define ACTIVATION_OF_WORKER        ("activation")
#define WORKER_RECOVER_TIME         ("recover_time")
#define MAX_REPLY_TIMEOUTS_OF_WORKER ("max_reply_timeouts")
#define WORKER_MAX_PACKET_SIZE      ("max_packet_size")
#define STYLE_SHEET_OF_WORKER       ("css")
#define NAMESPACE_OF_WORKER         ("ns")
#define XML_NAMESPACE_OF_WORKER     ("xmlns")
#define XML_DOCTYPE_OF_WORKER       ("doctype")
#define PROP_PREFIX_OF_WORKER       ("prefix")

#define READ_ONLY_OF_WORKER         ("read_only")
#define USER_OF_WORKER              ("user")
#define USER_CASE_OF_WORKER         ("user_case_insensitive")
#define GOOD_RATING_OF_WORKER       ("good")
#define BAD_RATING_OF_WORKER        ("bad")

#define DEFAULT_WORKER_TYPE         JK_AJP13_WORKER_NAME
#define SECRET_KEY_OF_WORKER        ("secretkey")
#define RETRIES_OF_WORKER           ("retries")
#define STATUS_FAIL_OF_WORKER       ("fail_on_status")

#define DEFAULT_WORKER              JK_AJP13_WORKER_NAME
#define WORKER_LIST_PROPERTY_NAME     ("worker.list")
#define LIST_PROPERTY_NAME            ("list")
#define WORKER_MAINTAIN_PROPERTY_NAME ("worker.maintain")
#define MAINTAIN_PROPERTY_NAME        ("maintain")
#define DEFAULT_MAINTAIN_TIME       (60)
#define DEFAULT_LB_FACTOR           (1)
#define DEFAULT_DISTANCE            (0)

#define TOMCAT32_BRIDGE_NAME        ("tomcat32")
#define TOMCAT33_BRIDGE_NAME        ("tomcat33")
#define TOMCAT40_BRIDGE_NAME        ("tomcat40")
#define TOMCAT41_BRIDGE_NAME        ("tomcat41")
#define TOMCAT50_BRIDGE_NAME        ("tomcat5")

#define HUGE_BUFFER_SIZE (8*1024)

#define MAKE_WORKER_PARAM(P)     \
        strcpy(buf, "worker.");  \
        strcat(buf, wname);      \
        strcat(buf, ".");        \
        strcat(buf, P)

/*
 * define the log format, we're using by default the one from error.log
 *
 * [Mon Mar 26 19:44:48 2001] [jk_uri_worker_map.c (155)]: Into jk_uri_worker_map_t::uri_worker_map_alloc
 * log format used by apache in error.log
 */
#define JK_TIME_CONV_MILLI    "%Q"
#define JK_TIME_CONV_MICRO    "%q"
#define JK_TIME_PATTERN_MILLI "000"
#define JK_TIME_PATTERN_MICRO "000000"
#define JK_TIME_FORMAT_NONE   "[%a %b %d %H:%M:%S %Y] "
#define JK_TIME_FORMAT_MILLI  "[%a %b %d %H:%M:%S." JK_TIME_CONV_MILLI " %Y] "
#define JK_TIME_FORMAT_MICRO  "[%a %b %d %H:%M:%S." JK_TIME_CONV_MICRO " %Y] "
#define JK_TIME_SUBSEC_NONE   (0)
#define JK_TIME_SUBSEC_MILLI  (1)
#define JK_TIME_SUBSEC_MICRO  (2)
#define JK_TIME_MAX_SIZE      (64)

/* Visual C++ Toolkit 2003 support */
#if defined (_MSC_VER) && (_MSC_VER == 1310)
    extern long _ftol(double); /* defined by VC6 C libs */
    extern long _ftol2(double dblSource) { return _ftol(dblSource); }
#endif

static const char *list_properties[] = {
    BALANCE_WORKERS,
    MOUNT_OF_WORKER,
    USER_OF_WORKER,
    GOOD_RATING_OF_WORKER,
    BAD_RATING_OF_WORKER,
    STATUS_FAIL_OF_WORKER,
    "list",
    NULL
};

static const char *unique_properties[] = {
    SECRET_OF_WORKER,
    REFERENCE_OF_WORKER,
    HOST_OF_WORKER,
    PORT_OF_WORKER,
    TYPE_OF_WORKER,
    CACHE_OF_WORKER_DEPRECATED,
    CACHE_OF_WORKER,
    CACHE_OF_WORKER_MIN,
    CACHE_TIMEOUT_DEPRECATED,
    CACHE_TIMEOUT_OF_WORKER,
    RECOVERY_OPTS_OF_WORKER,
    CONNECT_TIMEOUT_OF_WORKER,
    PREPOST_TIMEOUT_OF_WORKER,
    REPLY_TIMEOUT_OF_WORKER,
    SOCKET_TIMEOUT_OF_WORKER,
    SOCKET_BUFFER_OF_WORKER,
    SOCKET_KEEPALIVE_OF_WORKER,
    RECYCLE_TIMEOUT_DEPRECATED,
    LOAD_FACTOR_OF_WORKER,
    STICKY_SESSION,
    STICKY_SESSION_FORCE,
    LOCAL_WORKER_DEPRECATED,
    LOCAL_WORKER_ONLY_DEPRECATED,
    JVM_ROUTE_OF_WORKER_DEPRECATED,
    ROUTE_OF_WORKER,
    DOMAIN_OF_WORKER,
    REDIRECT_OF_WORKER,
    METHOD_OF_WORKER,
    LOCK_OF_WORKER,
    IS_WORKER_DISABLED_DEPRECATED,
    IS_WORKER_STOPPED_DEPRECATED,
    ACTIVATION_OF_WORKER,
    WORKER_RECOVER_TIME,
    MAX_REPLY_TIMEOUTS_OF_WORKER,
    WORKER_MAX_PACKET_SIZE,
    STYLE_SHEET_OF_WORKER,
    READ_ONLY_OF_WORKER,
    RETRIES_OF_WORKER,
    WORKER_MAINTAIN_PROPERTY_NAME,
    NAMESPACE_OF_WORKER,
    XML_NAMESPACE_OF_WORKER,
    XML_DOCTYPE_OF_WORKER,
    PROP_PREFIX_OF_WORKER,
    USER_CASE_OF_WORKER,
    NULL
};

static const char *deprecated_properties[] = {
    SYSPROPS_OF_WORKER,
    STDERR_OF_WORKER,
    STDOUT_OF_WORKER,
    MX_OF_WORKER,
    MS_OF_WORKER,
    CP_OF_WORKER,
    BRIDGE_OF_WORKER,
    JVM_OF_WORKER,
    LIBPATH_OF_WORKER,
    CMD_LINE_OF_WORKER,
    NATIVE_LIB_OF_WORKER,
    CACHE_OF_WORKER_DEPRECATED,
    CACHE_TIMEOUT_DEPRECATED,
    RECYCLE_TIMEOUT_DEPRECATED,
    BALANCED_WORKERS_DEPRECATED,
    JVM_ROUTE_OF_WORKER_DEPRECATED,
    LOCAL_WORKER_DEPRECATED,
    LOCAL_WORKER_ONLY_DEPRECATED,
    IS_WORKER_DISABLED_DEPRECATED,
    IS_WORKER_STOPPED_DEPRECATED,
    NULL
};

static const char *supported_properties[] = {
    SYSPROPS_OF_WORKER,
    STDERR_OF_WORKER,
    STDOUT_OF_WORKER,
    SECRET_OF_WORKER,
    MX_OF_WORKER,
    MS_OF_WORKER,
    CP_OF_WORKER,
    BRIDGE_OF_WORKER,
    JVM_OF_WORKER,
    LIBPATH_OF_WORKER,
    CMD_LINE_OF_WORKER,
    NATIVE_LIB_OF_WORKER,
    REFERENCE_OF_WORKER,
    HOST_OF_WORKER,
    PORT_OF_WORKER,
    TYPE_OF_WORKER,
    CACHE_OF_WORKER_DEPRECATED,
    CACHE_OF_WORKER,
    CACHE_OF_WORKER_MIN,
    CACHE_TIMEOUT_DEPRECATED,
    CACHE_TIMEOUT_OF_WORKER,
    RECOVERY_OPTS_OF_WORKER,
    CONNECT_TIMEOUT_OF_WORKER,
    PREPOST_TIMEOUT_OF_WORKER,
    REPLY_TIMEOUT_OF_WORKER,
    SOCKET_TIMEOUT_OF_WORKER,
    SOCKET_BUFFER_OF_WORKER,
    SOCKET_KEEPALIVE_OF_WORKER,
    RECYCLE_TIMEOUT_DEPRECATED,
    LOAD_FACTOR_OF_WORKER,
    DISTANCE_OF_WORKER,
    BALANCED_WORKERS_DEPRECATED,
    BALANCE_WORKERS,
    STICKY_SESSION,
    STICKY_SESSION_FORCE,
    LOCAL_WORKER_DEPRECATED,
    LOCAL_WORKER_ONLY_DEPRECATED,
    JVM_ROUTE_OF_WORKER_DEPRECATED,
    ROUTE_OF_WORKER,
    DOMAIN_OF_WORKER,
    REDIRECT_OF_WORKER,
    MOUNT_OF_WORKER,
    METHOD_OF_WORKER,
    LOCK_OF_WORKER,
    IS_WORKER_DISABLED_DEPRECATED,
    IS_WORKER_STOPPED_DEPRECATED,
    ACTIVATION_OF_WORKER,
    WORKER_RECOVER_TIME,
    MAX_REPLY_TIMEOUTS_OF_WORKER,
    WORKER_MAX_PACKET_SIZE,
    STYLE_SHEET_OF_WORKER,
    NAMESPACE_OF_WORKER,
    XML_NAMESPACE_OF_WORKER,
    XML_DOCTYPE_OF_WORKER,
    PROP_PREFIX_OF_WORKER,
    READ_ONLY_OF_WORKER,
    USER_OF_WORKER,
    USER_CASE_OF_WORKER,
    GOOD_RATING_OF_WORKER,
    BAD_RATING_OF_WORKER,
    SECRET_KEY_OF_WORKER,
    RETRIES_OF_WORKER,
    STATUS_FAIL_OF_WORKER,
    LIST_PROPERTY_NAME,
    MAINTAIN_PROPERTY_NAME
};

static const char *jk_level_verbs[] = {
    "[" JK_LOG_TRACE_VERB "] ",
    "[" JK_LOG_DEBUG_VERB "] ",
    "[" JK_LOG_INFO_VERB "] ",
    "[" JK_LOG_WARN_VERB "] ",
    "[" JK_LOG_ERROR_VERB "] ",
    "[" JK_LOG_EMERG_VERB "] ",
    NULL
};

const char *jk_get_bool(int v)
{
    if (v == 0)
        return "False";
    else
        return "True";
}

int jk_get_bool_code(const char *v, int def)
{
    if (!v) {
        return def;
    }
    else if (!strcasecmp(v, "off") ||
             *v == 'F' || *v == 'f' ||
             *v == 'N' || *v == 'n' ||
             *v == '0') {
        return 0;
    }
    else if (!strcasecmp(v, "on") ||
             *v == 'T' || *v == 't' ||
             *v == 'Y' || *v == 'y' ||
             *v == '1') {
        return 1;
    }
    return def;
}

/* Sleep for 100ms */
void jk_sleep(int ms)
{
#ifdef OS2
    DosSleep(ms);
#elif defined(BEOS)
    snooze(ms * 1000);
#elif defined(NETWARE)
    delay(ms);
#elif defined(WIN32)
    Sleep(ms);
#else
    struct timeval tv;
    tv.tv_usec = ms * 1000;
    tv.tv_sec = 0;
    select(0, NULL, NULL, NULL, &tv);
#endif
}

void jk_set_time_fmt(jk_logger_t *l, const char *jk_log_fmt)
{
    if (l) {
        char *s;
        char log_fmt_safe[JK_TIME_MAX_SIZE];
        char *fmt;

        if (!jk_log_fmt) {
#ifndef NO_GETTIMEOFDAY
            jk_log_fmt = JK_TIME_FORMAT_MILLI;
#else
            jk_log_fmt = JK_TIME_FORMAT_NONE;
#endif
        }
        l->log_fmt_type = JK_TIME_SUBSEC_NONE;
        l->log_fmt_offset = 0;
        l->log_fmt_size = 0;
        l->log_fmt_subsec = jk_log_fmt;
        l->log_fmt = jk_log_fmt;

        fmt = (char *)malloc(JK_TIME_MAX_SIZE + strlen(JK_TIME_PATTERN_MICRO));
        if (fmt) {
            strncpy(log_fmt_safe, jk_log_fmt, JK_TIME_MAX_SIZE);
            if ((s = strstr(log_fmt_safe, JK_TIME_CONV_MILLI))) {
                size_t offset = s - log_fmt_safe;
                size_t len = strlen(JK_TIME_PATTERN_MILLI);

                l->log_fmt_type = JK_TIME_SUBSEC_MILLI;
                l->log_fmt_offset = offset;
                strncpy(fmt, log_fmt_safe, offset);
                strncpy(fmt + offset, JK_TIME_PATTERN_MILLI, len);
                strncpy(fmt + offset + len,
                        s + strlen(JK_TIME_CONV_MILLI),
                        JK_TIME_MAX_SIZE - offset - len);
                fmt[JK_TIME_MAX_SIZE-1] = '\0';
                l->log_fmt_subsec = fmt;
                l->log_fmt_size = strlen(fmt);
            }
            else if ((s = strstr(log_fmt_safe, JK_TIME_CONV_MICRO))) {
                size_t offset = s - log_fmt_safe;
                size_t len = strlen(JK_TIME_PATTERN_MICRO);

                l->log_fmt_type = JK_TIME_SUBSEC_MICRO;
                l->log_fmt_offset = offset;
                strncpy(fmt, log_fmt_safe, offset);
                strncpy(fmt + offset, JK_TIME_PATTERN_MICRO, len);
                strncpy(fmt + offset + len,
                        s + strlen(JK_TIME_CONV_MICRO),
                        JK_TIME_MAX_SIZE - offset - len);
                fmt[JK_TIME_MAX_SIZE-1] = '\0';
                l->log_fmt_subsec = fmt;
                l->log_fmt_size = strlen(fmt);
            }
        }
    }
}

static int set_time_str(char *str, int len, jk_logger_t *l)
{
    time_t t;
    struct tm *tms;
    int done;
    char log_fmt[JK_TIME_MAX_SIZE];

    if (!l || !l->log_fmt) {
        return 0;
    }

    log_fmt[0] = '\0';

#ifndef NO_GETTIMEOFDAY
    if ( l->log_fmt_type != JK_TIME_SUBSEC_NONE ) {
        struct timeval tv;
        int rc = 0;

#ifdef WIN32
        gettimeofday(&tv, NULL);
#else
        rc = gettimeofday(&tv, NULL);
#endif
        if (rc == 0) {
            char subsec[7];
            t = tv.tv_sec;
            strncpy(log_fmt, l->log_fmt_subsec, l->log_fmt_size + 1);
            if (l->log_fmt_type == JK_TIME_SUBSEC_MILLI) {
                sprintf(subsec, "%03d", (int)(tv.tv_usec/1000));
                strncpy(log_fmt + l->log_fmt_offset, subsec, 3);
            }
            else if (l->log_fmt_type == JK_TIME_SUBSEC_MICRO) {
                sprintf(subsec, "%06d", (int)(tv.tv_usec));
                strncpy(log_fmt + l->log_fmt_offset, subsec, 6);
            }
        }
        else {
            t = time(NULL);
        }
    }
    else {
        t = time(NULL);
    }
#else
    t = time(NULL);
#endif
    tms = localtime(&t);
    if (log_fmt[0])
        done = (int)strftime(str, len, log_fmt, tms);
    else
        done = (int)strftime(str, len, l->log_fmt, tms);
    return done;

}

static int JK_METHOD log_to_file(jk_logger_t *l, int level, int used, char *what)
{
    if (l &&
        (l->level <= level || level == JK_LOG_REQUEST_LEVEL) &&
        l->logger_private && what) {
        jk_file_logger_t *p = l->logger_private;
        if (p->logfile) {
            what[used++] = '\n';
            what[used] = '\0';
            fputs(what, p->logfile);
            /* [V] Flush the dam' thing! */
            fflush(p->logfile);
        }
        return JK_TRUE;
    }
    return JK_FALSE;
}

int jk_parse_log_level(const char *level)
{
    if (0 == strcasecmp(level, JK_LOG_TRACE_VERB)) {
        return JK_LOG_TRACE_LEVEL;
    }

    if (0 == strcasecmp(level, JK_LOG_DEBUG_VERB)) {
        return JK_LOG_DEBUG_LEVEL;
    }

    if (0 == strcasecmp(level, JK_LOG_INFO_VERB)) {
        return JK_LOG_INFO_LEVEL;
    }

    if (0 == strcasecmp(level, JK_LOG_WARN_VERB)) {
        return JK_LOG_WARNING_LEVEL;
    }

    if (0 == strcasecmp(level, JK_LOG_ERROR_VERB)) {
        return JK_LOG_ERROR_LEVEL;
    }

    if (0 == strcasecmp(level, JK_LOG_EMERG_VERB)) {
        return JK_LOG_EMERG_LEVEL;
    }

    return JK_LOG_DEF_LEVEL;
}

int jk_open_file_logger(jk_logger_t **l, const char *file, int level)
{
    if (l && file) {

        jk_logger_t *rc = (jk_logger_t *)malloc(sizeof(jk_logger_t));
        jk_file_logger_t *p = (jk_file_logger_t *) malloc(sizeof(jk_file_logger_t));
        if (rc && p) {
            rc->log = log_to_file;
            rc->level = level;
            jk_set_time_fmt(rc, NULL);
            rc->logger_private = p;
#if defined(AS400) && !defined(AS400_UTF8)
            p->logfile = fopen(file, "a+, o_ccsid=0");
#else
            p->logfile = fopen(file, "a+");
#endif
            if (p->logfile) {
                *l = rc;
                jk_log(rc, JK_LOG_DEBUG, "log time stamp format is '%s'", rc->log_fmt);
                return JK_TRUE;
            }
        }
        if (rc) {
            free(rc);
        }
        if (p) {
            free(p);
        }

        *l = NULL;
    }
    return JK_FALSE;
}

int jk_close_file_logger(jk_logger_t **l)
{
    if (l && *l) {
        jk_file_logger_t *p = (*l)->logger_private;
        if (p) {
            fflush(p->logfile);
            fclose(p->logfile);
            free(p);
        }
        free(*l);
        *l = NULL;

        return JK_TRUE;
    }
    return JK_FALSE;
}

int jk_log(jk_logger_t *l,
           const char *file, int line, const char *funcname, int level,
           const char *fmt, ...)
{
    int rc = 0;
    /*
     * Need to reserve space for terminating zero byte
     * and platform specific line endings added during the call
     * to the output routing.
     */
    static int usable_size = HUGE_BUFFER_SIZE - 3;
    if (!l || !file || !fmt) {
        return -1;
    }

    if ((l->level <= level) || (level == JK_LOG_REQUEST_LEVEL)) {
#ifdef NETWARE
        /* On NetWare, this can get called on a thread that has a limited stack so */
        /* we will allocate and free the temporary buffer in this function         */
        char *buf;
#else
        char buf[HUGE_BUFFER_SIZE];
#endif
        char *f = (char *)(file + strlen(file) - 1);
        va_list args;
        int used = 0;

        while (f != file && '\\' != *f && '/' != *f) {
            f--;
        }
        if (f != file) {
            f++;
        }

#ifdef NETWARE
        buf = (char *)malloc(HUGE_BUFFER_SIZE);
        if (NULL == buf)
            return -1;
#endif
        used = set_time_str(buf, usable_size, l);

        if (line) { /* line==0 only used for request log item */
            /* Log [pid:threadid] for all levels except REQUEST. */
            /* This information helps to correlate lines from different logs. */
            /* Performance is no issue, because with production log levels */
            /* we only call it often, if we have a lot of errors */
            rc = snprintf(buf + used, usable_size - used,
                          "[%" JK_PID_T_FMT ":%" JK_UINT32_T_FMT "] ", getpid(), jk_gettid());
            used += rc;
            if (rc < 0 ) {
                return 0;
            }

            rc = (int)strlen(jk_level_verbs[level]);
            if (usable_size - used >= rc) {
                strncpy(buf + used, jk_level_verbs[level], rc);
                used += rc;
            }
            else {
                return 0;           /* [V] not sure what to return... */
            }

            if (funcname) {
                rc = (int)strlen(funcname);
                if (usable_size - used >= rc + 2) {
                    strncpy(buf + used, funcname, rc);
                    used += rc;
                    strncpy(buf + used, "::", 2);
                    used += 2;
                }
                else {
                    return 0;           /* [V] not sure what to return... */
                }
            }

            rc = (int)strlen(f);
            if (usable_size - used >= rc) {
                strncpy(buf + used, f, rc);
                used += rc;
            }
            else {
                return 0;           /* [V] not sure what to return... */
            }

            rc = snprintf(buf + used, usable_size - used,
                          " (%d): ", line);
            used += rc;
            if (rc < 0 || usable_size - used < 0) {
                return 0;           /* [V] not sure what to return... */
            }
        }

        va_start(args, fmt);
        rc = vsnprintf(buf + used, usable_size - used, fmt, args);
        va_end(args);
        if ( rc <= usable_size - used ) {
            used += rc;
        } else {
            used = usable_size;
        }
        l->log(l, level, used, buf);

#ifdef NETWARE
        free(buf);
#endif
    }

    return rc;
}

const char *jk_get_worker_type(jk_map_t *m, const char *wname)
{
    char buf[1024];

    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(TYPE_OF_WORKER);
    return jk_map_get_string(m, buf, DEFAULT_WORKER_TYPE);
}

const char *jk_get_worker_route(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];
    const char *v;
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(ROUTE_OF_WORKER);
    v = jk_map_get_string(m, buf, NULL);
    if (v) {
        return v;
    }
    /* Try old jvm_route directive */
    MAKE_WORKER_PARAM(JVM_ROUTE_OF_WORKER_DEPRECATED);
    return jk_map_get_string(m, buf, def);
}

const char *jk_get_worker_domain(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(DOMAIN_OF_WORKER);
    return jk_map_get_string(m, buf, def);
}

const char *jk_get_worker_redirect(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
   MAKE_WORKER_PARAM(REDIRECT_OF_WORKER);
    return jk_map_get_string(m, buf, def);
}

const char *jk_get_worker_secret(jk_map_t *m, const char *wname)
{
    char buf[1024];

    if (!m || !wname) {
        return NULL;
    }

    MAKE_WORKER_PARAM(SECRET_OF_WORKER);

    return jk_map_get_string(m, buf, NULL);
}

/* [V] I suggest that the following general purpose functions be used.       */
/*     More should be added (double etc.), but now these were enough for me. */
/*     Functions that can be simulated with these should be "deprecated".    */

int jk_get_worker_str_prop(jk_map_t *m,
                           const char *wname, const char *pname, const char **prop)
{
    char buf[1024];

    if (m && prop && wname && pname) {
        MAKE_WORKER_PARAM(pname);
        *prop = jk_map_get_string(m, buf, NULL);
        if (*prop) {
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

int jk_get_worker_int_prop(jk_map_t *m,
                           const char *wname, const char *pname, int *prop)
{
    char buf[1024];

    if (m && prop && wname && pname) {
        int i;
        MAKE_WORKER_PARAM(pname);
        i = jk_map_get_int(m, buf, -1);
        if (-1 != i) {
            *prop = i;
            return JK_TRUE;
        }
    }
    return JK_FALSE;
}

const char *jk_get_worker_host(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];

    if (!m || !wname) {
        return NULL;
    }

    MAKE_WORKER_PARAM(HOST_OF_WORKER);

    return jk_map_get_string(m, buf, def);
}

int jk_get_worker_port(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(PORT_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

static int def_cache_size = -1;
int jk_get_worker_def_cache_size(int protocol)
{
    if (def_cache_size < 1) {
        if (protocol == AJP14_PROTO)
            def_cache_size = AJP14_DEF_CACHE_SZ;
        else
            def_cache_size = AJP13_DEF_CACHE_SZ;
    }
    return def_cache_size;
}

void jk_set_worker_def_cache_size(int sz)
{
    def_cache_size = sz;
}

int jk_get_worker_cache_size(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];
    int rv;

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(CACHE_OF_WORKER);
    if ((rv = jk_map_get_int(m, buf, -1)) >= 0)
        return rv;
    MAKE_WORKER_PARAM(CACHE_OF_WORKER_DEPRECATED);
    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_cache_size_min(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(CACHE_OF_WORKER_MIN);
    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_socket_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(SOCKET_TIMEOUT_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_recover_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(WORKER_RECOVER_TIME);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_max_reply_timeouts(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(MAX_REPLY_TIMEOUTS_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_socket_buffer(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];
    int i;
    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(SOCKET_BUFFER_OF_WORKER);

    i = jk_map_get_int(m, buf, 0);
    if (i > 0 && i < def)
        i = def;
    return i;
}

int jk_get_worker_socket_keepalive(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(SOCKET_KEEPALIVE_OF_WORKER);

    return jk_map_get_bool(m, buf, def);
}

int jk_get_worker_cache_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];
    int rv;

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(CACHE_TIMEOUT_OF_WORKER);
    if ((rv = jk_map_get_int(m, buf, -1)) >= 0)
        return rv;
    MAKE_WORKER_PARAM(CACHE_TIMEOUT_DEPRECATED);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_connect_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(CONNECT_TIMEOUT_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_prepost_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(PREPOST_TIMEOUT_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_reply_timeout(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(REPLY_TIMEOUT_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

int jk_get_worker_recycle_timeout(jk_map_t *m, const char *wname, int def)
{
    return def;
}

int jk_get_worker_retries(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];
    int rv;
    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(RETRIES_OF_WORKER);

    rv = jk_map_get_int(m, buf, def);
    if (rv < 1)
        rv = 1;

    return rv;
}

int jk_get_worker_recovery_opts(jk_map_t *m, const char *wname, int def)
{
    char buf[1024];

    if (!m || !wname) {
        return -1;
    }

    MAKE_WORKER_PARAM(RECOVERY_OPTS_OF_WORKER);

    return jk_map_get_int(m, buf, def);
}

const char *jk_get_worker_secret_key(jk_map_t *m, const char *wname)
{
    char buf[1024];

    if (!m || !wname) {
        return NULL;
    }

    MAKE_WORKER_PARAM(SECRET_KEY_OF_WORKER);
    return jk_map_get_string(m, buf, NULL);
}

int jk_get_worker_list(jk_map_t *m, char ***list, unsigned *num_of_workers)
{
    if (m && list && num_of_workers) {
        char **ar = jk_map_get_string_list(m,
                                        WORKER_LIST_PROPERTY_NAME,
                                        num_of_workers,
                                        DEFAULT_WORKER);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num_of_workers = 0;
    }

    return JK_FALSE;
}

int jk_get_is_worker_disabled(jk_map_t *m, const char *wname)
{
    int rc = JK_TRUE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(IS_WORKER_DISABLED_DEPRECATED);
        value = jk_map_get_bool(m, buf, 0);
        if (!value)
            rc = JK_FALSE;
    }
    return rc;
}

int jk_get_is_worker_stopped(jk_map_t *m, const char *wname)
{
    int rc = JK_TRUE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(IS_WORKER_STOPPED_DEPRECATED);
        value = jk_map_get_bool(m, buf, 0);
        if (!value)
            rc = JK_FALSE;
    }
    return rc;
}

int jk_get_worker_activation(jk_map_t *m, const char *wname)
{
    char buf[1024];
    const char *v;
    if (!m || !wname) {
        return JK_LB_ACTIVATION_ACTIVE;
    }

    MAKE_WORKER_PARAM(ACTIVATION_OF_WORKER);
    v = jk_map_get_string(m, buf, NULL);
    if (v) {
        return jk_lb_get_activation_code(v);
    }
    else if (jk_get_is_worker_stopped(m, wname))
        return JK_LB_ACTIVATION_STOPPED;
    else if (jk_get_is_worker_disabled(m, wname))
        return JK_LB_ACTIVATION_DISABLED;
    else
        return JK_LB_ACTIVATION_DEF;
}

int jk_get_lb_factor(jk_map_t *m, const char *wname)
{
    char buf[1024];

    if (!m || !wname) {
        return DEFAULT_LB_FACTOR;
    }

    MAKE_WORKER_PARAM(LOAD_FACTOR_OF_WORKER);

    return jk_map_get_int(m, buf, DEFAULT_LB_FACTOR);
}

int jk_get_distance(jk_map_t *m, const char *wname)
{
    char buf[1024];

    if (!m || !wname) {
        return DEFAULT_DISTANCE;
    }

    MAKE_WORKER_PARAM(DISTANCE_OF_WORKER);

    return jk_map_get_int(m, buf, DEFAULT_DISTANCE);
}

int jk_get_is_sticky_session(jk_map_t *m, const char *wname)
{
    int rc = JK_TRUE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(STICKY_SESSION);
        value = jk_map_get_bool(m, buf, 1);
        if (!value)
            rc = JK_FALSE;
    }
    return rc;
}

int jk_get_is_sticky_session_force(jk_map_t *m, const char *wname)
{
    int rc = JK_FALSE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(STICKY_SESSION_FORCE);
        value = jk_map_get_bool(m, buf, 0);
        if (value)
            rc = JK_TRUE;
    }
    return rc;
}

int jk_get_lb_method(jk_map_t *m, const char *wname)
{
    char buf[1024];
    const char *v;
    if (!m || !wname) {
        return JK_LB_METHOD_DEF;
    }

    MAKE_WORKER_PARAM(METHOD_OF_WORKER);
    v = jk_map_get_string(m, buf, NULL);
    return jk_lb_get_method_code(v);
}

int jk_get_lb_lock(jk_map_t *m, const char *wname)
{
    char buf[1024];
    const char *v;
    if (!m || !wname) {
        return JK_LB_LOCK_DEF;
    }

    MAKE_WORKER_PARAM(LOCK_OF_WORKER);
    v = jk_map_get_string(m, buf, NULL);
    return jk_lb_get_lock_code(v);
}

int jk_get_max_packet_size(jk_map_t *m, const char *wname)
{
    char buf[1024];
    int sz;

    if (!m || !wname) {
        return DEF_BUFFER_SZ;
    }

    MAKE_WORKER_PARAM(WORKER_MAX_PACKET_SIZE);
    sz = jk_map_get_int(m, buf, DEF_BUFFER_SZ);
    sz = JK_ALIGN(sz, 1024);
    if (sz < DEF_BUFFER_SZ)
        sz = DEF_BUFFER_SZ;
    else if (sz > 64*1024)
        sz = 64*1024;

    return sz;
}

int jk_get_worker_fail_on_status(jk_map_t *m, const char *wname,
                                 int *list, unsigned int list_size)
{
    char buf[1024];
    if (!m || !wname || !list) {
        return 0;
    }
    MAKE_WORKER_PARAM(STATUS_FAIL_OF_WORKER);
    if (list_size) {
        return jk_map_get_int_list(m, buf,
                                   list, list_size,
                                   NULL);
    }

    return 0;
}

int jk_get_worker_user_case_insensitive(jk_map_t *m, const char *wname)
{
    int rc = JK_FALSE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(USER_CASE_OF_WORKER);
        value = jk_map_get_bool(m, buf, 0);
        if (value)
            rc = JK_TRUE;
    }
    return rc;

}

const char *jk_get_worker_style_sheet(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];

    if (!m || !wname) {
        return NULL;
    }

    MAKE_WORKER_PARAM(STYLE_SHEET_OF_WORKER);

    return jk_map_get_string(m, buf, def);
}

const char *jk_get_worker_name_space(jk_map_t *m, const char *wname, const char *def)
{
    const char *rc;
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(NAMESPACE_OF_WORKER);
    rc = jk_map_get_string(m, buf, def);
    if (*rc == '-')
        return "";
    else
        return rc;
}

const char *jk_get_worker_xmlns(jk_map_t *m, const char *wname, const char *def)
{
    const char *rc;
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(XML_NAMESPACE_OF_WORKER);
    rc = jk_map_get_string(m, buf, def);
    if (*rc == '-')
        return "";
    else
        return rc;
}

const char *jk_get_worker_xml_doctype(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(XML_DOCTYPE_OF_WORKER);
    return jk_map_get_string(m, buf, def);
}

const char *jk_get_worker_prop_prefix(jk_map_t *m, const char *wname, const char *def)
{
    char buf[1024];
    if (!m || !wname) {
        return NULL;
    }
    MAKE_WORKER_PARAM(PROP_PREFIX_OF_WORKER);
    return jk_map_get_string(m, buf, def);
}

int jk_get_is_read_only(jk_map_t *m, const char *wname)
{
    int rc = JK_FALSE;
    char buf[1024];
    if (m && wname) {
        int value;
        MAKE_WORKER_PARAM(READ_ONLY_OF_WORKER);
        value = jk_map_get_bool(m, buf, 0);
        if (value)
            rc = JK_TRUE;
    }
    return rc;
}

int jk_get_worker_user_list(jk_map_t *m,
                            const char *wname,
                            char ***list, unsigned int *num)
{
    char buf[1024];

    if (m && list && num && wname) {
        char **ar = NULL;

        MAKE_WORKER_PARAM(USER_OF_WORKER);
        ar = jk_map_get_string_list(m, buf, num, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num = 0;
    }

    return JK_FALSE;
}

int jk_get_worker_good_rating(jk_map_t *m,
                              const char *wname,
                              char ***list, unsigned int *num)
{
    char buf[1024];

    if (m && list && num && wname) {
        char **ar = NULL;

        MAKE_WORKER_PARAM(GOOD_RATING_OF_WORKER);
        ar = jk_map_get_string_list(m, buf, num, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num = 0;
    }

    return JK_FALSE;
}

int jk_get_worker_bad_rating(jk_map_t *m,
                             const char *wname,
                             char ***list, unsigned int *num)
{
    char buf[1024];

    if (m && list && num && wname) {
        char **ar = NULL;

        MAKE_WORKER_PARAM(BAD_RATING_OF_WORKER);
        ar = jk_map_get_string_list(m, buf, num, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num = 0;
    }

    return JK_FALSE;
}

int jk_get_lb_worker_list(jk_map_t *m,
                          const char *wname,
                          char ***list, unsigned int *num_of_workers)
{
    char buf[1024];

    if (m && list && num_of_workers && wname) {
        char **ar = NULL;

        MAKE_WORKER_PARAM(BALANCE_WORKERS);
        ar = jk_map_get_string_list(m, buf, num_of_workers, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        /* Try old balanced_workers directive */
        MAKE_WORKER_PARAM(BALANCED_WORKERS_DEPRECATED);
        ar = jk_map_get_string_list(m, buf, num_of_workers, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num_of_workers = 0;
    }

    return JK_FALSE;
}

int jk_get_worker_mount_list(jk_map_t *m,
                             const char *wname,
                             char ***list, unsigned int *num_of_maps)
{
    char buf[1024];

    if (m && list && num_of_maps && wname) {
        char **ar = NULL;

        MAKE_WORKER_PARAM(MOUNT_OF_WORKER);
        ar = jk_map_get_string_list(m, buf, num_of_maps, NULL);
        if (ar) {
            *list = ar;
            return JK_TRUE;
        }
        *list = NULL;
        *num_of_maps = 0;
    }

    return JK_FALSE;
}

int jk_get_worker_maintain_time(jk_map_t *m)
{
    return jk_map_get_int(m, WORKER_MAINTAIN_PROPERTY_NAME,
                          DEFAULT_MAINTAIN_TIME);
}

int jk_get_worker_mx(jk_map_t *m, const char *wname, unsigned *mx)
{
    char buf[1024];

    if (m && mx && wname) {
        int i;
        MAKE_WORKER_PARAM(MX_OF_WORKER);

        i = jk_map_get_int(m, buf, -1);
        if (-1 != i) {
            *mx = (unsigned)i;
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_ms(jk_map_t *m, const char *wname, unsigned *ms)
{
    char buf[1024];

    if (m && ms && wname) {
        int i;
        MAKE_WORKER_PARAM(MS_OF_WORKER);

        i = jk_map_get_int(m, buf, -1);
        if (-1 != i) {
            *ms = (unsigned)i;
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_classpath(jk_map_t *m, const char *wname, const char **cp)
{
    char buf[1024];

    if (m && cp && wname) {
        MAKE_WORKER_PARAM(CP_OF_WORKER);

        *cp = jk_map_get_string(m, buf, NULL);
        if (*cp) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_bridge_type(jk_map_t *m, const char *wname, unsigned *bt)
{
    char buf[1024];
    const char *type;

    if (m && bt && wname) {
        MAKE_WORKER_PARAM(BRIDGE_OF_WORKER);

        type = jk_map_get_string(m, buf, NULL);

        if (type) {
            if (!strcasecmp(type, TOMCAT32_BRIDGE_NAME))
                *bt = TC32_BRIDGE_TYPE;
            else if (!strcasecmp(type, TOMCAT33_BRIDGE_NAME))
                *bt = TC33_BRIDGE_TYPE;
            else if (!strcasecmp(type, TOMCAT40_BRIDGE_NAME))
                *bt = TC40_BRIDGE_TYPE;
            else if (!strcasecmp(type, TOMCAT41_BRIDGE_NAME))
                *bt = TC41_BRIDGE_TYPE;
            else if (!strcasecmp(type, TOMCAT50_BRIDGE_NAME))
                *bt = TC50_BRIDGE_TYPE;

            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_jvm_path(jk_map_t *m, const char *wname, const char **vm_path)
{
    char buf[1024];

    if (m && vm_path && wname) {
        MAKE_WORKER_PARAM(JVM_OF_WORKER);

        *vm_path = jk_map_get_string(m, buf, NULL);
        if (*vm_path) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

/* [V] This is unused. currently. */
int jk_get_worker_callback_dll(jk_map_t *m, const char *wname, const char **cb_path)
{
    char buf[1024];

    if (m && cb_path && wname) {
        MAKE_WORKER_PARAM(NATIVE_LIB_OF_WORKER);

        *cb_path = jk_map_get_string(m, buf, NULL);
        if (*cb_path) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_cmd_line(jk_map_t *m, const char *wname, const char **cmd_line)
{
    char buf[1024];

    if (m && cmd_line && wname) {
        MAKE_WORKER_PARAM(CMD_LINE_OF_WORKER);

        *cmd_line = jk_map_get_string(m, buf, NULL);
        if (*cmd_line) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}


int jk_stat(const char *f, struct stat * statbuf)
{
  int rc;
/**
 * i5/OS V5R4 expect filename in ASCII for fopen but required them in EBCDIC for stat()
 */
#ifdef AS400_UTF8
  char *ptr;

  ptr = (char *)malloc(strlen(f) + 1);
  jk_ascii2ebcdic((char *)f, ptr);
  rc = stat(ptr, statbuf);
  free(ptr);
#else
  rc = stat(f, statbuf);
#endif

  return (rc);
}


int jk_file_exists(const char *f)
{
    if (f) {
        struct stat st;

        if ((0 == jk_stat(f, &st)) && (st.st_mode & S_IFREG))
      return JK_TRUE;
    }

    return JK_FALSE;
}

static int jk_is_some_property(const char *prp_name, const char *suffix, const char *sep)
{
    char buf[1024];

    if (prp_name && suffix) {
        size_t prp_name_len;
        size_t suffix_len;
        strcpy(buf, sep);
        strcat(buf, suffix);
        prp_name_len = strlen(prp_name);
        suffix_len = strlen(buf);
        if (prp_name_len >= suffix_len) {
            const char *prp_suffix = prp_name + prp_name_len - suffix_len;
            if (0 == strcmp(buf, prp_suffix)) {
                return JK_TRUE;
            }
        }
    }

    return JK_FALSE;
}

int jk_is_path_property(const char *prp_name)
{
    return jk_is_some_property(prp_name, "path", "_");
}

int jk_is_cmd_line_property(const char *prp_name)
{
    return jk_is_some_property(prp_name, CMD_LINE_OF_WORKER, ".");
}

int jk_is_list_property(const char *prp_name)
{
    const char **props = &list_properties[0];
    while (*props) {
        if (jk_is_some_property(prp_name, *props, "."))
            return JK_TRUE;
        props++;
    }
    return JK_FALSE;
}

int jk_is_unique_property(const char *prp_name)
{
    const char **props = &unique_properties[0];
    while (*props) {
        if (jk_is_some_property(prp_name, *props, "."))
            return JK_TRUE;
        props++;
    }
    return JK_FALSE;
}

int jk_is_deprecated_property(const char *prp_name)
{
    const char **props = &deprecated_properties[0];
    while (*props) {
        if (jk_is_some_property(prp_name, *props, "."))
            return JK_TRUE;
        props++;
    }
    return JK_FALSE;
}
/*
 * Check that property is a valid one (to prevent user typos).
 * Only property starting with worker.
 */
int jk_is_valid_property(const char *prp_name)
{
    const char **props;
    if (memcmp(prp_name, "worker.", 7))
        return JK_TRUE;

    props = &supported_properties[0];
    while (*props) {
        if (jk_is_some_property(prp_name, *props, "."))
            return JK_TRUE;
        props++;
    }
    return JK_FALSE;
}

int jk_get_worker_stdout(jk_map_t *m, const char *wname, const char **stdout_name)
{
    char buf[1024];

    if (m && stdout_name && wname) {
        MAKE_WORKER_PARAM(STDOUT_OF_WORKER);

        *stdout_name = jk_map_get_string(m, buf, NULL);
        if (*stdout_name) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_stderr(jk_map_t *m, const char *wname, const char **stderr_name)
{
    char buf[1024];

    if (m && stderr_name && wname) {
        MAKE_WORKER_PARAM(STDERR_OF_WORKER);

        *stderr_name = jk_map_get_string(m, buf, NULL);
        if (*stderr_name) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_sysprops(jk_map_t *m, const char *wname, const char **sysprops)
{
    char buf[1024];

    if (m && sysprops && wname) {
        MAKE_WORKER_PARAM(SYSPROPS_OF_WORKER);

        *sysprops = jk_map_get_string(m, buf, NULL);
        if (*sysprops) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

int jk_get_worker_libpath(jk_map_t *m, const char *wname, const char **libpath)
{
    char buf[1024];

    if (m && libpath && wname) {
        MAKE_WORKER_PARAM(LIBPATH_OF_WORKER);

        *libpath = jk_map_get_string(m, buf, NULL);
        if (*libpath) {
            return JK_TRUE;
        }
    }

    return JK_FALSE;
}

char **jk_parse_sysprops(jk_pool_t *p, const char *sysprops)
{
    char **rc = NULL;
#ifdef _REENTRANT
    char *lasts;
#endif

    if (p && sysprops) {
        char *prps = jk_pool_strdup(p, sysprops);
        if (prps && strlen(prps)) {
            unsigned num_of_prps;

            for (num_of_prps = 1; *sysprops; sysprops++) {
                if ('*' == *sysprops) {
                    num_of_prps++;
                }
            }

            rc = jk_pool_alloc(p, (num_of_prps + 1) * sizeof(char *));
            if (rc) {
                unsigned i = 0;
#ifdef _REENTRANT
                char *tmp = strtok_r(prps, "*", &lasts);
#else
                char *tmp = strtok(prps, "*");
#endif

                while (tmp && i < num_of_prps) {
                    rc[i] = tmp;
#ifdef _REENTRANT
                    tmp = strtok_r(NULL, "*", &lasts);
#else
                    tmp = strtok(NULL, "*");
#endif
                    i++;
                }
                rc[i] = NULL;
            }
        }
    }

    return rc;
}

void jk_append_libpath(jk_pool_t *p, const char *libpath)
{
    char *env = NULL;
    char *current = getenv(PATH_ENV_VARIABLE);

    if (current) {
        env = jk_pool_alloc(p, strlen(PATH_ENV_VARIABLE) +
                            strlen(current) + strlen(libpath) + 5);
        if (env) {
            sprintf(env, "%s=%s%c%s",
                    PATH_ENV_VARIABLE, libpath, PATH_SEPERATOR, current);
        }
    }
    else {
        env = jk_pool_alloc(p, strlen(PATH_ENV_VARIABLE) +
                            strlen(libpath) + 5);
        if (env) {
            sprintf(env, "%s=%s", PATH_ENV_VARIABLE, libpath);
        }
    }

    if (env) {
        putenv(env);
    }
}

void jk_init_ws_service(jk_ws_service_t *s)
{
    s->ws_private = NULL;
    s->pool = NULL;
    s->method = NULL;
    s->protocol = NULL;
    s->req_uri = NULL;
    s->remote_addr = NULL;
    s->remote_host = NULL;
    s->remote_user = NULL;
    s->auth_type = NULL;
    s->query_string = NULL;
    s->server_name = NULL;
    s->server_port = 80;
    s->server_software = NULL;
    s->content_length = 0;
    s->is_chunked = 0;
    s->no_more_chunks = 0;
    s->content_read = 0;
    s->is_ssl = JK_FALSE;
    s->ssl_cert = NULL;
    s->ssl_cert_len = 0;
    s->ssl_cipher = NULL;
    s->ssl_session = NULL;
    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = 0;
    s->attributes_names = NULL;
    s->attributes_values = NULL;
    s->num_attributes = 0;
    s->route = NULL;
    s->retries = JK_RETRIES;
    s->add_log_items = NULL;
}

#ifdef _MT_CODE_PTHREAD
jk_uint32_t jk_gettid()
{
    union {
        pthread_t tid;
        jk_uint64_t alignme;
    } u;
#ifdef AS400
    /* OS400 use 64 bits ThreadId */
    pthread_id_np_t       tid;
#endif /* AS400 */
    u.tid = pthread_self();
#ifdef AS400
    /* Get only low 32 bits for now */
    pthread_getunique_np(&(u.tid), &tid);
    return ((jk_uint32_t)(tid.intId.lo & 0xFFFFFFFF));
#else
    switch(sizeof(pthread_t)) {
    case sizeof(jk_uint32_t):
        return *(jk_uint32_t *)&u.tid;
    case sizeof(jk_uint64_t):
        return (*(jk_uint64_t *)&u.tid) & 0xFFFFFFFF;
    default:
        return 0;
    }
#endif /* AS400 */
}
#endif

/***
 * ASCII <-> EBCDIC conversions
 *
 * For now usefull only in i5/OS V5R4 where UTF and EBCDIC mode are mixed
 */

#ifdef AS400_UTF8

/* EBCDIC to ASCII translation table */
static u_char ebcdic_to_ascii[256] =
{
  0x00,0x01,0x02,0x03,0x20,0x09,0x20,0x7f, /* 00-07 */
  0x20,0x20,0x20,0x0b,0x0c,0x0d,0x0e,0x0f, /* 08-0f */
  0x10,0x11,0x12,0x13,0x20,0x0a,0x08,0x20, /* 10-17 */
  0x18,0x19,0x20,0x20,0x20,0x1d,0x1e,0x1f, /* 18-1f */
  0x20,0x20,0x1c,0x20,0x20,0x0a,0x17,0x1b, /* 20-27 */
  0x20,0x20,0x20,0x20,0x20,0x05,0x06,0x07, /* 28-2f */
  0x20,0x20,0x16,0x20,0x20,0x20,0x20,0x04, /* 30-37 */
  0x20,0x20,0x20,0x20,0x14,0x15,0x20,0x1a, /* 38-3f */
  0x20,0x20,0x83,0x84,0x85,0xa0,0xc6,0x86, /* 40-47 */
  0x87,0xa4,0xbd,0x2e,0x3c,0x28,0x2b,0x7c, /* 48-4f */
  0x26,0x82,0x88,0x89,0x8a,0xa1,0x8c,0x8b, /* 50-57 */
  0x8d,0xe1,0x21,0x24,0x2a,0x29,0x3b,0xaa, /* 58-5f */
  0x2d,0x2f,0xb6,0x8e,0xb7,0xb5,0xc7,0x8f, /* 60-67 */
  0x80,0xa5,0xdd,0x2c,0x25,0x5f,0x3e,0x3f, /* 68-6f */
  0x9b,0x90,0xd2,0xd3,0xd4,0xd6,0xd7,0xd8, /* 70-77 */
  0xde,0x60,0x3a,0x23,0x40,0x27,0x3d,0x22, /* 78-7f */
  0x9d,0x61,0x62,0x63,0x64,0x65,0x66,0x67, /* 80-87 */
  0x68,0x69,0xae,0xaf,0xd0,0xec,0xe7,0xf1, /* 88-8f */
  0xf8,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,0x70, /* 90-97 */
  0x71,0x72,0xa6,0xa7,0x91,0xf7,0x92,0xcf, /* 98-9f */
  0xe6,0x7e,0x73,0x74,0x75,0x76,0x77,0x78, /* a8-a7 */
  0x79,0x7a,0xad,0xa8,0xd1,0xed,0xe8,0xa9, /* a8-af */
  0x5e,0x9c,0xbe,0xfa,0xb8,0x15,0x14,0xac, /* b0-b7 */
  0xab,0xf3,0x5b,0x5d,0xee,0xf9,0xef,0x9e, /* b8-bf */
  0x7b,0x41,0x42,0x43,0x44,0x45,0x46,0x47, /* c0-c7 */
  0x48,0x49,0xf0,0x93,0x94,0x95,0xa2,0xe4, /* c8-cf */
  0x7d,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50, /* d0-d7 */
  0x51,0x52,0xfb,0x96,0x81,0x97,0xa3,0x98, /* d8-df */
  0x5c,0xf6,0x53,0x54,0x55,0x56,0x57,0x58, /* e0-e7 */
  0x59,0x5a,0xfc,0xe2,0x99,0xe3,0xe0,0xe5, /* e8-ef */
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37, /* f0-f7 */
  0x38,0x39,0xfd,0xea,0x9a,0xeb,0xe9,0xff  /* f8-ff */
};

/* ASCII to EBCDIC translation table */
static u_char ascii_to_ebcdic[256] =
{
  0x00,0x01,0x02,0x03,0x37,0x2d,0x2e,0x2f, /* 00-07 */
  0x16,0x05,0x25,0x0b,0x0c,0x0d,0x0e,0x0f, /* 08-0f */
  0x10,0x11,0x12,0x13,0x3c,0x3d,0x32,0x26, /* 10-17 */
  0x18,0x19,0x3f,0x27,0x22,0x1d,0x1e,0x1f, /* 18-1f */
  0x40,0x5a,0x7f,0x7b,0x5b,0x6c,0x50,0x7d, /* 20-27 */
  0x4d,0x5d,0x5c,0x4e,0x6b,0x60,0x4b,0x61, /* 28-2f */
  0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7, /* 30-37 */
  0xf8,0xf9,0x7a,0x5e,0x4c,0x7e,0x6e,0x6f, /* 38-3f */
  0x7c,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7, /* 40-47 */
  0xc8,0xc9,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6, /* 48-4f */
  0xd7,0xd8,0xd9,0xe2,0xe3,0xe4,0xe5,0xe6, /* 50-57 */
  0xe7,0xe8,0xe9,0xba,0xe0,0xbb,0xb0,0x6d, /* 58-5f */
  0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87, /* 60-67 */
  0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96, /* 68-6f */
  0x97,0x98,0x99,0xa2,0xa3,0xa4,0xa5,0xa6, /* 70-77 */
  0xa7,0xa8,0xa9,0xc0,0x4f,0xd0,0xa1,0x07, /* 78-7f */
  0x68,0xdc,0x51,0x42,0x43,0x44,0x47,0x48, /* 80-87 */
  0x52,0x53,0x54,0x57,0x56,0x58,0x63,0x67, /* 88-8f */
  0x71,0x9c,0x9e,0xcb,0xcc,0xcd,0xdb,0xdd, /* 90-97 */
  0xdf,0xec,0xfc,0x70,0xb1,0x80,0xbf,0x40, /* 98-9f */
  0x45,0x55,0xee,0xde,0x49,0x69,0x9a,0x9b, /* a8-a7 */
  0xab,0xaf,0x5f,0xb8,0xb7,0xaa,0x8a,0x8b, /* a8-af */
  0x40,0x40,0x40,0x40,0x40,0x65,0x62,0x64, /* b0-b7 */
  0xb4,0x40,0x40,0x40,0x40,0x4a,0xb2,0x40, /* b8-bf */
  0x40,0x40,0x40,0x40,0x40,0x40,0x46,0x66, /* c0-c7 */
  0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x9f, /* c8-cf */
  0x8c,0xac,0x72,0x73,0x74,0x89,0x75,0x76, /* d0-d7 */
  0x77,0x40,0x40,0x40,0x40,0x6a,0x78,0x40, /* d8-df */
  0xee,0x59,0xeb,0xed,0xcf,0xef,0xa0,0x8e, /* e0-e7 */
  0xae,0xfe,0xfb,0xfd,0x8d,0xad,0xbc,0xbe, /* e8-ef */
  0xca,0x8f,0x40,0xb9,0xb6,0xb5,0xe1,0x9d, /* f0-f7 */
  0x90,0xbd,0xb3,0xda,0xea,0xfa,0x40,0x40  /* f8-ff */
};

void jk_ascii2ebcdic(char *src, char *dst) {
    char c;

    while ((c = *src++) != 0) {
        *dst++ = ascii_to_ebcdic[(unsigned int)c];
    }

    *dst = 0;
}

void jk_ebcdic2ascii(char *src, char *dst) {
    char c;

    while ((c = *src++) != 0) {
        *dst++ = ebcdic_to_ascii[(unsigned int)c];
    }

    *dst = 0;
}

#endif
