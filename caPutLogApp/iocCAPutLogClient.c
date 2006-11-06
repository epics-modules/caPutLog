/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      NOTES:
 *
 * Modification Log:
 * -----------------
 * .00 joh 080791  	Created
 * .01 joh 081591	Added epics env config
 * .02 joh 011995	Allow stdio also	
 *
 * Revision 1.1  2003/02/26 14:40:09  birke
 * Initial revision
 *
 * Revision 1.2  2000/11/06 11:27:27  korobov
 * Korobov: logMsg calls are replaced with errlogPrintf
 *
 * Revision 1.1  2000/09/14 14:08:10  korobov
 * Korobov: to create caPutLog iocCore
 *
 * Revision 1.1  1999/05/28 15:00:23  korobov
 * Korobov: 1-st commit
 *
 * Revision 1.1.1.1  1997/10/15 14:42:59  csuka
 * Initial import R3.13.0.beta11
 *
 * Revision 1.16  1997/06/25 06:12:49  jhill
 * added diagnostic
 *
 * Revision 1.15  1997/04/11 20:24:13  jhill
 * added const to failureNotify()
 *
 * Revision 1.14  1997/04/10 20:03:53  jhill
 * use include  not include <>
 *
 * Revision 1.13  1996/06/19 18:01:09  jhill
 * log entries in header were different
 *
 * iocCAPutLogClient: copied and modified from iocLogClient.c
 * by V.Korobov 25.08.98
 *
 * iocCAPutLogClient: logMsg() call is replaced with errlogPrintf()
 * by V.Korobov 02.11.00
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <socket.h>
#include <in.h>

#include <ioLib.h>
#include <taskLib.h>
#include <taskHookLib.h>
#include <logLib.h>
#include <inetLib.h>
#include <sockLib.h>
#include <sysLib.h>
#include <semLib.h>
#include <rebootLib.h>

#include "errlog.h"
#include "envDefs.h"
#include "task_params.h"

#define DEBUG 0

/*
 * for use by the vxWorks shell
 */
int 		iocCAPutLogDisable = 0;

LOCAL FILE		*iocCAPutLogFile = NULL;
LOCAL int 		iocCAPutLogFD = ERROR;
LOCAL unsigned		iocCAPutLogTries = 0U;
LOCAL unsigned		iocCAPutLogConnectCount = 0U;

LOCAL long 		ioc_log_port;
LOCAL struct in_addr 	ioc_log_addr;

int 			iocCAPutLogInit(void);

LOCAL void 		logClientShutdown(void);
LOCAL int 		getConfig(void);
LOCAL void 		failureNotify(const ENV_PARAM *pparam);
LOCAL void 		logRestart(void);
LOCAL int 		iocCAPutLogAttach(void);
LOCAL void 		logClientRollLocalPort(void);

LOCAL SEM_ID		iocCAPutLogMutex;	/* protects stdio */
LOCAL SEM_ID		iocCAPutLogSignal;	/* reattach to log server */

#define EPICS_IOC_LOG_CLIENT_CONNECT_TMO 5 /* sec */
#define EPICS_IOC_CA_PUT_LOG_PORT 7010	/* Default port number for CA Put Logging */


/*
 *	iocCAPutLogInit()
 */
int iocCAPutLogInit(void)
{
	int	status;
	int	attachStatus;
	int	options;

	if(iocCAPutLogDisable){
		return OK;
	}

	/*
	 * dont init twice
	 */
	if (iocCAPutLogMutex) {
		return OK;
	}

/**********/
	options = SEM_Q_PRIORITY|SEM_DELETE_SAFE|SEM_INVERSION_SAFE;
	iocCAPutLogMutex = semMCreate(options);
	if(!iocCAPutLogMutex){
		return ERROR;
	}
	if (!iocCAPutLogMutex) {
	   errlogPrintf("iocCAPutLogInit: iocCAPutLogMutex semaphore doesn't exist\n",
	   0,0,0,0,0,0);
	   return ERROR;
	}
/************/

	iocCAPutLogSignal = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!iocCAPutLogSignal){
		return ERROR;
	}

	attachStatus = iocCAPutLogAttach();

	status = rebootHookAdd((FUNCPTR)logClientShutdown);
	if (status<0) {
		printf("Unable to add log server reboot hook\n");
	}

	status = taskSpawn(	
			"caPutLogRestart",
/***			LOG_RESTART_NAME, ***/
			LOG_RESTART_PRI, 
			LOG_RESTART_OPT, 
			LOG_RESTART_STACK, 
			(FUNCPTR)logRestart,
			0,0,0,0,0,0,0,0,0,0);
	if (status==ERROR) {
		printf("Unable to start log server connection watch dog\n");
	}

	return attachStatus;
}


/*
 *	iocCAPutLogAttach()
 */
LOCAL int iocCAPutLogAttach(void)
{

	int            		sock;
        struct sockaddr_in      addr;
	int			status;
	int			optval;
	struct timeval		tval;
	FILE			*fp;

	status = getConfig();
	if(status<0){
		printf (
		"iocCAPutLogClient: EPICS environment under specified\n");
		printf ("iocCAPutLogClient: failed to initialize\n");
		return ERROR;
	}

	/* allocate a socket       */
	sock = socket(AF_INET,		/* domain       */
		      SOCK_STREAM,	/* type         */
		      0);		/* deflt proto  */
	if (sock < 0){
		printf ("iocCAPutLogClient: no socket error %s\n", 
			strerror(errno));
		return ERROR;
	}

        /*      set socket domain       */
        addr.sin_family = AF_INET;

        /*      set the port    */
        addr.sin_port = htons(ioc_log_port);

        /*      set the addr */
        addr.sin_addr.s_addr = ioc_log_addr.s_addr;

	/* connect */
#ifdef vxWorks
	tval.tv_sec = EPICS_IOC_LOG_CLIENT_CONNECT_TMO;
	tval.tv_usec = 0;
	status = connectWithTimeout(
			 sock,
			 (struct sockaddr *)&addr,
			 sizeof(addr),
			 &tval);
#else
	status = connect(
			 sock,
			 (struct sockaddr *)&addr,
			 sizeof(addr));
#endif
	if (status < 0) {
		/*
		 * only print a message if it is the first try and
		 * we havent got a valid connection already
		 */
		if (iocCAPutLogTries==0U && iocCAPutLogFD==ERROR) {
			char name[INET_ADDR_LEN];

			inet_ntoa_b(addr.sin_addr, name);
			printf(
	"iocCAPutLogClient: unable to connect to %s port %d because \"%s\"\n", 
				name,
				addr.sin_port,
				strerror(errno));
		}
		iocCAPutLogTries++;
		close(sock);
		return ERROR;
	}

	iocCAPutLogTries=0U;
	iocCAPutLogConnectCount++;

	/*
	 * discover that the connection has expired
	 * (after a long delay)
	 */
        optval = TRUE;
        status = setsockopt(    sock,
                                SOL_SOCKET,
                                SO_KEEPALIVE,
                                (char *) &optval,
                                sizeof(optval));
        if(status<0){
                printf ("iocCAPutLogClient: %s\n", strerror(errno));
		close(sock);
                return ERROR;
        }

	/*
	 * set how long we will wait for the TCP state machine
	 * to clean up when we issue a close(). This
	 * guarantees that messages are serialized when we
	 * switch connections.
	 */
	{
		struct  linger		lingerval;

		lingerval.l_onoff = TRUE;
		lingerval.l_linger = 60*5; 
		status = setsockopt(    sock,
					SOL_SOCKET,
					SO_LINGER,
					(char *) &lingerval,
					sizeof(lingerval));
		if(status<0){
			printf ("iocCAPutLogClient: %s\n", strerror(errno));
			close(sock);
			return ERROR;
		}
	}

	fp = fdopen (sock, "a");

	/*
	 * mutex on
	 */
	status = semTake(iocCAPutLogMutex, WAIT_FOREVER);
	assert(status==OK);
	/*
	 * close any preexisting connection to the log server
	 */
	if (iocCAPutLogFile) {
/***		logFdDelete(iocCAPutLogFD);  ***/
		fclose(iocCAPutLogFile);
		iocCAPutLogFile = NULL;
		iocCAPutLogFD = ERROR;
	}
	else if (iocCAPutLogFD!=ERROR) {
/***		logFdDelete(iocCAPutLogFD);   ***/
		close(iocCAPutLogFD);
		iocCAPutLogFD = ERROR;
	}

	/*
	 * export the new connection
	 */
	iocCAPutLogFD = sock;
/***	logFdAdd (iocCAPutLogFD); ***/
	iocCAPutLogFile = fp;

	/*
	 * mutex off
	 */
	status = semGive(iocCAPutLogMutex);
	assert(status==OK);
	return OK;
}


/*
 * logRestart()
 */
LOCAL void logRestart(void)
{
	int 	status;
	int	reattach;
	int	delay = LOG_RESTART_DELAY;	


	/*
	 * roll the local port forward so that we dont collide
	 * with the first port assigned when we reboot 
	 */
	logClientRollLocalPort();

	while (1) {
		semTake(iocCAPutLogSignal, delay);

		/*
		 * mutex on
		 */
		status = semTake(iocCAPutLogMutex, WAIT_FOREVER);
		assert(status==OK);
		if (iocCAPutLogFile==NULL) {
			reattach = TRUE;
		}
		else {
			reattach = ferror(iocCAPutLogFile);
		}

		/*
		 * mutex off
		 */
		status = semGive(iocCAPutLogMutex);
		assert(status==OK);
		if (reattach==FALSE) {
			continue;
		}

		/*
		 * restart log server
		 */
		iocCAPutLogConnectCount = 0U;
		logClientRollLocalPort();
	}
}


/*
 * logClientRollLocalPort()
 */
LOCAL void logClientRollLocalPort(void)
{
	int	status;

	/*
	 * roll the local port forward so that we dont collide
	 * with it when we reboot
	 */
	while (iocCAPutLogConnectCount<10U) {
		/*
		 * switch to a new log server connection 
		 */
		status = iocCAPutLogAttach();
		if (status==OK) {
			/*
			 * only print a message after the first connect
			 */
			if (iocCAPutLogConnectCount==1U) {
				printf(
		"iocCAPutLogClient: reconnected to the log server\n");
			}
		}
		else {
			/*
			 * if we cant connect then we will roll
			 * the port later when we can
			 * (we must not spin on connect fail)
			 */
			if (errno!=ETIMEDOUT) {
				return;
			}
		}
	}
}


/*
 * logClientShutdown() is needed also if CA Put Log Client
 * is terminated when CA Put Logging is disabled
 */
LOCAL void logClientShutdown(void)
{
	if (iocCAPutLogFD!=ERROR) {
	/*
	 * unfortunately this does not currently work because WRS
	 * runs the reboot hooks in the order that
	 * they are installed (and the network is already shutdown 
	 * by the time we get here)
	 */
		errlogPrintf("logClientShutdown routine entered\n");

#if 0
		/*
		 * this aborts the connection because we 
		 * have specified a nill linger interval
		 */
		printf("CA log client: lingering for connection close...");
		close(iocCAPutLogFD);
		printf("done\n");
#endif 
	}	
}


/*
 *
 *	getConfig()
 *	Get Server Configuration
 *
 *
 */
LOCAL int getConfig(void)
{
	long	status;
	
/********* changed by V.Korobov 25.06.98 ***********

	status = envGetLongConfigParam(
			&EPICS_IOC_LOG_PORT, 
			&ioc_log_port);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_PORT);
		return ERROR;
	}

	status = envGetInetAddrConfigParam(
			&EPICS_IOC_LOG_INET, 
			&ioc_log_addr);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_INET);
		return ERROR;
	}
**********************************/

   char *caPutLogPort;
   char *caPutLogInet;
   struct in_addr iocLogInet;
   
   	caPutLogPort = getenv("EPICS_IOC_CA_PUT_LOG_PORT");

	if ((!caPutLogPort)||(!*caPutLogPort)) 	/* If no env var set default */
	   ioc_log_port = EPICS_IOC_CA_PUT_LOG_PORT;
	else 
	   ioc_log_port = atol(caPutLogPort);
	   
#if DEBUG
   printf("CAPutLogClient: getConfig: ioc_log_port = %ld\n", ioc_log_port);
#endif
	   
	caPutLogInet = getenv("EPICS_IOC_CA_PUT_LOG_INET");
	
	if ((!caPutLogInet)||(!*caPutLogInet)) {	/* If no env var use one for iocLogClient */
	   status = envGetInetAddrConfigParam(
			&EPICS_IOC_LOG_INET, 
			&iocLogInet);
	   if (status < 0) {
	      errlogPrintf("CAPutLogClient: getConfig: EPICS_IOC_CA_PUT_LOG_INET environment variable is not defined\n", 0,0,0,0,0,0);
	      return(ERROR);
	   }
	   else
	      ioc_log_addr.s_addr = iocLogInet.s_addr;
	}
	else {
	   ioc_log_addr.s_addr = inet_addr(caPutLogInet);
	}
	   
#if DEBUG
   printf("CAPutLogClient: getConfig: INET = '%s' ioc_log_addr = %lx\n", 
      caPutLogInet, ioc_log_addr.s_addr);
#endif
	         	   
	return OK;
}



/*
 *	failureNotify()
 */
LOCAL void failureNotify(const ENV_PARAM *pparam)
{
	printf(
	"iocCAPutLogClient: EPICS environment variable \"%s\" undefined\n",
		pparam->name);
}


/*
 * iocCAPutLogVPrintf()
 */
int iocCAPutLogVPrintf(const char *pFormat, va_list pvar)
{
	int status;
	int semStatus;

	if (!pFormat || iocCAPutLogDisable) {
		return 0;
	}

	/*
	 * Check for init 
	 */
	if (!iocCAPutLogMutex) {
		status = iocCAPutLogInit();
		if (status) {
			return 0;
		}
	}

	/*
	 * mutex on
	 */
	semStatus = semTake(iocCAPutLogMutex, WAIT_FOREVER);

	assert(semStatus==OK);
	if (iocCAPutLogFile) {
		status = vfprintf(iocCAPutLogFile, pFormat, pvar);
		if (status>0) {
			status = fflush(iocCAPutLogFile);
		}

		if (status<0) {
/***			logFdDelete(iocCAPutLogFD); ***/
			fclose(iocCAPutLogFile);
			iocCAPutLogFile = NULL;
			iocCAPutLogFD = ERROR;
			semStatus = semGive(iocCAPutLogSignal);
			printf("iocCAPutLogClient: lost contact with the log server\n");
			assert(semStatus==OK);
		}
	}
	else {
		status = EOF;
	}

	/*
	 * mutex off
	 */
	semStatus = semGive(iocCAPutLogMutex);
	assert(semStatus==OK);
	return status;
}


/*
 * iocCAPutLogPrintf()
 */
int iocCAPutLogPrintf(const char *pFormat, ...)
{
	va_list		pvar;

	va_start (pvar, pFormat);

	return iocCAPutLogVPrintf (pFormat, pvar);
}

