/*	File:	  caPutLogTask.c
 *	Author:   V.korobov
 *	Created:  25.05.98
 *
 *	Contains codes for a task which waits for messages
 *	on specified Ring Buffer, sorts messages to avoid 
 *	multiple logging for subsequent messages with the same
 *	record name, forms logging message and logs it via iocCAPutLogPrintf.
 *
 *	Modification log:
 *	----------------
 *	v 2.0
 *	12/11/05	bfr	Port to R3.14 osi layer.
 *				General cleanup. Greatly boosted efficiency.
 *
 *	v 1.0
 *	12/11/98	kor	because of string length restriction for DBF_STRING
 *				dbPutField copies not a string, but
 *				an array of characters. Appropriate modification
 *				for rsl-record is done.
 *	03/15/99	kor	stack size is increased up to 5000
 *
 *	v 1.1
 *	04/19/00	kor	requests to Message Queue are replaced
 *				with requests to Ring Buffer.
 *	05/04/00	kor	symFindByname call is replaced with
 *				symFindBynameEPICS.
 *	11/02/00	kor	logMsg calls are replaced with errlogPrintf
 *	11/17/00	kor	place first "new value" and then "old value"
 *				in log message.
 *	v 1.2
 *	01/03/01	kor	value in the Ring is passed as a value
 *				for DBF_STRING < type <= DBF_ENUM. Other types are
 *				passed as a string.
 *				Optionally logging is done either for all puts
 *				or for puts changing value.
 *	12/12/02	kor	Added RngLogTaskVersio request
 *	
 *	03/24/14	jp	filled in val_dump()
 */

#include <stdlib.h>
#include <stddef.h>
#include <epicsStdio.h>
#include <string.h>

#include <epicsMessageQueue.h>
#include <epicsThread.h>

#include <dbFldTypes.h>
#include <epicsTypes.h>
#include <tsDefs.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <errlog.h>
#include <asLib.h>
#include <epicsAssert.h>

#include "caPutLog.h"
#include "caPutLogAs.h"
#include "caPutLogClient.h"

#define epicsExportSharedSymbols
#include "caPutLogTask.h"

#ifdef NO
#undef NO
#endif
#define NO 0

#ifdef YES
#undef YES
#endif
#define YES 1

#ifndef max
#define max(x, y)       (((x) < (y)) ? (y) : (x))
#endif

#ifndef min
#define min(x, y)       (((x) < (y)) ? (x) : (y))
#endif

#define MAX_BUF_SIZE    120     /* Length of log string */

static void caPutLogTask(void *arg);
static void log_msg(const VALUE *pold_value, const LOGDATA *pLogData,
    int burst, const VALUE *pmin, const VALUE *pmax, int config);
static int  val_to_string(char *pbuf, size_t buflen, const VALUE *pval, short type);
static void val_min(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);
static void val_max(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);
static int  val_equal(const VALUE *pa, const VALUE *pb, short type);
static void val_assign(VALUE *dst, const VALUE *src, short type);
#if 0
static void val_dump(LOGDATA *pdata);
#endif

static int shut_down = FALSE;           /* Shut down flag */
static DBADDR caPutLogPV;               /* Structure to keep address of Log PV */
static DBADDR *pcaPutLogPV;             /* Pointer to PV address structure, 
                                           also used as a flag whether this
                                           PV is defined or not */
static epicsMessageQueueId caPutLogQ;   /* Mailbox for caPutLogTask */

#define MAX_MSGS 1000                   /* The length of queue (in messages) */
#define MSG_SIZE sizeof(LOGDATA*)       /* We store only pointers */

#define isDbrNumeric(type) ((type) > DBR_STRING && (type) <= DBR_ENUM)

/* Start Rng Log Task */
int caPutLogTaskStart(int config)
{
    epicsThreadId threadId;
    char *caPutLogPVEnv;

    if (!asActive) {
        errlogSevPrintf(errlogMajor, "caPutLog: access security disabled, exiting now\n");
        return caPutLogError;
    }

    if (!caPutLogQ) {
        caPutLogQ = epicsMessageQueueCreate(MAX_MSGS, MSG_SIZE);
    }
    if (!caPutLogQ) {
        errlogSevPrintf(errlogFatal, "caPutLog: message queue creation failed\n");
        return caPutLogError;
    }

    caPutLogPVEnv = getenv("EPICS_AS_PUT_LOG_PV"); /* Search for variable */

    if (!caPutLogPVEnv || !caPutLogPVEnv[0]) {
        pcaPutLogPV = NULL;     /* If no -- clear pointer */
#if 0
        errlogSevPrintf(errlogMinor,
            "caPutLog: EPICS_AS_PUT_LOG_PV variable not defined. CA Put Logging to PV is disabled\n");
#endif
    }
    else {
        long getpv_st;

        pcaPutLogPV = &caPutLogPV;
        getpv_st = dbNameToAddr(caPutLogPVEnv/* fullPVname */, pcaPutLogPV);
        if (getpv_st) {
            pcaPutLogPV = NULL; /* If not OK -- clear pointer */
            errlogSevPrintf(errlogMajor,
                "caPutLog: PV for CA Put Logging not found, logging to PV disabled\n");
        }
    }

    if (epicsThreadGetId("caPutLog")) {
        errlogSevPrintf(errlogInfo, "caPutLog: task already running\n");
        return caPutLogSuccess;
    }
    shut_down = FALSE;
    threadId = epicsThreadCreate("caPutLog", epicsThreadPriorityLow,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        caPutLogTask, (void*)config);
    if (!threadId) {
        errlogSevPrintf(errlogFatal,"caPutLog: thread creation failed\n");
        return caPutLogError;
    }
    return caPutLogSuccess;
}

void caPutLogTaskStop(void)
{
    shut_down = TRUE;
}

void caPutLogTaskSend(LOGDATA *plogData)
{
    if (!caPutLogQ) {
        return;
    }
    if (epicsMessageQueueTrySend(caPutLogQ, &plogData, MSG_SIZE)) {
        errlogSevPrintf(errlogMinor, "caPutLog: message queue overflow\n");
    }
}

static void caPutLogTask(void *arg)
{
    int sent = FALSE, burst = FALSE;
    int config = (int)arg;
    LOGDATA *pcurrent, *pnext;
    VALUE old_value, max_value, min_value;
    VALUE *pold=&old_value, *pmax=&max_value, *pmin=&min_value;

    /* Receive 1st message */
    epicsMessageQueueReceive(caPutLogQ, &pcurrent, MSG_SIZE);

#if 0
    printf("caPutLog: received a message\n");
    val_dump(pcurrent);
#endif

    /* Store the initial old_value */
    val_assign(pold, &pcurrent->old_value, pcurrent->type);

    /* Set the initial min & max values; NOP if type==DBR_STRING */
    val_assign(pmax, &pcurrent->new_value.value, pcurrent->type);
    val_assign(pmin, &pcurrent->new_value.value, pcurrent->type);

    while (!shut_down) {                 /* Main Server Loop */
        int msg_size;

        /* Receive next message */
        msg_size = epicsMessageQueueReceiveWithTimeout(caPutLogQ, &pnext, MSG_SIZE, 5.0);

        if (msg_size == -1) {   /* timeout */
            if (!sent) {
                log_msg(pold, pcurrent, burst, pmin, pmax, config);
                val_assign(pold, &pcurrent->new_value.value, pcurrent->type);
                sent = TRUE;
                burst = FALSE;
            }
        }
        else if (msg_size != MSG_SIZE) {
            errlogSevPrintf(errlogMinor, "caPutLog: discarding incomplete log data message\n");
        }
        else if ((pnext->pfield == pcurrent->pfield) && (config != caPutLogAllNoFilter)) {

#if 0
            printf("caPutLog: received a message, same pv\n");
            val_dump(pnext);
#endif
            /* current and next are same pv */

            caPutLogDataFree(pcurrent);
            pcurrent = pnext;

            if (sent) {
                /* Set new initial max & min values */
                val_assign(pmax, &pcurrent->new_value.value, pcurrent->type);
                val_assign(pmin, &pcurrent->new_value.value, pcurrent->type);

                sent = FALSE;
                burst = FALSE;   /* First message after logging */
            }
            else {              /* Next put of multiple puts */
                if (isDbrNumeric(pcurrent->type)) {
                    burst = TRUE;
                    val_max(pmax, &pcurrent->new_value.value, pmax, pcurrent->type);
                    val_min(pmin, &pcurrent->new_value.value, pmin, pcurrent->type);
                }
            }
        }
        else {

#if 0
            printf("caPutLog: received a message, different pv\n");
            val_dump(pnext);
#endif
            /* current and next are different pvs */

            if (!sent) {
                log_msg(pold, pcurrent, burst, pmin, pmax, config);
                sent = TRUE;
            }

            caPutLogDataFree(pcurrent);
            pcurrent = pnext;

            /* Set new old_value */
            val_assign(pold, &pcurrent->old_value, pcurrent->type);

            /* Set new max & min values */
            val_assign(pmax, &pcurrent->new_value.value, pcurrent->type);
            val_assign(pmin, &pcurrent->new_value.value, pcurrent->type);

            sent = FALSE;
            burst = FALSE;
        }
    }
    errlogSevPrintf(errlogInfo, "caPutLog: log task exiting\n");
}

static void do_log(char *msg, size_t len, int truncated)
{
    if (truncated) {
        errlogSevPrintf(errlogMinor, "caPutLog: message truncated\n");
    }

    /* note: we reserved one extra char for this */
    assert(len < MAX_BUF_SIZE-1);
    strcpy(msg+len, "\n");

    /* send msg to log client */
    caPutLogClientSend(msg);

    /* log to PV if enabled */
    if (pcaPutLogPV) {
        long status;

        /* remove terminating newline for pv log */
        msg[len] = 0;
        status = dbPutField(pcaPutLogPV, DBR_CHAR, msg, len+1);

        if (status) {
            errlogSevPrintf(errlogMajor,
                "caPutLog: dbPutField to Log PV failed, status = %ld\n", status);
        }
    }
}

static void log_msg(const VALUE *pold_value, const LOGDATA *pLogData,
    int burst, const VALUE *pmin, const VALUE *pmax, int config)
{
    char buffer[MAX_BUF_SIZE];
    char * const msg = buffer;
    /* reserve one extra byte for terminating newline: */
    const size_t space = MAX_BUF_SIZE-1;
    size_t len;

    /* for single puts check optionally equalness of old and new values */
    if (!burst && !config) {
        if (val_equal(&pLogData->old_value, &pLogData->new_value.value, pLogData->type))
            return;                     /* don't log if values are equal */
    }

    /* first comes the time */
    len = epicsTimeToStrftime(msg, space, "%d-%b-%y %H:%M:%S",
        &pLogData->new_value.time);
    /* this should always succeed (18 chars, last time i counted */
    assert(len);

    /* host, user, pv_name */
    len += epicsSnprintf(msg+len, space-len,
        " %s %s %s new=", pLogData->hostid, pLogData->userid, pLogData->pv_name);
    if (len >= space) { do_log(msg, space-1, YES); return; }

    /* new value */
    len += val_to_string(msg+len, space-len,
        &pLogData->new_value.value, pLogData->type);
    if (len >= space) { do_log(msg, space-1, YES); return; }

    len += epicsSnprintf(msg+len, space-len, " old=");
    if (len >= space) { do_log(msg, space-1, YES); return; }

    /* old value */
    len += val_to_string(msg+len, space-len, pold_value, pLogData->type);
    if (len >= space) { do_log(msg, space-1, YES); return; }

    if (burst && isDbrNumeric(pLogData->type)) {
        /* min value */
        len += epicsSnprintf(msg+len, space-len, " min=");
        if (len >= space) { do_log(msg, space-1, YES); return; }
        len += val_to_string(msg+len, space-len, pmin, pLogData->type);
        if (len >= space) { do_log(msg, space-1, YES); return; }

        /* max value */
        len += epicsSnprintf(msg+len, space-len, " max=");
        if (len >= space) { do_log(msg, space-1, YES); return; }
        len += val_to_string(msg+len, space-len, pmax, pLogData->type);
        if (len >= space) { do_log(msg, space-1, YES); return; }
    }
    do_log(msg, len, NO);
}

static void val_min(VALUE *pres, const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
    case DBR_CHAR:
        pres->v_char = min(pa->v_char, pb->v_char);
        return;
    case DBR_UCHAR:
        pres->v_uchar = min(pa->v_uchar, pb->v_uchar);
        return;
    case DBR_SHORT:
        pres->v_short = min(pa->v_short, pb->v_short);
        return;
    case DBR_USHORT:
    case DBR_ENUM:
        pres->v_ushort = min(pa->v_ushort, pb->v_ushort);
        return;
    case DBR_LONG:
        pres->v_long = min(pa->v_long, pb->v_long);
        return;
    case DBR_ULONG:
        pres->v_ulong = min(pa->v_ulong, pb->v_ulong);
        return;
    case DBR_FLOAT:
        pres->v_float = min(pa->v_float, pb->v_float);
        return;
    case DBR_DOUBLE:
        pres->v_double = min(pa->v_double, pb->v_double);
        return;
    }
}

static void val_max(VALUE *pres, const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
    case DBR_CHAR:
        pres->v_char = max(pa->v_char, pb->v_char);
        return;
    case DBR_UCHAR:
        pres->v_uchar = max(pa->v_uchar, pb->v_uchar);
        return;
    case DBR_SHORT:
        pres->v_short = max(pa->v_short, pb->v_short);
        return;
    case DBR_USHORT:
    case DBR_ENUM:
        pres->v_ushort = max(pa->v_ushort, pb->v_ushort);
        return;
    case DBR_LONG:
        pres->v_long = max(pa->v_long, pb->v_long);
        return;
    case DBR_ULONG:
        pres->v_ulong = max(pa->v_ulong, pb->v_ulong);
        return;
    case DBR_FLOAT:
        pres->v_float = max(pa->v_float, pb->v_float);
        return;
    case DBR_DOUBLE:
        pres->v_double = max(pa->v_double, pb->v_double);
        return;
    }
}

/*
 * val_equal(): compare two VALUEs for equality
 */
static int val_equal(const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
    case DBR_CHAR:
        return (pa->v_char == pb->v_char);
    case DBR_UCHAR:
        return (pa->v_uchar == pb->v_uchar);
    case DBR_SHORT:
        return (pa->v_short == pb->v_short);
    case DBR_USHORT:
    case DBR_ENUM:
        return (pa->v_ushort == pb->v_ushort);
    case DBR_LONG:
        return (pa->v_long == pb->v_long);
    case DBR_ULONG:
        return (pa->v_ulong == pb->v_ulong);
    case DBR_FLOAT:
        return (pa->v_float == pb->v_float);
    case DBR_DOUBLE:
        return (pa->v_double == pb->v_double);
    default:
        return (0==strcmp(pa->v_string, pb->v_string));
    }
}

#if 0
#define cmp(a,b) (((a) < (b)) ? (-1) : (((a) > (b)) ? (+1) : 0))

/*
 * val_cmp(): compare two VALUEs for equality(0), smaller(-1) or larger(+1)
 */
static int val_cmp(const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
    case DBR_CHAR:
        return cmp(pa->v_char, pb->v_char);
    case DBR_UCHAR:
        return cmp(pa->v_uchar, pb->v_uchar);
    case DBR_SHORT:
        return cmp(pa->v_short, pb->v_short);
    case DBR_USHORT:
    case DBR_ENUM:
        return cmp(pa->v_ushort, pb->v_ushort);
    case DBR_LONG:
        return cmp(pa->v_long, pb->v_long);
    case DBR_ULONG:
        return cmp(pa->v_ulong, pb->v_ulong);
    case DBR_FLOAT:
        return cmp(pa->v_float, pb->v_float);
    case DBR_DOUBLE:
        return cmp(pa->v_double, pb->v_double);
    default:
        return strcmp(pa->v_string, pb->v_string);
    }
}
#endif

/*
 * val_assign(): assign src VALUE to dst VALUE
 */
static void val_assign(VALUE *dst, const VALUE *src, short type)
{
    switch (type) {
    case DBR_CHAR:
        dst->v_char = src->v_char;
        break;
    case DBR_UCHAR:
        dst->v_uchar = src->v_uchar;
        break;
    case DBR_SHORT:
        dst->v_short = src->v_short;
        break;
    case DBR_USHORT:
    case DBR_ENUM:
        dst->v_ushort = src->v_ushort;
        break;
    case DBR_LONG:
        dst->v_long = src->v_long;
        break;
    case DBR_ULONG:
        dst->v_ulong = src->v_ulong;
        break;
    case DBR_FLOAT:
        dst->v_float = src->v_float;
        break;
    case DBR_DOUBLE:
        dst->v_double = src->v_double;
        break;
    default:
        epicsSnprintf(dst->v_string, MAX_STRING_SIZE, "%s", src->v_string);
    }
}

/*
 * val_to_string(): convert VALUE to string
 */
static int val_to_string(char *pbuf, size_t buflen, const VALUE *pval, short type)
{
    switch (type) {
    case DBR_CHAR:
       /* CHAR and UCHAR are typically used as SHORTSHORT,
	* so avoid mounting NULL-bytes into the string
	*/
        return epicsSnprintf(pbuf, buflen, "%d", (int)pval->v_uchar);
    case DBR_UCHAR:
        return epicsSnprintf(pbuf, buflen, "%d", (int)pval->v_uchar);
    case DBR_SHORT:
        return epicsSnprintf(pbuf, buflen, "%hd", pval->v_short);
    case DBR_USHORT:
    case DBR_ENUM:
        return epicsSnprintf(pbuf, buflen, "%hu", pval->v_ushort);
    case DBR_LONG:
        return epicsSnprintf(pbuf, buflen, "%ld", pval->v_long);
    case DBR_ULONG:
        return epicsSnprintf(pbuf, buflen, "%lu", pval->v_ulong);
    case DBR_FLOAT:
        return epicsSnprintf(pbuf, buflen, "%g", pval->v_float);
    case DBR_DOUBLE:
        return epicsSnprintf(pbuf, buflen, "%g", pval->v_double);
    default:
        return epicsSnprintf(pbuf, buflen, "%s", pval->v_string);
    }
}

#if 0
static void val_dump(LOGDATA *pdata)
{
  char oldbuf[512], newbuf[512], timebuf[64];

    printf("pdata = %p\n", pdata);
    if (!pdata) {
        printf("pdata = NULL\n");
    }
    else {
        strcpy(oldbuf,"(conv fail)");
        strcpy(newbuf,"(conv fail)");
        strcpy(timebuf,"(strftime fail)");
        val_to_string(oldbuf,sizeof(oldbuf),&pdata->old_value,pdata->type);
        val_to_string(newbuf,sizeof(newbuf),&pdata->new_value.value,pdata->type);
        epicsTimeToStrftime(timebuf,sizeof(timebuf),"%Y-%m-%dT%H:%M:%S",&pdata->new_value.time);
        printf("userid = %s\n", pdata->userid);
        printf("hostid = %s\n", pdata->hostid);
        printf("pv_name = %s\n", pdata->pv_name);
        printf("pfield = %p\n", pdata->pfield);
        printf("type = %d\n", pdata->type);
        printf("old_value = %s\n", oldbuf);
        printf("new_value.time = %s\n", timebuf);
        printf("new_value.value = %s\n", newbuf);
    }
}
#endif
