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
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <logLib.h>
#include <envLib.h>
#include <rngLib.h>
#include <semLib.h>
#include <symLib.h>
#include <taskLib.h>
#include <sysSymTbl.h>
#include <errnoLib.h>
#include <usrLib.h>             /* for printErrno() prototype */

#include "errlog.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "asLib.h"
#include "dbCommon.h"
#include "tsDefs.h"
#include "drvTS.h"
#include "freeList.h"
#include "epicsDynLink.h"
#include "caPutLog.h"
#include "asTrapWrite.h"

extern int asActive;

RING_ID caPutLogRng = NULL; 	/* Ring Buffer ID for CA puts logging */
SEM_ID caPLRngSem = NULL; 	/* Semaphore ID for Ring Buffer 
				   is used for mutual exclusion during
				   Ring Buffer deleting */

#define DEBUG 0

char *strcat_cnt(char *s1, char *s2, int max);

static asTrapWriteId listenerId = NULL;		/* Trap Write Listener Id */
static void *pvt = 0;				/* For freeList requests */

static void caPutLog(asTrapWriteMessage *pmessage,int afterPut);

#define MAX_MSGS	1000	/* The length of Ring Buffer (in messages) */

/* caPutLogVersion - prints current CVS Id */
void caPutLogVersion()
{
 printf("\ncaPutLog version: \"%s\"\n",cvsid);
}

void caPutLogInit(int option)	/* Option: 0 - log only Puts changing values
					   1 - log all Puts */
{
 int status;
 SYM_TYPE symType;
 long (*RngLogTaskStart)();

	if ((option != 0)&&(option != 1)) option = 0;	/* Set default option */
	if (asActive) {		/* AS is enabled */
	   if (!caPutLogRng) {	/* Check is there some Rng */
	      caPutLogRng = rngCreate(sizeof(LOGDATA)*MAX_MSGS);	/* Create Rng */
	   }
	   if (!caPutLogRng) {
	      errlogPrintf("caPutLogInit: caPutLog Rng creation failed. CA Put Logging is disabled\n");
	      return;
	   }
	   else {
	      caPutLogVersion();
	      errlogPrintf("caPutLogInit: Rng exists. CA Put Registering is enabled\n");

	      caPLRngSem = semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);	/* Create semaphore */
	      if (!caPLRngSem) {
		errlogPrintf("caPutLogInit: semaphore creation failure. CA Put Logging is disabled\n");
		goto init_exit;
	      }       
	      status = semGive(caPLRngSem);
	      if (status) {
		errlogPrintf("caPutLogInit: semGive status=");
		printErrno(errno);
	      }

	/* Initialize the free list of log elements */
	
	      if (!pvt) {	/* freeList initialization should be done once only */
		freeListInitPvt(&pvt, sizeof(LOGDATA), FREE_LIST_SIZE);
	      }	 

	/* Search for "RngLogTaskStart" entry point to start
	   RngLogtask (taking Log messages from Rng and putting them
	   into PV or to Log file) */
	   
	      status = symFindByNameEPICS(sysSymTbl, "_RngLogTaskStart", (char**)&RngLogTaskStart, &symType);
	      if (status) {	/* Symbol isn't found */
	         errlogPrintf("caPutLogInit: 'RngLogTask' is not found. Load it and execute 'asInit' again\n");
		 return;
	      }
	      else {

	/* Initialize the Trap Write Listener */
	
                 listenerId = asTrapWriteRegisterListener(caPutLog);
		 
		 (*RngLogTaskStart)(option);	/* Start or check RngLogTask */
	      }
	   }
	}
init_exit:
	return;
}

void caPutLogStop()
{
 int tid,status;
 long (*RngLogTaskStop)();
 SYM_TYPE symType;
 
	   if (caPutLogRng) {	/* If there is any Ring Buffer delete it
	   			   and stop 'RngLogTask' if it exists */
	      if ((tid = taskNameToId("RngLogTask")) != ERROR) {	/* Task exists; it should be stoppped */
	         status = symFindByNameEPICS(sysSymTbl, "_RngLogTaskStop", (char**)&RngLogTaskStop, &symType);
	         if (status) {	/* Symbol isn't found */
	            errlogPrintf("caPutLogStop: 'RngLogTaskStop' is not found!?. RngLogTask cannot be stopped\n");
	         }
		 else (*RngLogTaskStop)();		/* stop the 'RngLogTask' */
	      }
	      if (caPLRngSem) {
	         semTake(caPLRngSem,WAIT_FOREVER);
	         rngDelete(caPutLogRng);
		 semDelete(caPLRngSem);		/* Delete semaphore */
		 caPLRngSem = NULL;
	      }
	   }
	   caPutLogRng = NULL;		/* clear pointer */

	   if (listenerId) {
	      asTrapWriteUnregisterListener(listenerId);	/* Unregister listener */
	      listenerId = NULL;
	   }
	   errlogPrintf("caPutLogInit: CA Put Logging is disabled\n");
	   return;
}

static void caPutLog(asTrapWriteMessage *pmessage,int afterPut)
{
 DBADDR *paddr = (DBADDR *)pmessage->serverSpecific;
 struct dbCommon *precord = (struct dbCommon *)paddr->precord;
 dbFldDes *pdbFldDes = (dbFldDes *)(paddr->pfldDes);
 LOGDATA *plocal = NULL;
 long options, num_elm = 1;
 STATUS status;
 int free_bytes;
 TS_STAMP curTime;

    if (!caPutLogRng) return;		/* If no Rng no actions */

    if (afterPut == FALSE) {		/* Call before Put, allocate the Log structure */
	plocal = freeListMalloc(pvt);
	if (plocal == NULL) {
	   errlogPrintf("aslogPut: memory allocation failed\n");
	   pmessage->userPvt = NULL;
	   return;
	}
	else pmessage->userPvt = (void *)plocal;	/* Save the pointer on allocated structure */
	plocal->userid[0] = 0;

	strcat_cnt(plocal->userid,pmessage->userid,MAX_USR_ID);	/* Set user ID */
    	plocal->hostid[0] = 0;
	strcat_cnt(plocal->hostid,pmessage->hostid,MAX_HOST_ID);	/* and host ID into LogData */

/* Set PV_name.field_name & field type */

	plocal->pv_name[0]=0;
	strcat(plocal->pv_name,precord->name);
	strcat(plocal->pv_name,".");
	strcat(plocal->pv_name,pdbFldDes->name);
	plocal->type = paddr->field_type;

	options = 0;
	num_elm = 1;
	if (plocal->type > DBF_ENUM)		/* Get other field as a string */
	   status = dbGetField(paddr, DBR_STRING, &plocal->old_value, &options,
	 		    &num_elm, 0);
	else			/* Up to ENUM type get as a value */
	   status = dbGetField(paddr, plocal->type, &plocal->old_value, &options,
	 		    &num_elm, 0);

	if (status) {
	   errlogPrintf("caPutLog: dbGetField error=%d\n", status);
	   plocal->type = DBR_STRING;
	   strcpy(plocal->old_value.v_string,"Not_Accesable");	   
	}

	return;
    }
    else {		/* Call after Put */

	plocal = (LOGDATA *) pmessage->userPvt;		/* Restore the pointer */
	options = DBR_TIME;

	num_elm = 1;
	if (plocal->type > DBF_ENUM)		/* Get other field as a string */
	   status = dbGetField(paddr, DBR_STRING, &plocal->new_value, &options,
	 		    &num_elm, 0);
	else			/* Up to ENUM type get as a value */
	   status = dbGetField(paddr, plocal->type, &plocal->new_value, &options,
			    &num_elm, 0);
	TScurrentTimeStamp((struct timespec *)&curTime);	/* get current time stamp */
	if (status) {
	   errlogPrintf("caPutLog: dbGetField error=%d\n", status);
	   plocal->type = DBR_STRING;
	   strcpy(plocal->new_value.value.v_string,"Not_Accesable");	   
	}
	/* replace, if necessary, the time stamp */
	if (plocal->new_value.time.secPastEpoch < curTime.secPastEpoch) {
		plocal->new_value.time.secPastEpoch = curTime.secPastEpoch;
		plocal->new_value.time.nsec = curTime.nsec;
	}
    }

    if (caPutLogRng) {		/* If Rng exists */
	free_bytes = rngFreeBytes(caPutLogRng);	/* Get free bytes in Rng */
	if (free_bytes < sizeof(LOGDATA)) {	/* Rng is full */
	         errlogPrintf("caPutLog: Ring is full\n");
	}
	else {
	   free_bytes = rngBufPut(caPutLogRng, (char *) plocal, sizeof(LOGDATA));	/* Send message */
	   if (free_bytes < sizeof(LOGDATA))
	      errlogPrintf("caPutLog: was saved %d bytes into Rng instead of %d\n", 
	      		free_bytes, sizeof(LOGDATA));
	}
    }

    freeListFree(pvt, plocal);		/* Release LogData memory */

    return;
}



/* Routine to concatenate string not overflowing a string buffer */

char *strcat_cnt(char *s1, char *s2, int max)
{
 int len = strlen(s1);		/* Get the original string length */
 int cnt;

 if (len < max) {
    cnt = max - len - 1;	/* Count the rest of the buffer */
    strncat(s1,s2,cnt);		/* Concatenate not more than cnt chars */
    return (s1);
 }
 else return(NULL);
}
