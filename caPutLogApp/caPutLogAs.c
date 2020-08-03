/*
 *	Author:	V. Korobov
 *	Date:	5-98
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	MKS-2 Group, DESY, Hamburg
 *
 *	This module contains routines to initialize and register
 *	CA Put requests into Message Queue.
 *
 * 	Modification Log:
 * 	-----------------
 *	05-25-98	kor	Added Logging via Message Queue
 *	01-06-00	kor	Added caPutLogInit routine
 *	03-09-00	kor	malloc/free pair replaced with
 *				freeListMalloc/freeListFree
 *	04-10-00	kor	Message Queue requests are replaced
 *				with Ring Buffer requests.
 *	05-04-00	kor	symFindByname call is replaced with
 *				symFindBynameEPICS.
 *	11/02/00	kor	logMsg call are replaced with errlogPrintf
 *	01/09/01	kor	adapted to M.Kraimer's Trap Write Hook.
 *				Added caPutLogStop routine. Reduced
 *				environment variable names to check.
 *	11/12/02	kor	bug fix: when modified field doesn't cause
 *				record processing the timeStamp contain the time
 *				when the record was last time processed but not
 *				the time when the field was changed.
 *				First use DBR_TIME option in dbGetField request,
 *				and compare seconds of time stamp with current
 *				time stamp. If less than current then set current
 *				time for the time stamp in registering structure.
 *	12/12/02	kor	Added caPutLogVersion() routine.
 *	03/13/14	jp	added caPutLogDataCalloc() so non-IOC tasks
 *				like pv gateway can use this module to log
 *				puts.
 */
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <errlog.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <asLib.h>
#include <epicsStdio.h>
#include <freeList.h>
#include <asTrapWrite.h>
#include <epicsVersion.h>

#define BASE_3_14 (EPICS_VERSION * 100 + EPICS_REVISION < 315)

#if !(BASE_3_14)
#include "dbChannel.h"
#endif

#define epicsExportSharedSymbols
#include "caPutLog.h"
#include "caPutLogTask.h"
#include "caPutLogAs.h"

static asTrapWriteId listenerId = 0;

static void *logDataFreeList = 0;

#define FREE_LIST_SIZE 1000

static void caPutLogAs(asTrapWriteMessage * pmessage, int afterPut);
static void (*psendCallback)(LOGDATA *);
static void (*pstopCallback)() = NULL;


int caPutLogAsInit(void (*sendCallback)(LOGDATA *), void (*stopCallback)())
{
    psendCallback = sendCallback;
    pstopCallback = stopCallback;

    if (!asActive) {
        errlogSevPrintf(errlogFatal, "caPutLog: access security is disabled\n");
        return caPutLogError;
    }

    /* Initialize the free list of log elements */
    if (!logDataFreeList) {
        freeListInitPvt(&logDataFreeList, sizeof(LOGDATA), FREE_LIST_SIZE);
    }

    /* Initialize the Trap Write Listener */
    listenerId = asTrapWriteRegisterListener(caPutLogAs);
    if (!listenerId) {
        errlogSevPrintf(errlogFatal, "caPutLog: asTrapWriteRegisterListener failed\n");
        return caPutLogError;
    }
    return caPutLogSuccess;
}

void caPutLogAsStop()
{
    if (pstopCallback != NULL) {
        pstopCallback();
    }

    if (listenerId) {
        asTrapWriteUnregisterListener(listenerId);
        listenerId = NULL;
        errlogPrintf("caPutLog: disabled\n");
    }
}

static void caPutLogAs(asTrapWriteMessage *pmessage, int afterPut)
{
#if BASE_3_14
    dbAddr *paddr = pmessage->serverSpecific;
#else
    struct dbChannel *pchan = pmessage->serverSpecific;
    dbAddr *paddr = &pchan->addr;
    const char *pv_name = pchan->name;
#endif
    LOGDATA *plogData;
    long options, num_elm;
    long status;

    if (!afterPut) {                    /* before put */
        plogData = caPutLogDataCalloc();
        if (plogData == NULL) {
            errlogPrintf("caPutLog: memory allocation failed\n");
            pmessage->userPvt = NULL;
            return;
        }
        pmessage->userPvt = (void *)plogData;

        epicsSnprintf(plogData->userid, MAX_USERID_SIZE, "%s", pmessage->userid);
        epicsSnprintf(plogData->hostid, MAX_HOSTID_SIZE, "%s", pmessage->hostid);
#if BASE_3_14
        dbNameOfPV(paddr, plogData->pv_name, PVNAME_STRINGSZ);
#else
        epicsSnprintf(plogData->pv_name, PVNAME_STRINGSZ, "%s", pv_name);
#endif

        if (VALID_DB_REQ(paddr->field_type)) {
            plogData->type = paddr->field_type;
        } else {
            plogData->type = DBR_STRING;
        }
        /* included for efficient pv-equality test: */
        plogData->pfield = paddr->pfield;

        options = 0;
        num_elm = caPutLogMaxArraySize(plogData->type);
        status = dbGetField(
            paddr, plogData->type, &plogData->old_value, &options, &num_elm, 0);
        plogData->old_log_size = num_elm;
        plogData->old_size = caPutLogActualArraySize(paddr);
        plogData->is_array = paddr->no_elements > 1 ? TRUE : FALSE;

        if (status) {
            errlogPrintf("caPutLog: dbGetField error=%ld\n", status);
            plogData->type = DBR_STRING;
            strcpy(plogData->old_value.v_string, "Not Accessible");
        }
    }
    else {                              /* after put */
        epicsTimeStamp curTime;

        plogData = (LOGDATA *) pmessage->userPvt;

        options = DBR_TIME;
        num_elm = caPutLogMaxArraySize(plogData->type);
        status = dbGetField(
            paddr, plogData->type, &plogData->new_value, &options, &num_elm, 0);
        plogData->new_log_size = num_elm;
        plogData->new_size = caPutLogActualArraySize(paddr);
        plogData->is_array = plogData->is_array || paddr->no_elements > 1 ? TRUE : FALSE;

        if (status) {
            errlogPrintf("caPutLog: dbGetField error=%ld.\n", status);
            plogData->type = DBR_STRING;
            strcpy(plogData->new_value.value.v_string, "Not Accessible");
        }
        epicsTimeGetCurrent(&curTime); /* get current time stamp */
        /* replace, if necessary, the time stamp */
        if (plogData->new_value.time.secPastEpoch < curTime.secPastEpoch) {
            plogData->new_value.time.secPastEpoch = curTime.secPastEpoch;
            plogData->new_value.time.nsec = curTime.nsec;
        }
        psendCallback(plogData);
    }
}

int caPutLogMaxArraySize(short type)
{
#if !JSON_AND_ARRAYS_SUPPORTED
    return 1;
#else
    static int const arraySizeLookUpTable [] = {
        MAX_ARRAY_SIZE_BYTES/MAX_STRING_SIZE,       /* DBR_STRING */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt8),     /* DBR_CHAR */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt8),    /* DBR_UCHAR */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt16),    /* DBR_SHORT */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt16),   /* DBR_USHORT */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt32),    /* DBR_LONG */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt32),   /* DBR_ULONG */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt64),    /* DBR_INT64 */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt64),   /* DBR_UINT64 */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsFloat32),  /* DBR_FLOAT */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsFloat64),  /* DBR_DOUBLE */
        MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt16)    /* DBR_ENUM */
    };

    if (type >= DBR_STRING || type <= DBR_ENUM){
        return arraySizeLookUpTable[type];
    } else {
        errlogSevPrintf(errlogMajor, "caPutLogAs: Array size for type %d can not be determind\n", type);
        return 1;
    }
#endif
}

long caPutLogActualArraySize(dbAddr * paddr)
{
    rset *prset = dbGetRset(paddr);
    long nActual;
    long offset;

    if (paddr->no_elements > 1 &&
             prset->get_array_info) {
        prset->get_array_info(paddr, &nActual, &offset);
    } else {
        nActual = paddr->no_elements;
    }
    return nActual;
}

void caPutLogDataFree(LOGDATA *plogData)
{
    freeListFree(logDataFreeList, plogData);
}

LOGDATA* caPutLogDataCalloc(void)
{
  return freeListCalloc(logDataFreeList);
}
