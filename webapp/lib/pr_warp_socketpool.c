/*
 *  Copyright 1999-2004 The Apache Software Foundation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#include "pr_warp.h"



/* Define the maximum number of idle sockets to leave aside for a connection */

#if APR_HAS_THREADS
#define MAX_SOCKET_POOL_SIZE 25
#else
#define MAX_SOCKET_POOL_SIZE 1
#endif


/* Create a warp socket pool.  A warp socket pool is a pool of connected, free
   sockets for a given warp connection. */
warp_socket_pool * warp_sockpool_create()
{
    int idx;

    warp_socket_pool * pool =
        (warp_socket_pool*)apr_palloc(wa_pool,sizeof(warp_socket_pool));

    /* Start the pool with no available sockets. */
    pool->available_socket_list = NULL;
    pool->available_socket_list_size = 0;

    /* Create mutex to provide thread-safe access to socket pool. */
#if APR_HAS_THREADS
    if (apr_thread_mutex_create(&pool->pool_mutex,
                                APR_THREAD_MUTEX_DEFAULT, 
                                wa_pool) !=APR_SUCCESS)
        return 0;
#endif

    /* Set up array of wa_chain elements to be used later for elements in the
       available socket list.  By allocating them up front we avoid the use
       of malloc and free during normal operation. */
    pool->available_elem_blocks = 
        (wa_chain*)apr_palloc(wa_pool,sizeof(wa_chain)*MAX_SOCKET_POOL_SIZE);

    /* From the array create a linked-list. */
    for (idx = 0; idx < MAX_SOCKET_POOL_SIZE - 1; idx++) {
        pool->available_elem_blocks[idx].next = 
            & pool->available_elem_blocks[idx+1];
    }
    pool->available_elem_blocks[idx-1].next = NULL;

    return pool;
}


/* Destroy the socket pool specified. */
void warp_sockpool_destroy(warp_socket_pool * pool)
{
    apr_status_t ret = APR_SUCCESS;
    apr_socket_t * sock = NULL;

    wa_chain *elem = pool->available_socket_list;

    /* Destroy mutex associated with the pool */
#if APR_HAS_THREADS
    ret = apr_thread_mutex_destroy(pool->pool_mutex);
    pool->pool_mutex = NULL;
#endif

    if (ret!=APR_SUCCESS)
        wa_log(WA_MARK,"Cannot destroy socket pool mutex");

    /* Destroy all the sockets in the pool */
    while (elem != NULL) {
        sock = (apr_socket_t*) elem->curr;

        if (sock != NULL) {
            ret = apr_shutdown(sock, APR_SHUTDOWN_READWRITE);
            ret = apr_socket_close(sock);
        }

        elem->curr = NULL;
        elem = elem->next;
    }

    pool->available_socket_list_size = 0;
    pool->available_socket_list = NULL;
}



/* Acquire a socket from the socket pool specified.  If there
   are no available sockets in the pool then NULL is returned. */
apr_socket_t * warp_sockpool_acquire(warp_socket_pool * pool)
{
    apr_socket_t * sock = NULL;
    wa_chain * first_elem = 0;

    if (pool->available_socket_list_size > 0)
    {
        /* Preliminary check of available_socket_list_size suggests
           that there is an available socket in the pool.  Lock the
           pool and then check again. */

#if APR_HAS_THREADS 
        if (apr_thread_mutex_lock(pool->pool_mutex) != APR_SUCCESS)
            return NULL;
#endif

        if (pool->available_socket_list_size > 0)
        {
            /* There is an available socket in the pool.  Get
               one from the pool. */
            first_elem = pool->available_socket_list;

            /* Update the available socket list. */
            pool->available_socket_list_size--;
            pool->available_socket_list = first_elem->next;

            /* Extract the socket from first_elem. */
            sock = (apr_socket_t*) first_elem->curr;

            /* Put the now unused first_elem block in the
               available elem list so it can be later reused. */
            first_elem->curr = 0;
            first_elem->next = pool->available_elem_blocks;
            pool->available_elem_blocks = first_elem;
        }

#if APR_HAS_THREADS 
        apr_thread_mutex_unlock(pool->pool_mutex);
#endif
    }

    return sock;
}



/* Returns the socket specified to socket pool.  If the pool already contains
   the maximum allowable number of sockets for a pool (MAX_SOCKET_POOL_SIZE)
   then the socket is simply disconnected. */
void warp_sockpool_release(warp_socket_pool * pool, 
                           wa_connection *conn, 
                           apr_socket_t * sock)
{
    wa_chain * new_elem = NULL;

#if APR_HAS_THREADS 
    if (apr_thread_mutex_lock(pool->pool_mutex) != APR_SUCCESS)
        return;
#endif

    /* If we already have the maximum number of available sockets in
       the pool then just disconnect the socket and return */
    if (pool->available_socket_list_size==MAX_SOCKET_POOL_SIZE) {
#if APR_HAS_THREADS
        apr_thread_mutex_unlock(pool->pool_mutex);
#endif
        n_disconnect(conn, sock);
        return;
    }

    /* Add the socket given to the pool...*/
    pool->available_socket_list_size++;

    /* Get a new pool element from the available list. */
    new_elem = pool->available_elem_blocks;
    pool->available_elem_blocks = new_elem->next;
    
    /* Initialise the new pool element. */
    new_elem->curr = sock;
    new_elem->next = pool->available_socket_list;

    /* Add the element to the head of the available
       socket list. */
    pool->available_socket_list = new_elem;

#if APR_HAS_THREADS 
    apr_thread_mutex_unlock(pool->pool_mutex);
#endif
}
