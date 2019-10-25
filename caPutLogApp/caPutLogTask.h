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
    epicsInt8       v_int8;
    epicsUInt8      v_uint8;
    epicsInt16      v_int16;
    epicsUInt16     v_uint16;
    epicsInt32      v_int32;
    epicsUInt32     v_uint32;
#ifdef DBR_INT64
    epicsInt64      v_int64;
    epicsUInt64     v_uint64;
#endif
    epicsFloat32    v_float;
    epicsFloat64    v_double;
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

epicsShareFunc int caPutLogTaskStart(int config);
epicsShareFunc void caPutLogTaskStop(void);
epicsShareFunc void caPutLogTaskSend(LOGDATA *plogData);
epicsShareFunc int epicsShareAPI caPutLogVALUEToString(char *pbuf, size_t buflen, const VALUE *pval, short type);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogTaskh*/
