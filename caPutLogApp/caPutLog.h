#ifndef INCcaPutLogh
#define INCcaPutLogh 1

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* status return values */
#define caPutLogSuccess     0
#define caPutLogError       -1

/* for parameter 'config' of caPutLogInit and caPutLogReconfigure */
#define caPutLogNone        -1  /* no logging (disable) */
#define caPutLogOnChange    0   /* log only on value change */
#define caPutLogAll         1   /* log all puts */
#define caPutLogAllNoFilter 2   /* log all puts no filtering on same PV*/

epicsShareFunc int caPutLogInit (const char *addr_str, int config);
epicsShareFunc int caPutLogReconf (int config);
epicsShareFunc void caPutLogShow (int level);
epicsShareFunc void caPutLogSetTimeFmt (const char *format);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogh*/
