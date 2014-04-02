#ifndef INCcaPutLogTaskh
#define INCcaPutLogTaskh 1

#include <shareLib.h>
#include <dbDefs.h>
#include <epicsTime.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_USERID_SIZE 32
#define MAX_HOSTID_SIZE 32

typedef union {
    char            v_char;
    unsigned char   v_uchar;
    short           v_short;
    unsigned short  v_ushort;
    long            v_long;
    unsigned long   v_ulong;
    float           v_float;
    double          v_double;
    char            v_string[MAX_STRING_SIZE];
} VALUE;

typedef struct {
    char            userid[MAX_USERID_SIZE];
    char            hostid[MAX_HOSTID_SIZE];
    char            pv_name[PVNAME_STRINGSZ];
    void            *pfield;
    short           type;
    VALUE           old_value;
    struct {
        TS_STAMP    time;
        VALUE       value;
    }               new_value;
} LOGDATA;

epicsShareFunc int epicsShareAPI caPutLogTaskStart(int config);
epicsShareFunc void epicsShareAPI caPutLogTaskStop(void);
epicsShareFunc void epicsShareAPI caPutLogTaskSend(LOGDATA *plogData);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogTaskh*/
