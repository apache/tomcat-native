/*--------------------------------------------------------------------
 *
 *      File:      dsapifilter.h
 *
 *      Copyright (c)1999 Iris Associates
 *
 *-------------------------------------------------------------------*/

#if !defined(DSAPIFILTER_H)
#define DSAPIFILTER_H

#ifdef __cplusplus
extern "C"
{
#endif

/*---
 *      Types and Defines
 */

#define kInterfaceVersion   2
#define kMaxFilterDesc   255

    typedef unsigned char LMBCS;

    typedef enum
    {
        kFilterNotHandled = 0,
        kFilterHandledRequest = 1,
        kFilterHandledEvent = 2,
        kFilterError = 3
    }
    FilterReturnCode;

/*--
 *      Filter interface
 */

/*---
*      events to register for
*/
    typedef enum
    {
        kFilterRawRequest = 0x01,
        kFilterParsedRequest = 0x02,
        kFilterAuthUser = 0x04,
        kFilterUserNameList = 0x08,
        kFilterMapURL = 0x10,
        kFilterResponse = 0x20,
        kFilterRawWrite = 0x40,
        kFilterEndRequest = 0x80,
        kFilterAny = 0xFF
    }
    EventFlags;

/*---
 *      filter initialization data
 */
    typedef struct
    {
        unsigned int serverFilterVersion;
        unsigned int appFilterVersion;
        unsigned int eventFlags;
        unsigned int initFlags;
        char filterDesc[kMaxFilterDesc + 1];
    }
    FilterInitData;

/*---
 *      request line structure
 */
    typedef struct
    {
        unsigned int method;
        char *URL;
        char *version;
        char *userName;
        char *password;
        unsigned char *clientCert;
        unsigned int clientCertLen;
        char *contentRead;
        unsigned int contentReadLen;
    }
    FilterRequest;

/*---
 *      filter context data included in every call to filter
 */
    typedef struct _FilterContext
    {
        unsigned int contextSize;
        unsigned int revision;
        void *serverContext;
        unsigned int serverReserved;
        unsigned int securePort;
        void *privateContext;

        int (*GetRequest) (struct _FilterContext * context,
                           FilterRequest * request, unsigned int *errID);

        int (*GetRequestContents) (struct _FilterContext * context,
                                   char **contents, unsigned int *errID);

        int (*GetServerVariable) (struct _FilterContext * context, char *name,
                                  void *buffer, unsigned int bufferSize,
                                  unsigned int *errID);

        int (*WriteClient) (struct _FilterContext * context, char *buffer,
                            unsigned int bufferLen, unsigned int reserved,
                            unsigned int *errID);

        void *(*AllocMem) (struct _FilterContext * context, unsigned int size,
                           unsigned int reserved, unsigned int *errID);

        int (*ServerSupport) (struct _FilterContext * context,
                              unsigned int funcType, void *data1, void *data2,
                              unsigned int other, unsigned int *errID);
    }
    FilterContext;

    typedef unsigned int (*FilterInitFuncType) (FilterInitData * initData);
    typedef unsigned int (*FilterEventFuncType) (FilterContext * context,
                                                 unsigned int eventType,
                                                 void *eventData,
                                                 unsigned int *errID);
    typedef unsigned int (*FilterTermFuncType) (unsigned int);

/*---
 *      request methods
 */
    typedef enum
    {
        kRequestNone = 0,
        kRequestHEAD = 1,
        kRequestGET = 2,
        kRequestPOST = 3,
        kRequestPUT = 4,
        kRequestDELETE = 5
    }
    RequestMethod;

/*---
 *      server support function types
 */
    typedef enum
    {
        kWriteResponseHeaders = 1
    }
    ServerSupportTypes;

/*---
 *      'data1' for server support function 'kWriteResponseHeaders'
 */
    typedef struct
    {
        unsigned int responseCode;
        char *reasonText;
        char *headerText;
    }
    FilterResponseHeaders;

/*---
 *      raw request (headers not processed yet)
 */
    typedef struct
    {
        unsigned int requestMethod;

        int (*GetAllHeaders) (FilterContext * context, char **headers,
                              unsigned int *errID);

        int (*GetHeader) (FilterContext * context, char *name, char *buffer,
                          unsigned int bufferSize, unsigned int *errID);

        int (*SetHeader) (FilterContext * context, char *name, char *value,
                          unsigned int *errID);

        int (*AddHeader) (FilterContext * context, char *header,
                          unsigned int *errID);

        unsigned int reserved;
    }
    FilterRawRequest;

/*---
 *      parsed request
 */
    typedef struct
    {
        unsigned int requestMethod;

        int (*GetAllHeaders) (FilterContext * context, char **headers,
                              unsigned int *errID);

        int (*GetHeader) (FilterContext * context, char *name, char *buffer,
                          unsigned int bufferSize, unsigned int *errID);

        unsigned int reserved;
    }
    FilterParsedRequest;

/*---
 *      URL map
 */
    typedef struct
    {
        const char *url;
        char *pathBuffer;
        unsigned int bufferSize;
        unsigned int mapType;
    }
    FilterMapURL;

/*---
 *      URL map types
 */
    typedef enum
    {
        kURLMapUnknown = 0,
        kURLMapPass = 1,
        kURLMapExec = 2,
        kURLMapRedirect = 3,
        kURLMapService = 4,
        kURLMapDomino = 5
    }
    FilterULMapTypes;

/*---
 *      user authentication
 */
    typedef struct
    {
        LMBCS *userName;
        LMBCS *password;
        unsigned char *clientCert;
        unsigned int clientCertLen;
        unsigned int authFlags;
        unsigned int preAuthenticated;
        unsigned int foundInCache;
        unsigned int authNameSize;
        LMBCS *authName;
        unsigned int authType;

        int (*GetUserNameList) (FilterContext * context, LMBCS * buffer,
                                unsigned int bufferSize,
                                unsigned int *numNames, unsigned int reserved,
                                unsigned int *errID);

        int (*GetHeader) (FilterContext * context, char *name, char *buffer,
                          unsigned int bufferSize, unsigned int *errID);
    }
    FilterAuthenticate;

/*---
 *      user authentication types
 */
    typedef enum
    {
        kNotAuthentic = 0,
        kAuthenticBasic = 1,
        kAuthenticClientCert = 2
    }
    FilterAuthenticationTypes;

/*---
 *      authentication configuration flags
 */
    typedef enum
    {
        kAuthAllowBasic = 1,
        kAuthAllowAnonymous = 2,
        kAuthAllowSSLCert = 4,
        kAuthAllowSSLBasic = 8,
        kAuthAllowSSLAnonymous = 16,
        kAuthRedirectToSSL = 32
    }
    FilterAuthConfigFlags;

/*---
 *      user name list
 */
    typedef struct
    {
        const LMBCS *userName;

        int (*GetUserNameList) (FilterContext * context, LMBCS * buffer,
                                unsigned int bufferSize,
                                unsigned int *numNames, unsigned int reserved,
                                unsigned int *errID);

        int (*PopulateUserNameList) (FilterContext * context, LMBCS * buffer,
                                     unsigned int bufferSize,
                                     unsigned int *numNames,
                                     unsigned int reserved,
                                     unsigned int *errID);

        int (*AddGroupsToList) (FilterContext * context, LMBCS * groupNames,
                                unsigned int numGroupNames,
                                unsigned int reserved, unsigned int *errID);

        int (*RemoveGroupsFromList) (FilterContext * context,
                                     unsigned int reserved,
                                     unsigned int *errID);

        unsigned int reserved;
    }
    FilterUserNameList;

/*---
 *      request response
 */
    typedef struct
    {
        unsigned int responseCode;
        char *reasonText;

        int (*GetAllHeaders) (FilterContext * context, char **headers,
                              unsigned int *errID);

        int (*GetHeader) (FilterContext * context, char *name, char *buffer,
                          unsigned int bufferSize, unsigned int *errID);

        int (*SetHeader) (FilterContext * context, char *name, char *value,
                          unsigned int *errID);

        int (*AddHeader) (FilterContext * context, char *header,
                          unsigned int *errID);

        unsigned int reserved;
    }
    FilterResponse;

/*---
 *      write content
 */
    typedef struct
    {
        char *content;
        unsigned int contentLen;
        unsigned int reserved;
    }
    FilterRawWrite;

#ifdef __cplusplus
}
#endif

#endif                          /* DSAPIFILTER_H */
