#ifndef INCcaPutLogAsh
#define INCcaPutLogAsh 1

#include <shareLib.h>

#include "caPutLogTask.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int caPutLogAsInit();
epicsShareFunc void caPutLogAsStop();
epicsShareFunc void caPutLogDataFree(LOGDATA *pLogData);
epicsShareFunc LOGDATA* caPutLogDataCalloc(void);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogAsh*/
