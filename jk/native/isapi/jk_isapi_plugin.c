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

/***************************************************************************
 * Description: ISAPI plugin for Tomcat                                    *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

/* Based on the the server redirector which was, in turn, based on the IIS 
 * redirector by Gal Shachor <shachor@il.ibm.com>
 */

#include "config.h"
#include "inifile.h"
#include "poolbuf.h"

/* ISAPI stuff */
#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

/* JK stuff */
#include "jk_global.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_uri_worker_map.h"

#include <stdarg.h>
#define NOERROR 0

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(DLLEXPORT)
#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif
#endif

#define VERSION				"2.0"
#define VERSION_STRING		"Jakarta/ISAPI/" VERSION

/* What we call ourselves */
#define FILTERDESC			"Apache Tomcat Interceptor (" VERSION_STRING ")"
#define SERVERDFLT			"Microsoft IIS"

/* Registry location of configuration data */
#define REGISTRY_LOCATION	"Software\\Apache Software Foundation\\Jakarta Isapi Redirector\\2.0"

/* Name of INI file relative to whatever the 'current' directory is when the filter is
 * loaded. Certainly on Linux this is the the server data directory -- it seems likely that
 * it's the same on other platforms
 */
#define ININAME				"libtomcat.ini"

/* Names of registry keys/ini items that contain commands to start, stop Tomcat */
#define TOMCAT_START		"tomcat_start"
#define TOMCAT_STOP			"tomcat_stop"
#define TOMCAT_STARTSTOP_TO	30000				/* 30 seconds */

static int					initDone	= JK_FALSE;
static jk_uri_worker_map_t	*uw_map		= NULL;
static jk_logger_t			*logger		= NULL;

static int					logLevel	= JK_LOG_EMERG_LEVEL;
static jk_pool_t			cfgPool;

static const char *logFile;
static const char *workerFile;
static const char *workerMountFile;
static const char *tomcatStart;
static const char *tomcatStop;

#if defined(JK_VERSION) && JK_VERSION >= MAKEVERSION(1, 2, 0, 1)
static jk_worker_env_t   worker_env;
#endif

static char					*crlf		= "\r\n";

typedef enum { HDR, BODY } rq_state;

typedef struct private_ws
{
	jk_pool_t			p;

	/* Passed in by server, used to access various methods and data.
	 */
    LPEXTENSION_CONTROL_BLOCK  lpEcb;

	/* True iff the response headers have been sent
	 */
	int					responseStarted;

	rq_state			state;

	poolbuf				hdr;
	poolbuf				body;

} private_ws_t;

/* These three functions are called back (indirectly) by
 * Tomcat during request processing. StartResponse() sends
 * the headers associated with the response.
 */
static int JK_METHOD StartResponse(jk_ws_service_t * s, int status, const char *reason,
									const char *const *hdrNames,
									const char *const *hdrValues, unsigned hdrCount);
/* Read() is called by Tomcat to read from the request body (if any).
 */
static int JK_METHOD Read(jk_ws_service_t * s, void *b, unsigned l, unsigned *a);
/* Write() is called by Tomcat to send data back to the client.
 */
static int JK_METHOD Write(jk_ws_service_t * s, const void *b, unsigned l);

static int ReadInitData(void);

#ifndef USE_INIFILE
static const char *GetRegString(HKEY hkey, const char *key);
#endif

//static unsigned int ParsedRequest(PHTTP_FILTER_CONTEXT *context, FilterParsedRequest *reqData);

/* Case insentive memcmp() clone
 */
#ifdef HAVE_MEMICMP
#define NoCaseMemCmp(ci, cj, l) _memicmp((void *) (ci), (void *) (cj), (l))
#else
static int NoCaseMemCmp(const char *ci, const char *cj, int len)
{
	if (0 == memcmp(ci, cj, len))
		return 0;
	while (len > 0)
	{
		int cmp = tolower(*ci) - tolower(*cj);
		if (cmp != 0) return cmp;
		ci++;
		cj++;
		len--;
	}
	return 0;
}
#endif

/* Case insentive strcmp() clone
 */
#ifdef HAVE_STRICMP
#define NoCaseStrCmp(si, sj) _stricmp((void *) (si), (void *) (sj))
#else
static int NoCaseStrCmp(const char *si, const char *sj)
{
	if (0 == strcmp(si, sj))
		return 0;

	while (*si && tolower(*si) == tolower(*sj))
		si++, sj++;

	return tolower(*si) - tolower(*sj);
}
#endif

/* Case insensitive substring search.
 * str		string to search
 * slen		length of string to search
 * ptn		pattern to search for
 * plen		length of pattern
 * returns	1 if there's a match otherwise 0
 */
static int FindPathElem(const char *str, int slen, const char *ptn, int plen)
{
	const char *sp = str;

	while (slen >= plen)
	{
		/* We're looking for a match for the specified string bounded by
		 * the start of the string, \ or / at the left and the end of the
		 * string, \ or / at the right. We look for \ as well as / on the
		 * suspicion that a Windows hosted server might accept URIs
		 * containing \.
		 */
		if (NoCaseMemCmp(sp, ptn, plen) == 0 &&
			(sp == str || *sp == '\\' || *sp == '/') &&
			(*sp == '\0' || *sp == '\\' || *sp == '/'))
			return 1;
		slen--;
		sp++;
	}
	return 0;
}

static void LogMessage(char *msg, unsigned short code, ...)
{
	va_list ap;

	if (code != NOERROR)
		printf("Error %d: ", code);

	va_start(ap, code);
	vprintf(msg, ap);
	va_end(ap);
	printf("\n");
}

/* Get the value of a server (CGI) variable as a string
 */
static int GetVariable(private_ws_t *ws, char *hdrName,
					 char *buf, DWORD bufsz, char **dest, const char *dflt)
{
	LPEXTENSION_CONTROL_BLOCK lpEcb = ws->lpEcb;

	if (lpEcb->GetServerVariable(lpEcb->ConnID, hdrName, buf, (LPDWORD) &bufsz))
	{
		if (bufsz > 0) buf[bufsz-1] = '\0';
		*dest = jk_pool_strdup(&ws->p, buf);
		return JK_TRUE;
	}

	*dest = jk_pool_strdup(&ws->p, dflt);
	return JK_FALSE;
}

/* Get the value of a server (CGI) variable as an integer
 */
static int GetVariableInt(private_ws_t *ws, char *hdrName,
						char *buf, DWORD bufsz, int *dest, int dflt)
{
	LPEXTENSION_CONTROL_BLOCK lpEcb = ws->lpEcb;

	if (lpEcb->GetServerVariable(lpEcb->ConnID, hdrName, buf, (LPDWORD) &bufsz))
	{
		if (bufsz > 0) buf[bufsz-1] = '\0';
		*dest = atoi(buf);
		return JK_TRUE;
	}

	*dest = dflt;
	return JK_FALSE;
}
/* Get the value of a server (CGI) variable as a boolean switch
 */
static int GetVariableBool(private_ws_t *ws, char *hdrName,
						char *buf, DWORD bufsz, int *dest, int dflt)
{
	LPEXTENSION_CONTROL_BLOCK lpEcb = ws->lpEcb;

	if (lpEcb->GetServerVariable(lpEcb->ConnID, hdrName, buf, (LPDWORD) &bufsz))
	{
		if (bufsz > 0) buf[bufsz-1] = '\0';
		if (isdigit(buf[0]))
			*dest = atoi(buf) != 0;
		else if (NoCaseStrCmp(buf, "yes") == 0 || NoCaseStrCmp(buf, "on") == 0)
			*dest = 1;
		else
			*dest = 0;
		return JK_TRUE;
	}

	*dest = dflt;
	return JK_FALSE;
}

/* A couple of utility macros to supply standard arguments to GetVariable() and
 * GetVariableInt().
 */
#define GETVARIABLE(name, dest, dflt)		GetVariable(ws, (name), workBuf, sizeof(workBuf), (dest), (dflt))
#define GETVARIABLEINT(name, dest, dflt)	GetVariableInt(ws, (name), workBuf, sizeof(workBuf), (dest), (dflt))
#define GETVARIABLEBOOL(name, dest, dflt)	GetVariableBool(ws, (name), workBuf, sizeof(workBuf), (dest), (dflt))

/* Return 1 iff the supplied string contains "web-inf" (in any case
 * variation. We don't allow URIs containing web-inf, although
 * FindPathElem() actually looks for the string bounded by path punctuation
 * or the ends of the string, so web-inf must appear as a single element
 * of the supplied URI
 */
static int BadURI(const char *uri)
{
	static char *wi = "web-inf";
	return FindPathElem(uri, strlen(uri), wi, strlen(wi));
}

/* Replacement for strcat() that updates a buffer pointer. It's
 * probably marginal, but this should be more efficient that strcat()
 * in cases where the string being concatenated to gets long because
 * strcat() has to count from start of the string each time.
 */
static void Append(char **buf, const char *str)
{
	int l = strlen(str);
	memcpy(*buf, str, l);
	(*buf)[l] = '\0';
	*buf += l;
}

/* Start the response by sending any headers. Invoked by Tomcat. I don't
 * particularly like the fact that this always allocates memory, but
 * perhaps jk_pool_alloc() is efficient.
 */
static int JK_METHOD StartResponse(jk_ws_service_t *s, int status, const char *reason,
									const char *const *hdrNames,
									const char *const *hdrValues, unsigned hdrCount)
{
	DEBUG(("StartResponse()\n"));
	jk_log(logger, JK_LOG_DEBUG, "Into jk_ws_service_t::StartResponse\n");

	if (status < 100 || status > 1000)
	{
		jk_log(logger, JK_LOG_ERROR, "jk_ws_service_t::StartResponse, invalid status %d\n", status);
		return JK_FALSE;
	}

	if (s && s->ws_private)
	{
		private_ws_t *p = s->ws_private;

		if (!p->responseStarted)
		{
			char *statBuf;
			char *hdrBuf;
			size_t statLen;

			p->responseStarted = JK_TRUE;

			if (NULL == reason)
				reason = "";

			/* TODO: coallesce the to jk_pool_alloc() calls into a single
			 * buffer alloc
			 */
			statLen = 4 + strlen(reason);
			statBuf = jk_pool_alloc(&p->p, statLen + 1);

			/* slightly quicker than sprintf() we hope */
			statBuf[0] = (status / 100) % 10 + '0';
			statBuf[1] = (status /  10) % 10 + '0';
			statBuf[2] = (status /   1) % 10 + '0';
			statBuf[3] = ' ';
			strcpy(statBuf + 4, reason);

			/* Build a single string containing all the headers
			 * because that's what the server needs.
			 */
			if (hdrCount > 0)
			{
				unsigned i;
				unsigned hdrLen;
				char *bufp;

				for (i = 0, hdrLen = 3; i < hdrCount; i++)
					hdrLen += strlen(hdrNames[i]) + strlen(hdrValues[i]) + 4;

				hdrBuf = jk_pool_alloc(&p->p, hdrLen);
				bufp = hdrBuf;

				for (i = 0; i < hdrCount; i++)
				{
					Append(&bufp, hdrNames[i]);
					Append(&bufp, ": ");
					Append(&bufp, hdrValues[i]);
					Append(&bufp, crlf);
				}

				Append(&bufp, crlf);
			}
			else
			{
				hdrBuf = crlf;
			}

			DEBUG(("%d %s\n%s", status, reason, hdrBuf));

			/* TODO: check API docs for this */
			if (!p->lpEcb->ServerSupportFunction(p->lpEcb->ConnID,
						HSE_REQ_SEND_RESPONSE_HEADER,
						statBuf, (LPDWORD) &statLen, (LPDWORD) hdrBuf))
			{
                jk_log(logger, JK_LOG_ERROR, 
                       "jk_ws_service_t::start_response, ServerSupportFunction failed\n");
                return JK_FALSE;
            }       

		}
		return JK_TRUE;
	}

	jk_log(logger, JK_LOG_ERROR, "jk_ws_service_t::StartResponse, NULL parameters\n");

	return JK_FALSE;
}

static int JK_METHOD Read(jk_ws_service_t * s, void *bytes, unsigned len, unsigned *countp)
{
#if 0
	DEBUG(("Read(%p, %p, %u, %p)\n", s, bytes, len, countp));
	jk_log(logger, JK_LOG_DEBUG, "Into jk_ws_service_t::Read\n");

	if (s && s->ws_private && bytes && countp)
	{
		private_ws_t *p = s->ws_private;

		/* Copy data from the server's buffer. Although it seems slightly
		 * improbably we're believing that the server always buffers the
		 * entire request in memory. Not properly tested yet.
		 */
		if (len > p->reqSize) len = p->reqSize;
		memcpy(bytes, p->reqBuffer, len);
		p->reqBuffer += len;
		p->reqSize -= len;
		*countp = len;
		return JK_TRUE;
	}

	jk_log(logger, JK_LOG_ERROR, "jk_ws_service_t::Read, NULL parameters\n");

#endif
	return JK_FALSE;
}

static int JK_METHOD Write(jk_ws_service_t *s, const void *bytes, unsigned len)
{
	DEBUG(("Write(%p, %p, %u)\n", s, bytes, len));
	jk_log(logger, JK_LOG_DEBUG, "Into jk_ws_service_t::Write\n");

	if (s && s->ws_private && bytes)
	{
		private_ws_t *p = s->ws_private;
		DWORD dwLen = len;

		/* Make sure the response has really started. I'm almost certain
		 * this isn't necessary, but it was in the ISAPI code, so it's in
		 * here too.
		 */
		if (!p->responseStarted)
			StartResponse(s, 200, NULL, NULL, NULL, 0);

		DEBUG(("Writing %d bytes of content\n", len));

		/* Send the data */
		if (len > 0)
		{
			if (!p->lpEcb->WriteClient(p->lpEcb->ConnID, (LPVOID) bytes, &dwLen, 0))
			{
                jk_log(logger, JK_LOG_ERROR, "jk_ws_service_t::Write, WriteClient failed\n");
                return JK_FALSE;
			}
		}
		
		return JK_TRUE;
	}

	jk_log(logger, JK_LOG_ERROR, "jk_ws_service_t::Write, NULL parameters\n");

	return JK_FALSE;
}

static int RunProg(const char *cmd)
{
#ifdef WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
    si.cb			= sizeof(si);    // Start the child process.
	si.dwFlags		= STARTF_USESHOWWINDOW;
	si.wShowWindow	= SW_SHOWMAXIMIZED;

	if (!CreateProcess(NULL, (char *) cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		DWORD err = GetLastError();
		LogMessage("Command \"%s\" (error %u)", NOERROR, cmd, err);
		return FALSE;
	}

	if (WAIT_OBJECT_0 == WaitForSingleObject(pi.hProcess, TOMCAT_STARTSTOP_TO))
		return TRUE;

	LogMessage("Command \"%s\" didn't complete in time", NOERROR, cmd);
	return FALSE;

#else
	int err = system(cmd);
	if (0 == err) return 1;
	LogMessage("Command \"%s\" failed (error %d)", NOERROR, cmd, err);
	return 0;
#endif
}

/* Called when the filter is unloaded. Free various resources and
 * display a banner.
 */
BOOL WINAPI TerminateFilter(DWORD dwFlags)
{
	if (initDone)
	{
		uri_worker_map_free(&uw_map, logger);
		wc_close(logger);
		if (logger)
			jk_close_file_logger(&logger);

		initDone = JK_FALSE;
	}

	if (NULL != tomcatStop && '\0' != *tomcatStop)
	{
		LogMessage("Attempting to stop Tomcat: %s", NOERROR, tomcatStop);
		RunProg(tomcatStop);
	}

	LogMessage(FILTERDESC " unloaded", NOERROR);

	jk_close_pool(&cfgPool);

	return TRUE;
}

/* Called when the server loads the filter. Reads a load of config data from
 * the registry and elsewhere and displays a banner.
 */
BOOL WINAPI GetFilterVersion(PHTTP_FILTER_VERSION pVer)
{    
	jk_open_pool(&cfgPool, NULL, 0);		/* empty pool for config data */

	if (!ReadInitData())
		goto initFailed;

	if (!jk_open_file_logger(&logger, logFile, logLevel))
		logger = NULL;

	if (NULL != tomcatStart && '\0' != *tomcatStart)
	{
		LogMessage("Attempting to start Tomcat: %s", NOERROR, tomcatStart);
		RunProg(tomcatStart);
	}

    pVer->dwFilterVersion = pVer->dwServerFilterVersion;
                        
    //if (pVer->dwFilterVersion > HTTP_FILTER_REVISION)
    //    pVer->dwFilterVersion = HTTP_FILTER_REVISION;

	/* Come back and check these... */
    pVer->dwFlags = SF_NOTIFY_ORDER_HIGH        | 
                    SF_NOTIFY_SECURE_PORT       | 
                    SF_NOTIFY_NONSECURE_PORT    |
                    SF_NOTIFY_PREPROC_HEADERS;
                    
    strcpy(pVer->lpszFilterDesc, FILTERDESC);

	/* Banner */
	LogMessage("%s loaded", NOERROR, pVer->lpszFilterDesc);

	return TRUE;

initFailed:
	LogMessage("Error loading %s", NOERROR, FILTERDESC);

	return FALSE;
}

/* Read parameters from the registry
 */
static int ReadInitData(void)
{
	int ok = JK_TRUE;
	const char *v;

#ifdef USE_INIFILE
// Using an INIFILE

#define GETV(key) inifile_lookup(key)

	ERRTYPE e;

	if (e = inifile_read(&cfgPool, ININAME), ERRNONE != e)
	{
		LogMessage("Error reading: %s, %s", NOERROR, ININAME, ERRTXT(e));
		return JK_FALSE;
	}

#else
// Using the registry
#define GETV(key) GetRegString(hkey, key)

	HKEY hkey;
	long rc;

	rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGISTRY_LOCATION, (DWORD) 0, KEY_READ, &hkey);
	if (ERROR_SUCCESS != rc) return JK_FALSE;

#endif

#define GETVNB(tag, var) \
	var = GETV(tag); \
	if (NULL == var) \
	{ \
		LogMessage("%s not defined in %s", NOERROR, tag, ININAME); \
		ok = JK_FALSE; \
	}

	GETVNB(JK_LOG_FILE_TAG, logFile)
	GETVNB(JK_LOG_LEVEL_TAG, v);
	GETVNB(JK_WORKER_FILE_TAG, workerFile);
	GETVNB(JK_MOUNT_FILE_TAG, workerMountFile);

	logLevel = (NULL == v) ? 0 : jk_parse_log_level(v);

	tomcatStart	= GETV(TOMCAT_START);
	tomcatStop	= GETV(TOMCAT_STOP);

#ifndef USE_INIFILE
	RegCloseKey(hkey);
#endif

	return ok;
}

#ifndef USE_INIFILE
static const char *GetRegString(HKEY hkey, const char *key)
{
	DWORD type = 0;
	DWORD sz = 0;
	LONG rc;
	char *val;

	rc = RegQueryValueEx(hkey, key, (LPDWORD) 0, &type, NULL, &sz);
	if (rc != ERROR_SUCCESS || type != REG_SZ)
		return NULL;

	if (val = jk_pool_alloc(&cfgPool, sz), NULL == val)
		return NULL;

	rc = RegQueryValueEx(hkey, key, (LPDWORD) 0, &type, val, &sz);
	if (rc == ERROR_SUCCESS)
		return val;

	return NULL;
}
#endif

/* Main entry point for the filter. Called by the server for every HTTP request.
 */
DWORD WINAPI HttpFilterProc(PHTTP_FILTER_CONTEXT pfc,
                            DWORD dwNotificationType, 
                            LPVOID pvNotification)
{
#if 0
	switch (dwNotificationType)
	{
	case kFilterParsedRequest:
		return ParsedRequest(context, (FilterParsedRequest *) eventData);
	default:
		break;
	}
	return kFilterNotHandled;
#endif

	return SF_STATUS_REQ_NEXT_NOTIFICATION;
}

/* Send a simple response. Used when we don't want to bother Tomcat,
 * which in practice means for various error conditions that we can
 * detect internally.
 */
static void SimpleResponse(PHTTP_FILTER_CONTEXT *context, int status, char *reason, char *body)
{
#if 0
	FilterResponseHeaders frh;
	int rc, errID;
	char hdrBuf[35];

	sprintf(hdrBuf, "Content-type: text/html%s%s", crlf, crlf);

	frh.responseCode = status;
	frh.reasonText = reason;
	frh.headerText = hdrBuf;

	rc = context->ServerSupport(context, kWriteResponseHeaders, &frh, NULL, 0, &errID);
	rc = context->WriteClient(context, body, strlen(body), 0, &errID);
#endif
}

/* Called to reject a URI that contains the string "web-inf". We block
 * these because they may indicate an attempt to invoke arbitrary code.
 */
static DWORD RejectBadURI(PHTTP_FILTER_CONTEXT *context)
{
	static char *msg = "<HTML><BODY><H1>Access is Forbidden</H1></BODY></HTML>";

	SimpleResponse(context, 403, "Forbidden", msg);
	return SF_STATUS_REQ_NEXT_NOTIFICATION;
}

/* Allocate space for a string given a start pointer and an end pointer
 * and return a pointer to the allocated, copied string.
 */
static char *MemDup(private_ws_t *ws, const char *start, const char *end)
{
	char *out = NULL;

	if (start != NULL && end != NULL && end > start)
	{
		int len = end - start;
		out = jk_pool_alloc(&ws->p, len + 1);
		memcpy(out, start, len);
		out[len] = '\0';
	}

	return out;
}

/* Given all the HTTP headers as a single string parse them into individual
 * name, value pairs. Called twice: once to work out how many headers there
 * are, then again to copy them.
 */
static int ParseHeaders(private_ws_t *ws, const char *hdrs, int hdrsz, jk_ws_service_t *s)
{
	int hdrCount = 0;
	const char *limit = hdrs + hdrsz;
	const char *name, *nameEnd;
	const char *value, *valueEnd;

	while (hdrs < limit)
	{
		/* Skip line *before* doing anything, cos we want to lose the first line which
		 * contains the request.
		 */
		while (hdrs < limit && (*hdrs != '\n' && *hdrs != '\r'))
			hdrs++;
		while (hdrs < limit && (*hdrs == '\n' || *hdrs == '\r'))
			hdrs++;

		if (hdrs >= limit)
			break;

		name = nameEnd = value = valueEnd = NULL;

		name = hdrs;
		while (hdrs < limit && *hdrs >= ' ' && *hdrs != ':')
			hdrs++;
		nameEnd = hdrs;

		if (hdrs < limit && *hdrs == ':')
		{
			hdrs++;
			while (hdrs < limit && (*hdrs == ' ' || *hdrs == '\t'))
				hdrs++;
			value = hdrs;
			while (hdrs < limit && *hdrs >= ' ')
				hdrs++;
			valueEnd = hdrs;
		}

		if (s->headers_names != NULL && s->headers_values != NULL)
		{
			s->headers_names[hdrCount]	= MemDup(ws, name, nameEnd);
			s->headers_values[hdrCount] = MemDup(ws, value, valueEnd);
			DEBUG(("%s = %s\n", s->headers_names[hdrCount], s->headers_values[hdrCount]));
		}
		hdrCount++;
	}

	return hdrCount;
}

#if 0
/* Set up all the necessary jk_* workspace based on the current HTTP request.
 */
static int InitService(private_ws_t *ws, jk_ws_service_t *s)
{
	/* This is the only fixed size buffer left. It won't be overflowed
	 * because the the server API that reads into the buffer accepts a length
	 * constraint, and it's unlikely ever to be exhausted because the
	 * strings being will typically be short, but it's still aesthetically
	 * troublesome.
	 */
	char workBuf[16 * 1024];
	FilterRequest fr;
	char *hdrs, *qp;
	int hdrsz;
	int errID;
	int hdrCount;
	int rc /*, dummy*/;

	static char *methodName[] = { "", "HEAD", "GET", "POST", "PUT", "DELETE" };

	rc = ws->context->GetRequest(ws->context, &fr, &errID);

	s->jvm_route		= NULL;
	s->start_response	= StartResponse;
	s->read				= Read;
	s->write			= Write;

	s->req_uri = jk_pool_strdup(&ws->p, fr.URL);
	s->query_string = NULL;
	if (qp = strchr(s->req_uri, '?'), qp != NULL)
	{
		*qp++ = '\0';
		if (strlen(qp))
			s->query_string = qp;
	}

	GETVARIABLE("AUTH_TYPE", &s->auth_type, "");
	GETVARIABLE("REMOTE_USER", &s->remote_user, "");
	GETVARIABLE("SERVER_PROTOCOL", &s->protocol, "");
	GETVARIABLE("REMOTE_HOST", &s->remote_host, "");
	GETVARIABLE("REMOTE_ADDR", &s->remote_addr, "");
	GETVARIABLE("SERVER_NAME", &s->server_name, "");
	GETVARIABLEINT("SERVER_PORT", &s->server_port, 80);
	GETVARIABLE("SERVER_SOFTWARE", &s->server_software, SERVERDFLT);
	GETVARIABLEINT("CONTENT_LENGTH", &s->content_length, 0);


	/* SSL Support
	 */
	GETVARIABLEBOOL("HTTPS", &s->is_ssl, 0);

	if (ws->reqData->requestMethod < 0 ||
		ws->reqData->requestMethod >= sizeof(methodName) / sizeof(methodName[0]))
		return JK_FALSE;

	s->method = methodName[ws->reqData->requestMethod];

	s->headers_names	= NULL;
	s->headers_values	= NULL;
	s->num_headers		= 0;

	s->ssl_cert_len	= fr.clientCertLen;
	s->ssl_cert		= fr.clientCert;
	s->ssl_cipher	= NULL;		/* required by Servlet 2.3 Api */
	s->ssl_session	= NULL;

#if defined(JK_VERSION) && JK_VERSION >= MAKEVERSION(1, 2, 0, 1)
	s->ssl_key_size = -1;       /* required by Servlet 2.3 Api, added in jtc */
#endif

	if (s->is_ssl)
	{
		int dummy;
#if 0
		char *sslNames[] =
		{
			"CERT_ISSUER", "CERT_SUBJECT", "CERT_COOKIE", "CERT_FLAGS", "CERT_SERIALNUMBER",
			"HTTPS_SERVER_SUBJECT", "HTTPS_SECRETKEYSIZE", "HTTPS_SERVER_ISSUER", "HTTPS_KEYSIZE"
		};

		char *sslValues[] =
		{
			NULL, NULL, NULL, NULL, NULL,
			NULL, NULL, NULL, NULL
		};


		unsigned i, varCount = 0;
#endif

		DEBUG(("SSL request\n"));

#if defined(JK_VERSION) && JK_VERSION >= MAKEVERSION(1, 2, 0, 1)
		/* Read the variable into a dummy variable: we do this for the side effect of
		 * reading it into workBuf.
		 */
		GETVARIABLEINT("HTTPS_KEYSIZE", &dummy, 0);
		if (workBuf[0] == '[')
			s->ssl_key_size = atoi(workBuf+1);
#else
		(void) dummy;
#endif

#if 0
		for (i = 0; i < sizeof(sslNames)/sizeof(sslNames[0]); i++)
		{
			GETVARIABLE(sslNames[i], &sslValues[i], NULL);
			if (sslValues[i]) varCount++;
		}

		/* Andy, some SSL vars must be mapped directly in  s->ssl_cipher,
         * ssl->session and s->ssl_key_size
		 * ie:
		 * Cipher could be "RC4-MD5"
		 * KeySize 128 (bits)
	     * SessionID a string containing the UniqID used in SSL dialogue
         */
		if (varCount > 0)
		{
			unsigned j;

			s->attributes_names = jk_pool_alloc(&ws->p, varCount * sizeof (char *));
			s->attributes_values = jk_pool_alloc(&ws->p, varCount * sizeof (char *));

			j = 0;
			for (i = 0; i < sizeof(sslNames)/sizeof(sslNames[0]); i++)
			{
				if (sslValues[i])
				{
					s->attributes_names[j] = sslNames[i];
					s->attributes_values[j] = sslValues[i];
					j++;
				}
			}
			s->num_attributes = varCount;
		}
#endif
	}

	/* Duplicate all the headers now */

	hdrsz = ws->reqData->GetAllHeaders(ws->context, &hdrs, &errID);
	DEBUG(("\nGot headers (length %d)\n--------\n%s\n--------\n\n", hdrsz, hdrs));

	s->headers_names =
	s->headers_values = NULL;
	hdrCount = ParseHeaders(ws, hdrs, hdrsz, s);
	DEBUG(("Found %d headers\n", hdrCount));
	s->num_headers = hdrCount;
	s->headers_names	= jk_pool_alloc(&ws->p, hdrCount * sizeof(char *));
	s->headers_values	= jk_pool_alloc(&ws->p, hdrCount * sizeof(char *));
	hdrCount = ParseHeaders(ws, hdrs, hdrsz, s);

	return JK_TRUE;
}
#endif

#if 0
/* Handle an HTTP request. Works out whether Tomcat will be interested then either
 * despatches it to Tomcat or passes it back to the server.
 */
static unsigned int ParsedRequest(PHTTP_FILTER_CONTEXT *context, FilterParsedRequest *reqData)
{
	unsigned int errID;
	int rc;
	FilterRequest fr;
	int result = kFilterNotHandled;

	DEBUG(("\nParsedRequest starting\n"));

	rc = context->GetRequest(context, &fr, &errID);

	if (fr.URL && strlen(fr.URL))
	{
		char *uri = fr.URL;
		char *workerName, *qp;

		if (!initDone)
		{
			/* One time initialisation which is deferred so that we have the name of
			 * the server software to plug into worker_env
			 */
			int ok = JK_FALSE;
			jk_map_t *map = NULL;

			DEBUG(("Initialising worker map\n"));

			if (map_alloc(&map))
			{
				if (map_read_properties(map, workerMountFile))
					if (uri_worker_map_alloc(&uw_map, map, logger))
						ok = JK_TRUE;
				map_free(&map);
			}

			DEBUG(("Got the URI worker map\n"));

			if (ok)
			{
				ok = JK_FALSE;
				DEBUG(("About to allocate map\n"));
				if (map_alloc(&map))
				{
					DEBUG(("About to read %s\n", workerFile));
					if (map_read_properties(map, workerFile))
					{
#if defined(JK_VERSION) && JK_VERSION >= MAKEVERSION(1, 2, 0, 1)
						char server[256];

						worker_env.uri_to_worker = uw_map;
						if (context->GetServerVariable(context, "SERVER_SOFTWARE", server, sizeof(server)-1, &errID))
							worker_env.server_name = jk_pool_strdup(&cfgPool, server);
						else
							worker_env.server_name = SERVERDFLT;

						DEBUG(("Server name %s\n", worker_env.server_name));

						if (wc_open(map, &worker_env, logger))
							ok = JK_TRUE;
#else
						if (wc_open(map, logger))
							ok = JK_TRUE;
#endif
						DEBUG(("OK = %d\n", ok));
					}

					DEBUG(("Read %s, OK = %d\n", workerFile, ok));
					map_free(&map);
				}
			}

			if (!ok) return kFilterError;
			initDone = JK_TRUE;
		}


		if (qp = strchr(uri, '?'), qp != NULL) *qp = '\0';
		workerName = map_uri_to_worker(uw_map, uri, logger);
		if (qp) *qp = '?';

		DEBUG(("Worker for this URL is %s\n", workerName));

		if (NULL != workerName)
		{
			private_ws_t ws;
			jk_ws_service_t s;
			jk_pool_atom_t buf[SMALL_POOL_SIZE];

			if (BadURI(uri))
				return RejectBadURI(context);

			/* Go dispatch the call */

			jk_init_ws_service(&s);
			jk_open_pool(&ws.p, buf, sizeof (buf));

			ws.responseStarted	= JK_FALSE;
			ws.context			= context;
			ws.reqData			= reqData;

			ws.reqSize = context->GetRequestContents(context, &ws.reqBuffer, &errID);

			s.ws_private = &ws;
			s.pool = &ws.p;

			if (InitService(&ws, &s))
			{
				jk_worker_t *worker = wc_get_worker_for_name(workerName, logger);

				jk_log(logger, JK_LOG_DEBUG, "HttpExtensionProc %s a worker for name %s\n",
					   worker ? "got" : "could not get", workerName);

				if (worker)
				{
					jk_endpoint_t *e = NULL;

					if (worker->get_endpoint(worker, &e, logger))
					{
						int recover = JK_FALSE;

						if (e->service(e, &s, logger, &recover))
						{
							result = kFilterHandledRequest;
							jk_log(logger, JK_LOG_DEBUG, "HttpExtensionProc service() returned OK\n");
							DEBUG(("HttpExtensionProc service() returned OK\n"));
						}
						else
						{
							result = kFilterError;
							jk_log(logger, JK_LOG_ERROR, "HttpExtensionProc error, service() failed\n");
							DEBUG(("HttpExtensionProc error, service() failed\n"));
						}
						e->done(&e, logger);
					}
				}
				else
				{
					jk_log(logger, JK_LOG_ERROR,
						   "HttpExtensionProc error, could not get a worker for name %s\n",
						   workerName);
				}
			}

			jk_close_pool(&ws.p);
		}
	}

	return result;
}
#endif

BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, LPVOID lpReserved)
{
    BOOL fReturn = TRUE;

    switch (ulReason) {

    case DLL_PROCESS_DETACH:
        TerminateFilter(HSE_TERM_MUST_UNLOAD);
        break;

    default:
        break;
    } 

    return fReturn;
}
