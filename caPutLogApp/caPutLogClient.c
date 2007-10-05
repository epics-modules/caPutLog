/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* caPutLogClient.c,v 1.25.2.6 2004/10/07 13:37:34 mrk Exp */
/*
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 */

/*
 * ANSI C
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <dbDefs.h>
#include <envDefs.h>
#include <errlog.h>
#include <logClient.h>

#include "caPutLog.h"

#define epicsExportSharedSymbols
#include "caPutLogClient.h"

#ifndef LOCAL
#define LOCAL static
#endif

LOCAL READONLY ENV_PARAM EPICS_CA_PUT_LOG_ADDR = {"EPICS_CA_PUT_LOG_ADDR", ""};

LOCAL logClientId caPutLogClient;

/*
 *  caPutLogClientFlush ()
 */
void epicsShareAPI epicsShareAPI caPutLogClientFlush ()
{
    if (caPutLogClient!=NULL) {
        logClientFlush (caPutLogClient);
    }
}

/*
 *  caPutLogClientShow ()
 */
void epicsShareAPI caPutLogClientShow (unsigned level)
{
    if (caPutLogClient!=NULL) {
        logClientShow (caPutLogClient, level);
    }
}

/*
 *  caPutLogClientInit()
 */
int epicsShareAPI caPutLogClientInit (const char *addr_str)
{
    int status;
    struct sockaddr_in saddr;
    long default_port = 7011;

    if (caPutLogClient!=NULL) {
        return caPutLogSuccess;
    }

    if (!addr_str || !addr_str[0]) {
        addr_str = envGetConfigParamPtr(&EPICS_CA_PUT_LOG_ADDR);
    }

    status = aToIPAddr (addr_str, default_port, &saddr);
    if (status<0) {
        fprintf (stderr, "caPutLog: bad address or host name\n");
        return caPutLogError;
    }

    caPutLogClient = logClientCreate (saddr.sin_addr, ntohs(saddr.sin_port));

    if (!caPutLogClient) {
        return caPutLogError;
    }
    else {
        return caPutLogSuccess;
    }
}

/*
 * caPutLogClientSend ()
 */
void epicsShareAPI caPutLogClientSend (const char *message)
{
    if (caPutLogClient) {
        logClientSend (caPutLogClient, message);
    }
}
