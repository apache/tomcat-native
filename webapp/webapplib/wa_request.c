#include <wa.h>

/* Function prototype declaration */
// Handle a request.
const char *wa_request_handle(wa_request *req, wa_callbacks *cb) {
    if (cb==NULL) return("Callback structure pointer is NULL");
    if (req==NULL) return("Request structure pointer is NULL");
    if (req->host==NULL) return("Web application host is NULL");
    if (req->application==NULL) return("Web application structure is NULL");
    if (req->method==NULL) return("Request method is NULL");
    if (req->uri==NULL) return("Request URI is NULL ");
    if (req->protocol==NULL) return("Request protocol is NULL");

    (*req->application->conn->prov->handle)(req,cb);
    return(NULL);
}    
