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
 * Description: Test harness for DSAPI plugin for Lotus Domino             *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#include <stdio.h>
#include <string.h>	
#include <stdlib.h>

#include "config.h"
#include "dsapifilter.h"

unsigned int FilterInit(FilterInitData *filterInitData);
unsigned int TerminateFilter(unsigned int reserved);
unsigned int HttpFilterProc(FilterContext *context, unsigned int eventType, void *eventData);
void TestMain(void);

/* Hardwired HTTP request.
 */

#define SERVER		"localhost"
#define PORT		"80"
#define URI			"/examples/jsp/num/numguess.jsp"
#define HTTPVERSION	"1.1"

typedef struct _ServerContext {

	char *hdrs;

} ServerContext;

/* FilterContext functions */
static int fcGetRequest(struct _FilterContext *context, FilterRequest *fr,
						unsigned int *errID) {
	printf("fc.GetRequest(%p, %p, %p)\n", context, fr, errID);
	
	fr->method			= kRequestGET;
	fr->URL				= URI;
	fr->version			= HTTPVERSION;
	fr->userName		= NULL;
	fr->password		= NULL;
	fr->clientCert		= NULL;
	fr->clientCertLen	= 0;
	fr->contentRead		= NULL;
	fr->contentReadLen	= 0;

	return 1;
}

static int fcGetRequestContents(struct _FilterContext *context, char **contents,
								unsigned int *errID) {
	printf("fc.GetRequestContents(%p, %p, %p)\n", context, contents, errID);
	*contents = "";
	return 0;
}

static int fcGetServerVariable(struct _FilterContext *context, char *name, void *buffer,
								unsigned int bufferSize, unsigned int *errID) {
	printf("fc.GetServerVariable(%p, \"%s\", %p, %u, %p)\n", context, name, buffer, bufferSize, errID);

	if (strcmp(name, "SERVER_NAME") == 0) {
		strcpy((char *) buffer, SERVER);
	} else if (strcmp(name, "SERVER_PORT") == 0) {
		strcpy((char *) buffer, PORT);
	} else if (strcmp(name, "SERVER_PROTOCOL") == 0) {
		strcpy((char *) buffer, "HTTP/" VERSION);
	} else if (strcmp(name, "REMOTE_ADDR") == 0) {
		strcpy((char *) buffer, "127.0.0.1");
	} else if (strcmp(name, "SERVER_SOFTWARE") == 0) {
		strcpy((char *) buffer, "TestHarness/1.0");
	} else {
		return 0;
	}

	return 1;
}

static int fcWriteClient(struct _FilterContext * context, char *buffer, unsigned int bufferLen,
							unsigned int reserved, unsigned int *errID) {
	/* printf("fc.WriteClient(%p, %p, %u, %u, %p)\n", context, buffer, bufferLen, reserved, errID); */

	while (bufferLen > 0) {
		fputc(*buffer++, stdout);
		bufferLen--;
	}
	
	return 1;
}

static void *fcAllocMem(struct _FilterContext *context, unsigned int size,
						unsigned int reserved, unsigned int *errID) {
	printf("fc.AllocMem(%p, %d, %d, %p)\n", context, size, reserved, errID);
	return malloc(size);
}

static int fcServerSupport(struct _FilterContext *context, unsigned int funcType, void *data1,
							void *data2, unsigned int other, unsigned int *errID) {
	if (funcType == kWriteResponseHeaders) {
		FilterResponseHeaders *frh = (FilterResponseHeaders *) data1;
		printf("%d %s\n%s\n", frh->responseCode, frh->reasonText, frh->headerText);
		return 1;
	} else {
		printf("fc.ServerSupport(%p, %u, %p, %p, %u, %p)\n", context, funcType, data1, data2, other, errID);
		return 0;
	}
	
}

static int fprGetAllHeaders(FilterContext *context, char **headers, unsigned int *errID) {
	ServerContext *sc = (ServerContext *) context->serverContext;
	printf("fpr.GetAllHeaders(%p, %p, %p)\n", context, headers, errID);

	*headers = sc->hdrs;
	return strlen(sc->hdrs) + 1;
}

static int fprGetHeader(FilterContext *context, char *name, char *buffer,
						unsigned int bufferSize, unsigned int *errID) {
	printf("fpr.GetHeader(%p, \"%s\", %p, %u, %p)\n", context, name, buffer, bufferSize, errID);
	return 0;
}

static void SendRequest(void) {
	FilterContext		fc;
	FilterParsedRequest fpr;
	ServerContext		sc;
	int					rc;

	sc.hdrs =	"Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, */*\n"
				"Accept-Language: en-us\n"
				"User-Agent: Mozilla/4.0 (compatible; MSIE 5.01; Windows NT)\n"
				"Host: " SERVER "\n"
				"Content-length: 0\n";
				"Connection: Keep-Alive\n";

	fc.contextSize			= sizeof(fc);
	fc.revision				= 0;			/* or whatever */
	fc.serverContext		= &sc;
	fc.serverReserved		= 0;
	fc.securePort			= 443;
	fc.privateContext		= NULL;

	fc.GetRequest			= fcGetRequest;
	fc.GetRequestContents	= fcGetRequestContents;
	fc.GetServerVariable	= fcGetServerVariable;
	fc.WriteClient			= fcWriteClient;
	fc.AllocMem				= fcAllocMem;
	fc.ServerSupport		= fcServerSupport;

	fpr.requestMethod		= kRequestGET;
	fpr.GetAllHeaders		= fprGetAllHeaders;
	fpr.GetHeader			= fprGetHeader;
	fpr.reserved			= 0;

	rc = HttpFilterProc(&fc, kFilterParsedRequest, &fpr);
}

int main(void) {
	FilterInitData init;
	int rc;

	TestMain();

	memset(&init, 0, sizeof(init));
	rc = FilterInit(&init);

	SendRequest();

	rc = TerminateFilter(0);

	return 0;
}
