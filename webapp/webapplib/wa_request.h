#ifndef _WA_REQUEST_H_
#define _WA_REQUEST_H_

/**
 * The wa_request structure embodies an HTTP request.
 */
struct wa_request {
    wa_host *host;                  // The host handling the request
    wa_application *application;    // The application that needs to be called
    void *data;                     // The web-server specific callback data
    char *method;                   // The HTTP method (GET, POST)
    char *uri;                      // The HTTP URI requested
    char *arguments;                // The HTTP query arguments
    char *protocol;                 // The HTTP protocol (HTTP/1.0, HTTP/1.1)
    int header_count;               // The number of headers in this request
    char **header_names;            // The array of header names
    char **header_values;           // The array of header values
};

/* Function prototype declaration */
// Handle a request.
const char *wa_request_handle(wa_request *, wa_callbacks *);

#endif // ifdef _WA_HOST_H_
