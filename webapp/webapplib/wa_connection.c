#include <wa.h>

/* The list of all available connections */
wa_connection *wa_connections=NULL;

/**
 * Create a new connection.
 *
 * @param name The connection unique name.
 * @param prov The provider name (must be compiled in).
 * @param parm An extra configuration parameter (might be null).
 * @return NULL or a string describing te error message.
 */
const char *wa_connection_create(char *name, char *prov, char *parm) {
    const char *mesg=NULL;
    wa_connection *conn=NULL;
    wa_connection *curr=NULL;
    wa_provider *p=NULL;

    // Check basic parameters
    if (name==NULL) return("Connection name not specified");
    if (prov==NULL) return("Connection provider not specified");
    
    // Try to retrieve the provider by name
    if ((p=wa_provider_get(prov))==NULL) return("Provider not found");
    
    // Allocate connection structure and set basic values
    conn=(wa_connection *)malloc(sizeof(struct wa_connection));
    conn->name=strdup(name);
    conn->prov=p;
    conn->next=NULL;

    // Call the provider specific configuration function
    mesg=((*p->configure)(conn,parm));
    if (mesg!=NULL) return(mesg);

    // Store this connection in our configurations
    if (wa_connections==NULL) {
        wa_connections=conn;
        return(NULL);
    }

    // Iterate thru the list of connections
    curr=wa_connections;
    while(curr!=NULL) {
        if (strcasecmp(curr->name,name)==0)
            return("Duplicate connection name");
        if (curr->next==NULL) {
            curr->next=conn;
            return(NULL);
        }
        curr=curr->next;
    }

    // Why the hack are we here?
    return("Unknown error trying to create connection");
}

/**
 * Get a specific webapp connection.
 *
 * @param name The connection name.
 * @return The wa_connection associated with the name or NULL.
 */
wa_connection *wa_connection_get(char *name) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        if (strcasecmp(curr->name,name)==0) return(curr);
        else curr=curr->next;
    }
    
    // No host found, sorry!
    return(NULL);
}

/**
 * Initialize all configured connections.
 */
void wa_connection_init(void) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        (*curr->prov->init)(curr);
        curr=curr->next;
    }
}

/**
 * Initialize all configured connections.
 */
void wa_connection_destroy(void) {
    wa_connection *curr=wa_connections;

    // Iterate thru our hosts chain
    while(curr!=NULL) {
        (*curr->prov->destroy)(curr);
        curr=curr->next;
    }
}

