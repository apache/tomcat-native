#include <wa.h>

/**
 * Retrieve a provider.
 *
 * @param name The provider name.
 * @return The wa_provider structure or NULL.
 */
wa_provider *wa_provider_get(char *name) {
    int x=0;
    
    if (name==NULL) return(NULL);
    
    while(TRUE) {
        if (wa_providers[x]==NULL) return(NULL);
        if (strcasecmp(wa_providers[x]->name,name)==0)
            return(wa_providers[x]);
        x++;
    }
}

/* The list of all compiled in providers */
wa_provider *wa_providers[] = {
    &wa_provider_info,
    NULL,
};

