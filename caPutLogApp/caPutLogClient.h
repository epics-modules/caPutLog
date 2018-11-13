#ifndef INCcaPutLogClienth
#define INCcaPutLogClienth 1

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int caPutLogClientInit (const char *addr_str);
epicsShareFunc void caPutLogClientShow (unsigned level);
epicsShareFunc void caPutLogClientFlush ();
epicsShareFunc void caPutLogClientSend (const char *message);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogClienth*/
