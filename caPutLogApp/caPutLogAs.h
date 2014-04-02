#ifndef INCcaPutLogAsh
#define INCcaPutLogAsh 1

#include <shareLib.h>

#include "caPutLogTask.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI caPutLogAsInit();
epicsShareFunc void epicsShareAPI caPutLogAsStop();
epicsShareFunc void epicsShareAPI caPutLogDataFree(LOGDATA *pLogData);
epicsShareFunc LOGDATA* epicsShareAPI caPutLogDataCalloc(void);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogAsh*/
