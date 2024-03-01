#include <stdio.h>

#include <dbDefs.h>
#include <errlog.h>
#include <iocsh.h>
#include <epicsExport.h>

#include "caPutLog.h"

/* Use colored ERROR/WARNING text if available */
#ifndef ERL_ERROR
#  define ERL_ERROR "ERROR"
#  define ERL_WARNING "WARNING"
#endif

static const iocshArg caPutLogInitArg0 = {"address", iocshArgString};
static const iocshArg caPutLogInitArg1 = {"config", iocshArgInt};
static const iocshArg *const caPutLogInitArgs[] = {
    &caPutLogInitArg0,
    &caPutLogInitArg1
};
static const iocshFuncDef caPutLogInitDef = {"caPutLogInit", 2, caPutLogInitArgs};
static void caPutLogInitCall(const iocshArgBuf *args)
{
    caPutLogInit(args[0].sval, args[1].ival);
}

static const iocshArg caPutLogReconfArg0 = {"config", iocshArgInt};
static const iocshArg *const caPutLogReconfArgs[] = {
    &caPutLogReconfArg0
};
static const iocshFuncDef caPutLogReconfDef = {"caPutLogReconf", 1, caPutLogReconfArgs};
static void caPutLogReconfCall(const iocshArgBuf *args)
{
    caPutLogReconf(args[0].ival);
}

static const iocshArg caPutLogShowArg0 = {"level", iocshArgInt};
static const iocshArg *const caPutLogShowArgs[] = {
    &caPutLogShowArg0
};
static const iocshFuncDef caPutLogShowDef = {"caPutLogShow", 1, caPutLogShowArgs};
static void caPutLogShowCall(const iocshArgBuf *args)
{
    caPutLogShow(args[0].ival);
}

static const iocshArg caPutLogSetTimeFmtArg0 = {"format", iocshArgPersistentString};
static const iocshArg *const caPutLogSetTimeFmtArgs[] = {
    &caPutLogSetTimeFmtArg0
};
static const iocshFuncDef caPutLogSetTimeFmtDef = {"caPutLogSetTimeFmt", 1, caPutLogSetTimeFmtArgs};
static void caPutLogSetTimeFmtCall(const iocshArgBuf *args)
{
    caPutLogSetTimeFmt(args[0].sval);
}

/* Error message if caPutJsonLogInit used */
static const iocshFuncDef caPutJsonLogInitDef = {"caPutJsonLogInit", 0, NULL};
static void caPutJsonLogInitCall(const iocshArgBuf *args)
{
    fprintf(stderr, ERL_ERROR
        ": The caPutLog module is configured for plain put-logging,\n"
        "  only caPutLog* commands are available. To use the new JSON\n"
        "  put-log format rebuild this IOC with 'caPutJsonLog.dbd'\n"
        "  instead of 'caPutLog.dbd'.\n");
    #ifdef IOCSHFUNCDEF_HAS_USAGE
        iocshSetError(-1);
    #endif
}

static void caPutLogRegister(void)
{
    extern int caPutLogRegisterDone;

    switch (caPutLogRegisterDone) {
    case 0:
        iocshRegister(&caPutLogInitDef,caPutLogInitCall);
        iocshRegister(&caPutLogReconfDef,caPutLogReconfCall);
        iocshRegister(&caPutLogShowDef,caPutLogShowCall);
        iocshRegister(&caPutLogSetTimeFmtDef,caPutLogSetTimeFmtCall);
        iocshRegister(&caPutJsonLogInitDef,caPutJsonLogInitCall);
        caPutLogRegisterDone = 1;
        break;

    case 1:
        /* Second registration, no problem */
        break;

    case 2:
        errlogPrintf(ERL_WARNING
            ": Registration of caPutLog commands skipped, as\n"
            "  the caPutJsonLog commands have already been registered.\n"
            "  This IOC may have been built with both 'caPutJsonLog.dbd'\n"
            "  and 'caPutLog.dbd', please rebuild it using only one.\n");
        break;
    }
}
epicsExportRegistrar(caPutLogRegister);
