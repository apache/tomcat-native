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

#include "ajp_logon.h"
#include "ajp.h"



/**
 * Binary to hex C String (null terminated)
 *
 * @param org        byte array to transform
 * @param dst        string result
 * @param n          len of byte array
 * @return           APR_SUCCESS or error
 */
static char * hextocstr(apr_byte_t *org, char *dst, int n)
{
    char       *os = dst;
    apr_byte_t  v;
    static char zitohex[] = "0123456789ABCDEF";

    while (--n >= 0) {
        v = *org++;
        *dst++ = zitohex[v >> 4];
        *dst++ = zitohex[v & 0x0f];
    }
    *dst = 0;

    return (os);
}

/**
 * Compute the MD5 of org and (if not null org2) string
 *
 * @param org        First String to compute MD5 from
 * @param org2       Second String to compute MD5 from (if null no action)
 * @param dst        Destination MD5 Hex CString
 * @return           APR_SUCCESS or error
 */
apr_status_t comp_md5(char *org, char *org2, char *dst)
{
    apr_md5_ctx_t ctx;
    unsigned char buf[AJP14_MD5_DIGESTSIZE + 1];

    apr_md5_init(&ctx);
    apr_md5_update(&ctx, org, (apr_size_t)strlen(org));

    if (org2 != NULL)
        apr_md5_update(&ctx, org2, (apr_size_t)strlen(org2));

    apr_md5_final(buf, &ctx);

    hextocstr(buf, dst, AJP14_MD5_DIGESTSIZE);
    
    return APR_SUCCESS;
}

/**
 * Decode the Incoming Login Command and build reply
 *
 * @param msg        AJP Message to be decoded and then filled
 * @param secret     secret string to be used in logon phase
 * @param servername local server name (ie: Apache 2.0.50)
 * @return           APR_SUCCESS or error
 */
apr_status_t ajp_handle_login(ajp_msg_t *msg, char *secret, char *servername)
{
    int             status;
    char            *entropy;
    char            computedKey[AJP14_COMPUTED_KEY_LEN];

    status = ajp_msg_get_string(msg, &entropy);
    
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_login(): can't get seed");

        return AJP_ELOGFAIL;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "ajp_handle_login(): received entropy %s",
                 entropy);

    comp_md5(entropy, secret, computedKey);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "ajp_handle_login(): computed md5 (%s/%s) -> (%s)",
                 entropy, secret, computedKey);

    ajp_msg_reset(msg);

    /* LOGCOMP CMD */    
    status = ajp_msg_append_uint8(msg, AJP14_LOGCOMP_CMD);
    
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_login(): can't log command");

        return AJP_ELOGFAIL;
    }

    /* COMPUTED-SEED */
    status = ajp_msg_append_string(msg, computedKey);

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_login(): can't serialize computed secret");

        return AJP_ELOGFAIL;
    }

    /* NEGOCIATION OPTION */    
    status = ajp_msg_append_uint32(msg, AJP14_CONTEXT_INFO_NEG | AJP14_PROTO_SUPPORT_AJP14_NEG);

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_login(): can't append negociation header");

        return AJP_ELOGFAIL;
    }

    /* SERVER NAME */    
    status = ajp_msg_append_string(msg, servername);

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_login(): can't serialize server name");

        return AJP_ELOGFAIL;
    }

    return APR_SUCCESS;
}


/**
 * Decode the LogOk Command. After that we're done, the connection is
 * perfect and ready.
 *
 * @param msg        AJP Message to be decoded
 * @return           APR_SUCCESS or error
 */
apr_status_t ajp_handle_logok(ajp_msg_t *msg)
{
    apr_status_t status;
    apr_uint32_t negociation;
    char         *server_name;

    status = ajp_msg_get_uint32(msg, &negociation);
    
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_logok(): can't get negociation header");

        return AJP_ELOGFAIL;
    }

    status = ajp_msg_get_string(msg, &server_name);

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_logok(): can't get servlet engine name");

        return AJP_ELOGFAIL;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "ajp_handle_logok(): Successfully logged to %s",
                 server_name);

    return APR_SUCCESS;
}


/**
 * Decode the Log Nok Command 
 *
 * @param msg        AJP Message to be decoded
 */
apr_status_t ajp_handle_lognok(ajp_msg_t *msg)
{
    apr_status_t status;
    apr_uint32_t failurecode;

    status = ajp_msg_get_uint32(msg, &failurecode);

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                      "ajp_handle_lognok(): can't get failure code");

        return AJP_ELOGFAIL;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "ajp_handle_logok(): logon failure code is %08lx",
                 (long)failurecode);

    return APR_SUCCESS;
}
