//static jk_worker_env_t worker_env;
#define _WIN32_WINNT 0x0400

#include <httpext.h>
#include <httpfilt.h>
#include <wininet.h>

#include "jk_global.h"
//#include "jk_util.h"
#include "jk_pool.h"

struct isapi_private_data {
    jk_pool_t p;
    
    int request_started;
    unsigned bytes_read_so_far;
    LPEXTENSION_CONTROL_BLOCK  lpEcb;
};
typedef struct isapi_private_data isapi_private_data_t;



static int JK_METHOD start_response(jk_env_t *env, jk_ws_service_t *s );

static int JK_METHOD read(jk_env_t *env, jk_ws_service_t *s,
                          void *b, unsigned len,
                          unsigned *actually_read);

static int JK_METHOD write(jk_env_t *env,jk_ws_service_t *s,
                           const void *b,
                           unsigned l);
