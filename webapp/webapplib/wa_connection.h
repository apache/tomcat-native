#ifndef _WA_CONNECTION_H_
#define _WA_CONNECTION_H_

/**
 * The wa_connection structure represents a connection to a webapp server.
 */
struct wa_connection {
    char *name;             // The connection unique name
    wa_provider *prov;      // The connection provider
    void *conf;             // Provider specific configurations
    wa_connection *next;    // The next configured provider
};

/* The list of configured connections */
extern wa_connection *wa_connections;

/* Function prototype declaration */
// Create a new connection.
const char *wa_connection_create(char *, char *, char *);
// Get a specific webapp connection.
wa_connection *wa_connection_get(char *);
// Initialize all configured connections.
void wa_connection_init(void);
// Initialize all configured connections.
void wa_connection_destroy(void);

#endif // ifdef _WA_CONNECTION_H_
