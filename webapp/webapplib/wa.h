#ifndef _WA_H_
#define _WA_H_

/* Generic includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Define TRUE and FALSE */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Define the log mark */
#define WA_LOG __FILE__,__LINE__

/* Types definitions */
typedef int boolean;
typedef struct wa_connection wa_connection;
typedef struct wa_application wa_application;
typedef struct wa_host wa_host;
typedef struct wa_provider wa_provider;
typedef struct wa_header wa_header;
typedef struct wa_request wa_request;
typedef struct wa_callbacks wa_callbacks;

/* Other includes from the webapp lib */
#include <wa_host.h>
#include <wa_connection.h>
#include <wa_provider.h>
#include <wa_request.h>

/**
 * The wa_callbacks structure contains function pointers for callbacks to the
 * web server.
 */
struct wa_callbacks {
    /**
     * Log data on the web server log file.
     *
     * @param file The source file of this log entry.
     * @param line The line number within the source file of this log entry.
     * @param data The web-server specific data (wa_request->data).
     * @param fmt The format string (printf style).
     * @param ... All other parameters (if any) depending on the format.
     */
    void (*log)(void *data, const char *file, int line, const char *fmt, ...);

    /**
     * Allocate memory while processing a request.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param size The size in bytes of the memory to allocate.
     * @return A pointer to the allocated memory or NULL.
     */
    void *(*alloc)(void *data, int size);

    /**
     * Read part of the request content.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param buf The buffer that will hold the data.
     * @param len The buffer length.
     * @return The number of bytes read, 0 on end of file or -1 on error.
     */
    int (*read)(void *data, char *buf, int len);

    /**
     * Set the HTTP response status code.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param status The HTTP status code for the response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setstatus)(void *data, int status);

    /**
     * Set the HTTP response mime content type.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param type The mime content type of the HTTP response.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*settype)(void *data, char *type);

    /**
     * Set an HTTP mime header.
     *
     * @param data The web-server specific data (wa_request->data).
     * @param name The mime header name.
     * @param value The mime header value.
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*setheader)(void *data, char *name, char *value);

    /**
     * Commit the first part of the response (status and headers).
     *
     * @param data The web-server specific data (wa_request->data).
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*commit)(void *data);

    /**
     * Write part of the response data back to the client.
     *
     * @param buf The buffer holding the data to be written.
     * @param len The number of characters to be written.
     * @return The number of characters written to the client or -1 on error.
     */
    int (*write)(void *data, char *buf, int len);

    /**
     * Flush any unwritten response data to the client.
     *
     * @param data The web-server specific data (wa_request->data).
     * @return TRUE on success, FALSE otherwise
     */
    boolean (*flush)(void *);
};

#endif // ifdef _WA_H_
