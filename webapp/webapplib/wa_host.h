#ifndef _WA_HOST_H_
#define _WA_HOST_H_

/**
 * The wa_application structure contains all required configuration data for
 * web applications.
 */
struct wa_application {
    char *name;             // The name of this web application
    char *path;             // The web application root URI path
    wa_connection *conn;    // Pointer to the appropriate connection
    wa_application *next;   // Pointer to the next web application
};

/**
 * The wa_host structure represents a configured host.
 */
struct wa_host {
    char *name;             // The main name of this host
    int port;               // The main port server
    wa_application *apps;   // The list of configured web applications
    wa_host *next;          // Pointer to the next configured host
};

/* The list of configured hosts */
extern wa_host *wa_hosts;

/* Function prototype declaration */
// Create configuration for a new host.
const char *wa_host_create(char *, int);
// Get the host configuration.
wa_host *wa_host_get(char *, int);
// Configure a web application for a specific host.
const char *wa_host_setapp(wa_host *, char *, char *, wa_connection *);
// Configure a web application for a specific host.
const char *wa_host_setapp_byname(char *, int, char *, char *, wa_connection *);
// Retrieve a web application for a specific host.
wa_application *wa_host_findapp(wa_host *, char *);
// Retrieve a web application for a specific host.
wa_application *wa_host_findapp_byname(char *, int , char *);

#endif // ifdef _WA_HOST_H_
