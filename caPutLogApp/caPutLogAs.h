#ifndef INCcaPutLogAsh
#define INCcaPutLogAsh 1

#include <shareLib.h>
#include <dbAddr.h>

#include "caPutLogTask.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int caPutLogAsInit(void (*sendCallback)(LOGDATA *), void (*stopCallback)());
epicsShareFunc void caPutLogAsStop();
epicsShareFunc void caPutLogDataFree(LOGDATA *pLogData);
epicsShareFunc LOGDATA* caPutLogDataCalloc(void);

epicsShareFunc int caPutLogMaxArraySize(short type);
epicsShareFunc long caPutLogActualArraySize(dbAddr * paddr);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogAsh*/
