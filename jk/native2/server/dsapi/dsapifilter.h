/*
 * Copyright 1999-2001,2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/***************************************************************************
 * Description: DSAPI plugin for Lotus Domino                              *
 * Author:      Andy Armstrong <andy@tagish.com>                           *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifndef __dsapifilter_h
#define __dsapifilter_h

#define kInterfaceVersion   2
#define kMaxFilterDesc      255

typedef enum {
    kFilterNotHandled       = 0,
    kFilterHandledRequest   = 1,
    kFilterHandledEvent     = 2,
    kFilterError            = 3
} FilterReturnCode;

typedef enum {
    kFilterRawRequest       = 0x01,
    kFilterParsedRequest    = 0x02,
    kFilterAuthUser         = 0x04,
    kFilterUserNameList     = 0x08,
    kFilterMapURL           = 0x10,
    kFilterResponse         = 0x20,
    kFilterRawWrite         = 0x40,
    kFilterEndRequest       = 0x80,
    kFilterAny              = 0xFF
} EventFlags;

typedef struct {
    unsigned int    serverFilterVersion;
    unsigned int    appFilterVersion;
    unsigned int    eventFlags;
    unsigned int    initFlags;
    char            filterDesc[kMaxFilterDesc + 1];
} FilterInitData;

typedef struct {
    unsigned int    method;
    char            *URL;
    char            *version;
    char            *userName;
    char            *password;
    unsigned char   *clientCert;
    unsigned int    clientCertLen;
    char            *contentRead;
    unsigned int    contentReadLen;
} FilterRequest;

typedef struct _FilterContext FilterContext;

struct _FilterContext {
    unsigned int    contextSize;
    unsigned int    revision;
    void            *serverContext;
    unsigned int    serverReserved;
    unsigned int    securePort;
    void            *privateContext;

    int (*GetRequest)(FilterContext *context, FilterRequest * request,  unsigned int *errID);
    int (*GetRequestContents)(FilterContext *context, char **contents, unsigned int *errID);
    int (*GetServerVariable)(FilterContext *context, char *name, void *buffer, unsigned int bufferSize, unsigned int *errID);
    int (*WriteClient)(FilterContext *context, char *buffer, unsigned int bufferLen, unsigned int reserved, unsigned int *errID);
    void *(*AllocMem)(FilterContext *context, unsigned int size, unsigned int reserved, unsigned int *errID);
    int (*ServerSupport)(FilterContext *context, unsigned int funcType, void *data1, void *data2, unsigned int other, unsigned int *errID);
};

typedef enum {
    kRequestNone    = 0,
    kRequestHEAD    = 1,
    kRequestGET     = 2,
    kRequestPOST    = 3,
    kRequestPUT     = 4,
    kRequestDELETE  = 5
} RequestMethod;

typedef enum {
    kWriteResponseHeaders = 1
} ServerSupportTypes;

typedef struct {
    unsigned int    responseCode;
    char            *reasonText;
    char            *headerText;
} FilterResponseHeaders;

typedef struct {
    unsigned int    requestMethod;

    int (*GetAllHeaders)(FilterContext *context, char **headers, unsigned int *errID);
    int (*GetHeader)(FilterContext *context, char *name, char *buffer, unsigned int bufferSize, unsigned int *errID);
    int (*SetHeader)(FilterContext *context, char *name, char *value, unsigned int *errID);
    int (*AddHeader)(FilterContext *context, char *header, unsigned int *errID);

    unsigned int    reserved;
} FilterRawRequest;

typedef struct {
    unsigned int    requestMethod;

    int (*GetAllHeaders)(FilterContext *context, char **headers, unsigned int *errID);
    int (*GetHeader)(FilterContext *context, char *name, char *buffer, unsigned int bufferSize, unsigned int *errID);

    unsigned int    reserved;
} FilterParsedRequest;

typedef struct {
    const char      *url;
    char            *pathBuffer;
    unsigned int    bufferSize;
    unsigned int    mapType;
} FilterMapURL;

typedef enum {
    kURLMapUnknown  = 0,
    kURLMapPass     = 1,
    kURLMapExec     = 2,
    kURLMapRedirect = 3,
    kURLMapService  = 4,
    kURLMapDomino   = 5
} FilterULMapTypes;

typedef struct {
    unsigned char   *userName;
    unsigned char   *password;
    unsigned char   *clientCert;
    unsigned int    clientCertLen;
    unsigned int    authFlags;
    unsigned int    preAuthenticated;
    unsigned int    foundInCache;
    unsigned int    authNameSize;
    unsigned char   *authName;
    unsigned int    authType;

    int (*GetUserNameList)(FilterContext *context, unsigned char * buffer, unsigned int bufferSize, unsigned int *numNames, unsigned int reserved, unsigned int *errID);
    int (*GetHeader)(FilterContext *context, char *name, char *buffer, unsigned int bufferSize, unsigned int *errID);
} FilterAuthenticate;

typedef enum {
    kNotAuthentic           = 0,
    kAuthenticBasic         = 1,
    kAuthenticClientCert    = 2
} FilterAuthenticationTypes;

typedef enum {
    kAuthAllowBasic         = 0x01,
    kAuthAllowAnonymous     = 0x02,
    kAuthAllowSSLCert       = 0x04,
    kAuthAllowSSLBasic      = 0x08,
    kAuthAllowSSLAnonymous  = 0x10,
    kAuthRedirectToSSL      = 0x20
} FilterAuthConfigFlags;

typedef struct {
    const unsigned char     *userName;

    int (*GetUserNameList)(FilterContext *context, unsigned char * buffer, unsigned int bufferSize, unsigned int *numNames, unsigned int reserved, unsigned int *errID);
    int (*PopulateUserNameList)(FilterContext *context, unsigned char * buffer, unsigned int bufferSize, unsigned int *numNames, unsigned int reserved, unsigned int *errID);
    int (*AddGroupsToList)(FilterContext *context, unsigned char * groupNames, unsigned int numGroupNames, unsigned int reserved, unsigned int *errID);
    int (*RemoveGroupsFromList)(FilterContext *context, unsigned int reserved, unsigned int *errID);

    unsigned int            reserved;
} FilterUserNameList;

typedef struct {
    unsigned int            responseCode;
    char                    *reasonText;

    int (*GetAllHeaders)(FilterContext *context, char **headers, unsigned int *errID);
    int (*GetHeader)(FilterContext *context, char *name, char *buffer, unsigned int bufferSize, unsigned int *errID);
    int (*SetHeader)(FilterContext *context, char *name, char *value, unsigned int *errID);
    int (*AddHeader)(FilterContext *context, char *header, unsigned int *errID);

    unsigned int            reserved;
} FilterResponse;

typedef struct {
    char            *content;
    unsigned int    contentLen;
    unsigned int    reserved;
} FilterRawWrite;

/* Non DSAPI stuff here for convenience */

#define NOERROR 0

void AddInLogMessageText(char *string, unsigned short err, ...);

#endif  /* __dsapi_filter_h */
