#include <wa.h>

static const char *wa_info_configure(wa_connection *conn, char *param) {
    if(conn==NULL) return("Connection not specified");
    if (param==NULL) conn->conf=strdup("[No data supplied]");
    else conn->conf=strdup(param);
    return(NULL);
}

static char *wa_info_describe(wa_connection *conn) {
    char buf[1024];
    
    if(conn==NULL) return("Null connection specified");
    sprintf(buf, "Extra parameters: %s", (char *)conn->conf);
    return(strdup(buf));
}

void wa_info_init(wa_connection *conn) {
}

void wa_info_destroy(wa_connection *conn) {
}

void wa_info_handle(wa_request *req, wa_callbacks *cb) {
    (*cb->setstatus)(req->data,200);
    (*cb->settype)(req->data,"text/html");
    (*cb->setheader)(req->data,"The-Freaks","come/out");
    (*cb->commit)(req->data);
    (*cb->write)(req->data,"<HTML><BODY>TEST</BODY></HTML>",30);
    (*cb->flush)(req->data);
}

wa_provider wa_provider_info = {
    "info",
    wa_info_configure,
    wa_info_init,
    wa_info_destroy,
    wa_info_describe,
    wa_info_handle,
};
