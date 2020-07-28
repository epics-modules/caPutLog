#ifndef INCcaPutLogAsh
#define INCcaPutLogAsh 1

#include <shareLib.h>

#include "caPutLogTask.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int caPutLogAsInit(void (*sendCallback)(LOGDATA *), void (*stopCallback)());
epicsShareFunc void caPutLogAsStop();
epicsShareFunc void caPutLogDataFree(LOGDATA *pLogData);
epicsShareFunc LOGDATA* caPutLogDataCalloc(void);

int caPutLogMaxArraySize(short type);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogAsh*/
