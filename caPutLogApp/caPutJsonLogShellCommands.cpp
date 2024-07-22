/* File:     caPutJsonLogShellCommands.cpp
 * Author:   Matic Pogacnik
 * Created:  21.07.2020
 *
 * Contains code for a IOC shell configuration commands of the
 * JSON CA put logger implementation.
 *
 * Modification log:
 * ----------------
 * v 1.0
 * - Initial version
*/

#include <stdio.h>
#include <errlog.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <string>

#include "caPutJsonLogTask.h"

// Use colored ERROR/WARNING text if available
#ifndef ERL_ERROR
#  define ERL_ERROR "ERROR"
#  define ERL_WARNING "WARNING"
#endif

/* EPICS iocsh shell commands */
extern "C"
{
    extern int caPutLogJsonMsgQueueSize;

    /* Initalisation */
    int caPutJsonLogInit(const char * address, caPutJsonLogConfig config, double timeout){
        CaPutJsonLogTask *logger =  CaPutJsonLogTask::getInstance();
        if (logger != NULL) return logger->initialize(address, config, timeout);
        else return -1;
    }

    static const iocshArg caPutJsonLogInitArg0 = {"address", iocshArgString};
    static const iocshArg caPutJsonLogInitArg1 = {"config", iocshArgInt};
    static const iocshArg caPutJsonLogInitArg2 = {"burst timeout", iocshArgDouble};
    static const iocshArg *const caPutJsonLogInitArgs[] = {
        &caPutJsonLogInitArg0,
        &caPutJsonLogInitArg1,
        &caPutJsonLogInitArg2
    };
    static const iocshFuncDef caPutJsonLogInitDef = {"caPutJsonLogInit", 3, caPutJsonLogInitArgs};
    static void caPutJsonLogInitCall(const iocshArgBuf *args)
    {
        caPutJsonLogInit(args[0].sval, static_cast<caPutJsonLogConfig>(args[1].ival), args[2].dval);
    }


    /* Reconfigure */
    int caPutJsonLogReconf(caPutJsonLogConfig config, double timeout){
        CaPutJsonLogTask *logger =  CaPutJsonLogTask::getInstance();
        if (logger != NULL)  return logger->reconfigure(config, timeout);
        else return -1;
    }

    static const iocshArg caPutJsonLogReconfArg0 = {"config", iocshArgInt};
    static const iocshArg caPutJsonLogReconfArg1 = {"burst timeout", iocshArgDouble};
    static const iocshArg *const caPutJsonLogReconfArgs[] = {
        &caPutJsonLogReconfArg0,
        &caPutJsonLogReconfArg1
    };
    static const iocshFuncDef caPutJsonLogReconfDef = {"caPutJsonLogReconf", 1, caPutJsonLogReconfArgs};
    static void caPutJsonLogReconfCall(const iocshArgBuf *args)
    {
        caPutJsonLogReconf(static_cast<caPutJsonLogConfig>(args[0].ival), args[1].dval);
    }

    /* Report */
    int caPutJsonLogShow(int level){
        CaPutJsonLogTask *logger =  CaPutJsonLogTask::getInstance();
        if (logger != NULL)  return logger->report(level);
        else return -1;
    }

    static const iocshArg caPutJsonLogShowArg0 = {"level", iocshArgInt};
    static const iocshArg *const caPutJsonLogShowArgs[] = {
        &caPutJsonLogShowArg0
    };
    static const iocshFuncDef caPutJsonLogShowDef = {"caPutJsonLogShow", 1, caPutJsonLogShowArgs};
    static void caPutJsonLogShowCall(const iocshArgBuf *args)
    {
        caPutJsonLogShow(args[0].ival);
    }

    /* Error message if caPutLogInit used */
    static const iocshFuncDef caPutLogInitDef = {"caPutLogInit", 0, NULL};
    static void caPutLogInitCall(const iocshArgBuf *args)
    {
        fprintf(stderr, ERL_ERROR
            ": The caPutLog module is configured for JSON put-logging,\n"
            "  only caPutJsonLog* commands are available. To use the plain\n"
            "  put-log format rebuild this IOC with 'caPutLog.dbd' instead\n"
            "  of 'caPutJsonLog.dbd'.\n");
        #ifdef IOCSHFUNCDEF_HAS_USAGE
            iocshSetError(-1);
        #endif
    }

    /* Metadata */
    int caPutJsonLogAddMetadata(const char *property, const char *value) {
        CaPutJsonLogTask *logger = CaPutJsonLogTask::getInstance();
        if (logger)
            return logger->addMetadata(property, value);
        return -1;
    }
    static const iocshArg caPutJsonLogAddMetadataArg0 = {"property", iocshArgString};
    static const iocshArg caPutJsonLogAddMetadataArg1 = {"value", iocshArgString};
    static const iocshArg *const caPutJsonLogAddMetadataArgs[] =
    {
        &caPutJsonLogAddMetadataArg0,
        &caPutJsonLogAddMetadataArg1
    };
    static const iocshFuncDef caPutJsonLogAddMetadataDef = {"caPutJsonLogAddMetadata", 2, caPutJsonLogAddMetadataArgs};
    static void caPutJsonLogAddMetadataCall(const iocshArgBuf *args)
    {
        caPutJsonLogAddMetadata(args[0].sval, args[1].sval);
    }

    /* Change burst filter timeout */
    int caPutJsonLogSetBurstTimeout(double timeout){
        CaPutJsonLogTask *logger =  CaPutJsonLogTask::getInstance();
        if (logger != NULL)  return logger->setBurstTimeout(timeout);
        else return -1;
    }

    static const iocshArg caPutJsonLogSetBurstTimeoutArg0 = {"burst timeout", iocshArgDouble};
    static const iocshArg *const caPutJsonLogSetBurstTimeoutArgs[] = {
        &caPutJsonLogSetBurstTimeoutArg0
    };
    static const iocshFuncDef caPutJsonLogSetBurstTimeoutDef = {"caPutJsonLogSetBurstTimeout", 1, caPutJsonLogSetBurstTimeoutArgs};
    static void caPutJsonLogSetBurstTimeoutCall(const iocshArgBuf *args)
    {
        caPutJsonLogSetBurstTimeout(args[0].dval);
    }

    /* Register JSON IOCsh commands */
    static void caPutJsonLogRegister(void)
    {
        extern int caPutLogRegisterDone;

        switch (caPutLogRegisterDone) {
        case 0:
            iocshRegister(&caPutJsonLogInitDef,caPutJsonLogInitCall);
            iocshRegister(&caPutJsonLogReconfDef,caPutJsonLogReconfCall);
            iocshRegister(&caPutJsonLogShowDef,caPutJsonLogShowCall);
            iocshRegister(&caPutLogInitDef,caPutLogInitCall);
            iocshRegister(&caPutJsonLogAddMetadataDef,caPutJsonLogAddMetadataCall);
            iocshRegister(&caPutJsonLogSetBurstTimeoutDef,caPutJsonLogSetBurstTimeoutCall);
            caPutLogRegisterDone = 2;
            break;

        case 1:
            errlogPrintf(ERL_WARNING
                ": Registration of caPutJsonLog commands skipped, as\n"
                "  the caPutLog commands have already been registered.\n"
                "  This IOC may have been built with both 'caPutJsonLog.dbd'\n"
                "  and 'caPutLog.dbd', please rebuild it using only one.\n");
            break;

        case 2:
            // Second registration, no problem
            break;
        }
    }
    epicsExportRegistrar(caPutJsonLogRegister);

    epicsExportAddress(int,caPutLogJsonMsgQueueSize);
}
