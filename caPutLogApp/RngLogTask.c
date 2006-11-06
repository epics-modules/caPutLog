/*	File:	  RngLogTask.c
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
#include <usrLib.h>
#include <errnoLib.h>
#include <symLib.h>
#include <sysSymTbl.h>

#include <dbFldTypes.h>
#include <epicsTypes.h>
#include <tsDefs.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <epicsDynLink.h>
#include <errlog.h>
#include <asLib.h>
#include <caPutLog.h>

#define MAX_BUF_SIZ 120 	/* Length of log string */

#define DEBUG 0

extern int asActive;
extern RING_ID caPutLogRng;			/* Ring Buffer ID to scan */
extern char *strcat_cnt(char *, char *, int);
extern SEM_ID caPLRngSem; 			/* Semaphore ID for Ring Buffer */
extern void CAPutLogClientShutdown(void);	/* To shutdown CA log
						Client when CA Put Logging 
						is disabled */

static long (*iocCAPutLogPrintf)() = NULL;	/* Call to log message */

typedef struct val_typ {
   VALUE val;
   ushort type;
} Val_Typ;

static void log_msg(Val_Typ *, LOGDATA *, ushort, Val_Typ *, Val_Typ *, int);
static STATUS RngLogTask(int);

static void val2a(VALUE *, short, char *);
static void val_min(ushort, Val_Typ *, VALUE *, Val_Typ *);
static void val_max(ushort, Val_Typ *, VALUE *, Val_Typ *);
static int val_cmp(VALUE *old, VALUE *new, short type);

static short shut_down = FALSE;		/* Shut down flag */

static DBADDR caPutLogPV;		/* Structure to keep address of Log PV */
static DBADDR *pcaPutLogPV = NULL;	/* Pointer on PV address structure, 
					   is used as a flag whether this
					   PV is defined or not */
					   
/* RngLogTaskVersion - prints current CVS version */
void RngLogTaskVersion()
{
 logMsg("RngLogTask version: \"%s\"\n",(int)cvsid,0,0,0,0,0);
 taskDelay(3);
}

/* Start Rng Log Task */

STATUS RngLogTaskStart(option)
{
 int tid;

   tid = taskNameToId("RngLogTask");
   if (tid != ERROR) {
	errlogPrintf("Task 'RngLogTask' exists already!\n");
	return(0);
   }
   shut_down = FALSE;
   tid = taskSpawn("RngLogTask", 200, VX_FP_TASK | VX_STDIO, 5000,
	    (FUNCPTR) RngLogTask, option, 0, 0, 0, 0, 0, 0, 0, 0, 0);

   if (tid == ERROR) {
      errlogPrintf("RngLogTask start failed: ");
      printErrno(0);
      return(errno);
   }

   return(0);
}

/* Stop Rng Log Task */

STATUS RngLogTaskStop(void)
{

 shut_down = TRUE;
 return(0);
}


#define RNG_TIME_OUT 180	/* Time-out for Rng Receive (3 sec) */
#define LOG_TIME_OUT 150	/* Time-out 5 sec not to log Puts from the same PV */

/* Log Server */

/* RngLogTask option: 0 - log only Puts changing values
 *		      1 - log all Puts
 */

static STATUS RngLogTask(int option)
{
 char *caPutLogPVEnv;
 char fullPVname[PVNAME_STRINGSZ+4];		/* <PV name>.<field name> */
 long getpv_st = 0;
 char *caPutLogRngEnv;
 LOGDATA curr_Logdata, next_Logdata;
 int byte_numb, status;
 int TO_cnt = LOG_TIME_OUT;			/* Initialize Log time-out counter */
 unsigned short curr_sent = FALSE, mult = FALSE;

 Val_Typ old_value;
 Val_Typ max_val, min_val;

 long *entry;
 SYM_TYPE symType;
 STATUS (*iocCAPutLogClientInit)();

	RngLogTaskVersion();	
	if (!asActive) {	/* If AS isn't active */
	   errlogPrintf("RngLogTask: AS isn't active. RngLogTask is down\n");
	   exit(0);
	}
	else {
	   if (caPutLogRng) {	/* If Ring Buffer exists */	

	/* Search for "iocCAPutLogInit" entry point to start
	    		iocCAPutLogClient */
	   
	      status = symFindByNameEPICS(sysSymTbl, "_iocCAPutLogInit", (char**)&iocCAPutLogClientInit, &symType);
	      if (status) {	/* Symbol isn't found */
	         errlogPrintf("RngLogTask: 'iocCAPutLogClient' is not found; load it and start again\n");
		 exit(0);
	      }
	      else {
		 (*iocCAPutLogClientInit)();	/* Init iocCAPutLogClient */

		/* Find entry point 'iocCAPutLogPrintf' */
		
	         status = symFindByNameEPICS(sysSymTbl, "_iocCAPutLogPrintf", (char**)&entry, &symType);
	         if (status) {	/* Symbol isn't found */
	            errlogPrintf("RngLogTask: 'iocCAPutLogPrintf' is not found!???. CA Put Logging is disabled\n");
		 }
		 else {
		    iocCAPutLogPrintf = entry;
		 }
	      }
	      
/*  Check is it necessary to log into PV */
/* Set/clear the pointer of CA Put Log PV */

		caPutLogPVEnv = getenv("EPICS_AS_PUT_LOG_PV");	/* Search for variable */

		if (!caPutLogPVEnv) {
	   	   pcaPutLogPV = NULL;		/* If no -- clear pointer */
	   	   errlogPrintf("RngLogTask: EPICS_AS_PUT_LOG_PV variable is not defined. CA Put Logging to PV is disabled\n");
		   if (iocCAPutLogPrintf == NULL) {
serv_down:
		      errlogPrintf("RngLogTask: Server shutdowned\n");
		      exit(0);
		   }
		}
		else {
	   	   if (!(*caPutLogPVEnv)) {
	      		pcaPutLogPV = NULL;		/* If no -- clear pointer */
	      		errlogPrintf("RngLogTask: EPICS_AS_PUT_LOG_PV variable is not defined. CA Put Logging to PV is disabled\n");
			if (iocCAPutLogPrintf == NULL) goto serv_down;
	      		else goto first_msg;
	   	   }
	   	   pcaPutLogPV = &caPutLogPV;
	   	   strcpy(fullPVname,caPutLogPVEnv);	/* Form string"<name>.<VAL>" */
	   	   if (!strpbrk(fullPVname,"."))
	      		strcat(fullPVname,".VAL");
	   	   getpv_st = dbNameToAddr(fullPVname, pcaPutLogPV);
	   	   if (getpv_st) {
	      		pcaPutLogPV = NULL;	/* If not OK -- clear pointer */
	      		errlogPrintf("RngLogTask: PV for CA Put Logging not found. CA Put Logging to PV is disabled\n");
			if (iocCAPutLogPrintf == NULL) goto serv_down;
	   	   }
		}

	      
first_msg:
	      if (!caPLRngSem) goto no_ring;	/* If no semaphore Rng, probably
	      					   have been deleted */
	      status = semTake(caPLRngSem,RNG_TIME_OUT);	/* Lock the Rng */
	      if (status) {
no_ring:
		 errlogPrintf("RngLogTask: Rng doesn't exist.\n");
	         goto serv_down;	/* If ERROR Rng, probably
	      				   has been deleted */
	      }

	      byte_numb = rngNBytes(caPutLogRng);
	      if (byte_numb < sizeof(LOGDATA)) {
	         semGive(caPLRngSem);		/* Unlock the Rng */
		 taskDelay(2);
	         goto first_msg;
	      }
	      
	      byte_numb = rngBufGet(caPutLogRng, (char *)&curr_Logdata, sizeof(LOGDATA));		/* Receive 1-st message */
	      semGive(caPLRngSem);		/* Unlock the Rng */

	      memcpy(&old_value.val,&curr_Logdata.old_value,sizeof(VALUE));	/* Store the initial old_value */
	      old_value.type = curr_Logdata.type;
	      if ((curr_Logdata.type > DBF_STRING)&&
	          (curr_Logdata.type <= DBF_ENUM)) {	/* Store initial max/min */

	/* Set the initial min & max values */
	
		memcpy(&max_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
		max_val.type = curr_Logdata.type;
		memcpy(&min_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
		min_val.type = curr_Logdata.type;
	      }
	   
	      while (1) {		/* Main Server Loop */
		if (shut_down) {
loop_exit:

		   errlogPrintf("RngLogTask is terminated\n");
		   exit(0);
		}
		if (!caPLRngSem) {	/* Probably, Rng was deleted */
		   errlogPrintf("Rng doesn't exist anymore (1).\n");
		   goto loop_exit;
		}
		status = semTake(caPLRngSem,RNG_TIME_OUT);	/* Lock the ring */
		if (status) {	/* Probably, Rng was deleted */
		   errlogPrintf("Rng doesn't exist anymore (2).\n");
		   goto loop_exit;
		}

		byte_numb = rngNBytes(caPutLogRng);
		if (byte_numb < sizeof(LOGDATA)) {	/* if Rng doesn't contain full message postpone */
		  if (!TO_cnt) {
		     if (!curr_sent) {
		
			log_msg(&old_value,&curr_Logdata,mult,&min_val,&max_val,option);	/* Log 'current' message if it wasn't logged */
			memcpy(&old_value.val,&curr_Logdata.new_value.value,sizeof(VALUE)); /* Update old_value */
	      		old_value.type = curr_Logdata.type;
			curr_sent = TRUE;	/* Set flag curr_set = TRUE */
			mult = FALSE;
		     }
		     TO_cnt = LOG_TIME_OUT;	/* Reset Log Time-out counter */
		  } else TO_cnt--;		/* Decrement Time-out counter */
		  semGive(caPLRngSem);			/* Unlock Rng */
		  taskDelay(2);
		  continue;
		} 
		TO_cnt = LOG_TIME_OUT;	/* Reset Log Time-out counter */
		byte_numb = rngBufGet(caPutLogRng, (char *)&next_Logdata, sizeof(LOGDATA));		/* Receive next message */
		semGive(caPLRngSem);		/* Unlock Rng */
		
		if (byte_numb == sizeof(LOGDATA)) {
		   if (!strcmp(next_Logdata.pv_name,curr_Logdata.pv_name)) {	/* Both messages have the same pv_name */
			memcpy(&curr_Logdata, &next_Logdata, sizeof(LOGDATA));	/* Copy 'next' into 'current' */
			if (curr_sent)	{	/* Put was logged */
			   if ((curr_Logdata.type > DBF_STRING)&&
	          		(curr_Logdata.type <= DBF_ENUM)) {	/* Only for numeric data */

		/* Set new initial max & min values */

				memcpy(&max_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
				max_val.type = curr_Logdata.type;
				memcpy(&min_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
				min_val.type = curr_Logdata.type;
			   }			   
			   curr_sent = FALSE;	/* Set flag curr_set = FALSE */
			   mult = FALSE;	/* First message after logging */
			}
			else {		/* Next put of multiple puts */
			   if ((curr_Logdata.type > DBF_STRING)&&
	          		(curr_Logdata.type <= DBF_ENUM)) {	/* For numeric
									values set new max/min */
			   	if (!mult) 	/* Set flag only for numeric values */
		   		   mult = TRUE;

				val_max(curr_Logdata.type, &max_val, &curr_Logdata.new_value.value, &max_val);
				val_min(curr_Logdata.type, &min_val, &curr_Logdata.new_value.value, &min_val);

			   }
			}
		   }
		   else {	/* Messages related to different PV */
			if (!curr_sent) {
	
			   log_msg(&old_value,&curr_Logdata,mult,&min_val,&max_val,option);	/* Log 'current' message */
			   curr_sent = TRUE;
			}

			memcpy(&old_value.val,&next_Logdata.old_value,sizeof(VALUE));	/* Store the initial old_value for 'next' */
			memcpy(&curr_Logdata, &next_Logdata, sizeof(LOGDATA));	/* Copy 'next' into 'current' */
			if ((curr_Logdata.type > DBF_STRING)&&
	          	    (curr_Logdata.type <= DBF_ENUM)) {	/* Only for numeric data */
	
			   memcpy(&max_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
			   max_val.type = curr_Logdata.type;
			   memcpy(&min_val.val,&curr_Logdata.new_value.value,sizeof(VALUE));
			   min_val.type = curr_Logdata.type;
	
			}
			curr_sent = FALSE;	/* Set flag curr_set = FALSE */
			mult = FALSE;	/* Reset mult flag */
		   }
		}
 	      }	/* End of main loop */
	   }
	   else {
		errlogPrintf("RngLogTask: Rng doesn't exist. Server is shutdowned\n");
		exit(0);
	   }
	}
	return(0);
}

static void log_msg(
   Val_Typ *pold_value,
   LOGDATA *pLogData,
   ushort mult, 
   Val_Typ *pmin,
   Val_Typ *pmax,
   int option)
{
 char msg_buf[MAX_BUF_SIZ];		/* string to form log message to Log Server */

 char amax[21] = " max=";
 char amin[21] = " min=", *cc;
 char atime[32], atime_mdf[19] = "  -   -";
 char aval[MAX_STRING_SIZE];

 long status;
 int strsiz;

/* for single puts check optionally equalness of old and new values */

   if ((!mult) && (!option)) {
      if (val_cmp(&(pLogData->old_value),&(pLogData->new_value.value), pLogData->type)) return;	/* don't log if values are equal */
   }

   msg_buf[0] = 0;
   tsStampToText(&pLogData->new_value.time, TS_TEXT_MONDDYYYY, atime);	/* Get time string */

/* Modify string to DD-Mon-YY */
	
   strncpy(atime_mdf, &atime[4], 2);		/* Move date */
   strncpy(&atime_mdf[3], atime, 3);		/* Move month */
   strncat(&atime_mdf[7], &atime[10], 11);	/* Move year and time in sec */
	
   strcat(msg_buf,atime_mdf);	/* Copy date & time */
   strcat(msg_buf," ");
   strcat(msg_buf,pLogData->hostid);		/* Copy host ID */
   strcat(msg_buf," ");
   strcat(msg_buf,pLogData->userid);		/* Copy user ID */
   strcat(msg_buf," ");
   strcat_cnt(msg_buf,pLogData->pv_name,MAX_BUF_SIZ);		/* Copy PV name.field name */

/* Log first new value then old one */

   strcat_cnt(msg_buf," new=",MAX_BUF_SIZ);

   if ((pLogData->type > DBF_STRING)&&(pLogData->type <= DBF_ENUM)) {
      val2a(&pLogData->new_value.value,pLogData->type,aval);	/* Convert new value to ASCII */
      strcat_cnt(msg_buf,aval,MAX_BUF_SIZ);	/* Copy new value */
   }
   else {
      strcat_cnt(msg_buf,pLogData->new_value.value.v_string,MAX_BUF_SIZ);	/* Copy string */
   }
   strcat_cnt(msg_buf," old=",MAX_BUF_SIZ);
   if ((pLogData->type > DBF_STRING)&&(pLogData->type <= DBF_ENUM)) {
      val2a(&pold_value->val,pLogData->type,aval);	/* Convert old value to ASCII */
      strcat_cnt(msg_buf,aval,MAX_BUF_SIZ);	/* Copy old value */
   }
   else {
      strcat_cnt(msg_buf,pold_value->val.v_string,MAX_BUF_SIZ);	/* Copy string */
   }
   
   if (mult) {		/* If log after multi puts insert max & min values */
      if ((pLogData->type > DBF_STRING)&&(pLogData->type <= DBF_ENUM)) {
	val2a(&pmin->val,pLogData->type, &amin[5]);	/* Convert min value to ASCII */
 	val2a(&pmax->val,pLogData->type, &amax[5]);	/* Convert max value to ASCII */

    	cc = strcat_cnt(msg_buf,amin,MAX_BUF_SIZ);
	if (cc == NULL)
	   errlogPrintf("RngLogTask: log_msg: Logbuffer is full\n");
     	cc = strcat_cnt(msg_buf,amax,MAX_BUF_SIZ);
 	if (cc == NULL)
	   errlogPrintf("RngLogTask: log_msg: Logbuffer is full\n");
      }
   }

/* Log the message */

   if (iocCAPutLogPrintf != NULL)
      (*iocCAPutLogPrintf)(" %s\n", msg_buf);	/* Log with iocCAPutLogClient */
   else
      errlogPrintf("RngLogTask: log_msg: iocCALogClient is not initialized\n");

    if (pcaPutLogPV) {	/* If Logging to PV is enabled */

	strsiz = strlen(msg_buf) + 1;		/* Get string length + '\0' */
	status = dbPutField(&caPutLogPV, DBR_CHAR, &msg_buf[0], strsiz); 	/* Log the data */

	if (status)
	   errlogPrintf("RngLogTask: dbPutField to Log PV failed, status = %ld\n", status);

    }		/* End of Logging into PV */
    return;
}

static void val_min(ushort type,
		 Val_Typ *pa,
		 VALUE *pb,
		 Val_Typ *pres)
{

	switch (type) {
	   case DBF_SHORT: 
	        pres->val.v_short = (pb->v_short < pa->val.v_short)? pb->v_short: pa->val.v_short;
		return;

	   case DBF_USHORT: 
	   case DBF_ENUM: 
	        pres->val.v_ushort = (pb->v_ushort < pa->val.v_ushort)? pb->v_ushort: pa->val.v_ushort;
		return;

	   case DBF_LONG: 
	        pres->val.v_long = (pb->v_long < pa->val.v_long)? pb->v_long: pa->val.v_long;
		return;

	   case DBF_ULONG: 
	        pres->val.v_ulong = (pb->v_ulong < pa->val.v_ulong)? pb->v_ulong: pa->val.v_ulong;
		return;

	   case DBF_FLOAT: 
	        pres->val.v_float = (pb->v_float < pa->val.v_float)? pb->v_float: pa->val.v_float;
		return;

	   case DBF_DOUBLE: 
	        pres->val.v_double = (pb->v_double < pa->val.v_double)? pb->v_double: pa->val.v_double;
		return;

	   case DBF_CHAR: 
	        pres->val.v_char = (pb->v_char < pa->val.v_char)? pb->v_char: pa->val.v_char;
		return;

	   case DBF_UCHAR: 
	        pres->val.v_uchar = (pb->v_uchar < pa->val.v_uchar)? pb->v_uchar: pa->val.v_uchar;
		return;
	}
}	

static void val_max(ushort type,
		 Val_Typ *pa,
		 VALUE *pb,
		 Val_Typ *pres)
{

	switch (type) {
	   case DBF_SHORT: 
	        pres->val.v_short = (pb->v_short > pa->val.v_short)? pb->v_short: pa->val.v_short;
		return;

	   case DBF_USHORT: 
	   case DBF_ENUM: 
	        pres->val.v_ushort = (pb->v_ushort > pa->val.v_ushort)? pb->v_ushort: pa->val.v_ushort;
		return;

	   case DBF_LONG: 
	        pres->val.v_long = (pb->v_long > pa->val.v_long)? pb->v_long: pa->val.v_long;
		return;

	   case DBF_ULONG: 
	        pres->val.v_ulong = (pb->v_ulong > pa->val.v_ulong)? pb->v_ulong: pa->val.v_ulong;
		return;

	   case DBF_FLOAT: 
	        pres->val.v_float = (pb->v_float > pa->val.v_float)? pb->v_float: pa->val.v_float;
		return;

	   case DBF_DOUBLE: 
	        pres->val.v_double = (pb->v_double > pa->val.v_double)? pb->v_double: pa->val.v_double;
		return;

	   case DBF_CHAR: 
	        pres->val.v_char = (pb->v_char > pa->val.v_char)? pb->v_char: pa->val.v_char;
		return;

	   case DBF_UCHAR: 
	        pres->val.v_uchar = (pb->v_uchar > pa->val.v_uchar)? pb->v_uchar: pa->val.v_uchar;
		return;
	}
}	

static void val2a(VALUE *pval, short type, char *pstr)
{
 int tmp;
 
	switch (type) {
	   case DBF_CHAR:
	   case DBF_UCHAR:
	   	tmp = (int) pval->v_uchar;
		sprintf(pstr,"%c", tmp);
		return;

	   case DBF_SHORT:
		sprintf(pstr,"%hd", pval->v_short);
		return;

	   case DBF_USHORT:
	   case DBF_ENUM:
		sprintf(pstr,"%hu", pval->v_ushort);
		return;

	   case DBF_LONG:
		sprintf(pstr,"%ld", pval->v_long);
		return;

	   case DBF_ULONG:
		sprintf(pstr,"%lu", pval->v_ulong);
		return;

	   case DBF_FLOAT:
		sprintf(pstr,"%g", pval->v_float);
		return;

	   case DBF_DOUBLE:
		sprintf(pstr,"%g", pval->v_double);
		return;
	}
		
}

/* val_cmp compares old and new value according to their types
 * returns: TRUE if values are equal
 *	    FALSE otherwise
 */

static int val_cmp(VALUE *old, VALUE *new, short type)
{
   switch (type) {
      case DBF_CHAR: return ((old->v_char == new->v_char)? TRUE: FALSE);

      case DBF_UCHAR: return ((old->v_uchar == new->v_uchar)? TRUE: FALSE);

      case DBF_SHORT: return ((old->v_short == new->v_short)? TRUE: FALSE);

      case DBF_USHORT:
      case DBF_ENUM: return ((old->v_ushort == new->v_ushort)? TRUE: FALSE);

      case DBF_LONG: return ((old->v_long == new->v_long)? TRUE: FALSE);

      case DBF_ULONG: return ((old->v_ulong == new->v_ulong)? TRUE: FALSE);

      case DBF_FLOAT: return ((old->v_float == new->v_float)? TRUE: FALSE);

      case DBF_DOUBLE: return ((old->v_double == new->v_double)? TRUE: FALSE);

	/* Other types are passed as a string */
	     
      default: return ((!strcmp(old->v_string,new->v_string))? TRUE: FALSE);
   }
}
