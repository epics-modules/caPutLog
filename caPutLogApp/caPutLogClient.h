#ifndef INCcaPutLogClienth
#define INCcaPutLogClienth 1

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI caPutLogClientInit (const char *addr_str);
epicsShareFunc void epicsShareAPI caPutLogClientShow (unsigned level);
epicsShareFunc void epicsShareAPI caPutLogClientFlush ();
epicsShareFunc void epicsShareAPI caPutLogClientSend (const char *message);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogClienth*/
