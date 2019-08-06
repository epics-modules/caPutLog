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
#include <epicsString.h>
#include <epicsStdio.h>

#define epicsExportSharedSymbols
#include "caPutLog.h"
#include "caPutLogClient.h"

#ifndef LOCAL
#define LOCAL static
#endif

#ifdef _WIN32
#define strtok_r strtok_s
#endif

LOCAL READONLY ENV_PARAM EPICS_CA_PUT_LOG_ADDR = {"EPICS_CA_PUT_LOG_ADDR", ""};

LOCAL struct clientlist {
    logClientId caPutLogClient;
    struct clientlist *next;
    char addr[1];
} *caPutLogClients = NULL;

/*
 *  caPutLogClientFlush ()
 */
void caPutLogClientFlush ()
{
    struct clientlist* c;
    for (c = caPutLogClients; c; c = c->next) {
        logClientFlush (c->caPutLogClient);
    }
}

/*
 *  caPutLogClientShow ()
 */
void caPutLogClientShow (unsigned level)
{
    struct clientlist* c;
    for (c = caPutLogClients; c; c = c->next) {
        logClientShow (c->caPutLogClient, level);
    }
}

/*
 *  caPutLogClientInit()
 */
int caPutLogClientInit (const char *addr_str)
{
    int status;
    struct sockaddr_in saddr;
    unsigned short default_port = 7011;
    struct clientlist** pclient;
    char *clientaddr;
    char *saveptr;
    char *addr_str_copy1;
    char *addr_str_copy2;

    if (!addr_str || !addr_str[0]) {
        if (caPutLogClients) return caPutLogSuccess;
        addr_str = envGetConfigParamPtr(&EPICS_CA_PUT_LOG_ADDR);
    }
    if (addr_str == NULL) {
        errlogSevPrintf(errlogMajor, "caPutLog: server address not specified\n");
        return caPutLogError;
    }

    addr_str_copy2 = addr_str_copy1 = epicsStrDup(addr_str);

    while(1) {
        clientaddr = strtok_r(addr_str_copy1, " \t\n\r", &saveptr);
        if (!clientaddr) break;
        addr_str_copy1 = NULL;

        for (pclient = &caPutLogClients; *pclient; pclient = &(*pclient)->next) {
            if (strcmp(clientaddr, (*pclient)->addr) == 0) {
                fprintf (stderr, "caPutLog: address %s already configured\n", clientaddr);
                break;
            }
        }
        if (*pclient) continue;

        status = aToIPAddr (clientaddr, default_port, &saddr);
        if (status<0) {
            fprintf (stderr, "caPutLog: bad address or host name %s\n", clientaddr);
            continue;
        }

        *pclient = malloc(sizeof(struct clientlist)+strlen(clientaddr));
        if (!*pclient) {
            fprintf (stderr, "caPutLog: out of memory\n");
            return caPutLogError;
        }
        strcpy((*pclient)->addr, clientaddr);

        (*pclient)->caPutLogClient = logClientCreate (saddr.sin_addr, ntohs(saddr.sin_port));
        if (!(*pclient)->caPutLogClient) {
            fprintf (stderr, "caPutLog: cannot create logClient %s\n", clientaddr);
            free(*pclient);
            *pclient = NULL;
            continue;
        }

        (*pclient)->next = NULL;
    }
    free(addr_str_copy2);
    return caPutLogClients ? caPutLogSuccess : caPutLogError;
}

/*
 * caPutLogClientSend ()
 */
void caPutLogClientSend (const char *message)
{
    struct clientlist* c;
    for (c = caPutLogClients; c; c = c->next) {
        logClientSend (c->caPutLogClient, message);
    }
}
