#ifdef NETWARE
/*
 * NATIVE_MAIN
 */

/*
 * INCLUDES
 */

#include <stdio.h>
#include <nwthread.h>
#include <netdb.h>

NETDB_DEFINE_CONTEXT

/*
 * main ()
 *
 * Main entry point -- don't do much more than I've provided
 *
 * Entry:
 *
 * Exit:
 *    Nothing
 */

void main ()
{
   ExitThread (TSR_THREAD, 0);
}
#endif
