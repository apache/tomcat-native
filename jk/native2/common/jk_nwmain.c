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

/***************************************************************************
 * Description: Netware Wrapper                                            *
 * Author:      Mike Anderson <mmander@novell.com>                         *
 * Version:     $Revision$                                           *
 ***************************************************************************/

#ifdef NETWARE
/*
 * NATIVE_MAIN
 */

/*
 * INCLUDES
 */

#include <stdio.h>

/* Apache 2/APR uses NOVELL_LIBC which has a different way of handling
 * "library" nlms.  If we aren't on LIBC, use the old method
 */

#ifndef __NOVELL_LIBC__
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
void main()
{
    ExitThread(TSR_THREAD, 0);
}
#else /* __NOVELL_LIBC__ */

/* Since we are on LibC, we need to handle our own startup and shutdown */

#include <netware.h>
#include "novsock2.h"

int _NonAppStart
    (void *NLMHandle,
     void *errorScreen,
     const char *cmdLine,
     const char *loadDirPath,
     size_t uninitializedDataLength,
     void *NLMFileHandle,
     int (*readRoutineP) (int conn, void *fileHandle, size_t offset,
                          size_t nbytes, size_t * bytesRead, void *buffer),
     size_t customDataOffset,
     size_t customDataSize, int messageCount, const char **messages)
{
#pragma unused(cmdLine)
#pragma unused(loadDirPath)
#pragma unused(uninitializedDataLength)
#pragma unused(NLMFileHandle)
#pragma unused(readRoutineP)
#pragma unused(customDataOffset)
#pragma unused(customDataSize)
#pragma unused(messageCount)
#pragma unused(messages)

    WSADATA wsaData;

    return WSAStartup((WORD) MAKEWORD(2, 0), &wsaData);
}

void _NonAppStop(void)
{
    WSACleanup();
}

int _NonAppCheckUnload(void)
{
    return 0;
}
#endif /* __NOVELL_LIBC__ */

#endif
