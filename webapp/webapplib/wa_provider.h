#ifndef _WA_PROVIDER_H_
#define _WA_PROVIDER_H_

/**
 * The wa_provider structure describes a connection provider.
 */
struct wa_provider {
    // The name of the current provider
    const char *name;
    // Configure a connection
    const char *(*configure) (wa_connection *, char *);
    // Initialize a connection
    void (*init) (wa_connection *);
    // Clean up a connection
    void (*destroy) (wa_connection *);
    // Get a descriptive string on a connection (based on wa_connection->conf)
    char *(*describe) (wa_connection *);
    // Handle an HTTP request
    void (*handle) (wa_request *, wa_callbacks *);
};

/* The list of all compiled in providers */
extern wa_provider *wa_providers[];

/* Pointers to the different providers */
extern wa_provider wa_provider_info;

/* Function prototype declaration */
// Retrieve a provider.
wa_provider *wa_provider_get(char *);

#endif // ifndef _WA_PROVIDER_H_