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

#include <errlog.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "caPutJsonLogTask.h"

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
    static const iocshFuncDef caPutjsonLogInitDef = {"caPutJsonLogInit", 3, caPutJsonLogInitArgs};
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

    /* Register IOCsh commands */
    static void caPutJsonLogRegister(void)
    {
        static int done = FALSE;
        if(done) return;
        done = TRUE;

        iocshRegister(&caPutjsonLogInitDef,caPutJsonLogInitCall);
        iocshRegister(&caPutJsonLogReconfDef,caPutJsonLogReconfCall);
        iocshRegister(&caPutJsonLogShowDef,caPutJsonLogShowCall);
        iocshRegister(&caPutJsonLogSetBurstTimeoutDef,caPutJsonLogSetBurstTimeoutCall);
    }
    epicsExportRegistrar(caPutJsonLogRegister);

    epicsExportAddress(int,caPutLogJsonMsgQueueSize);
}
