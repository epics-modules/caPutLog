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

#if JSON_AND_ARRAYS_SUPPORTED
#define MAX_ARRAY_SIZE_BYTES 400
#else
#define MAX_ARRAY_SIZE_BYTES 0
#endif

#define DEFAULT_BURST_TIMEOUT 5.0

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

#if JSON_AND_ARRAYS_SUPPORTED
    epicsInt8       a_int8[MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt8)];
    epicsUInt8      a_uint8[MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt8)];
    epicsInt16      a_int16[MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt16)];
    epicsUInt16     a_uint16[MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt16)];
    epicsInt32      a_int32[MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt32)];
    epicsUInt32     a_uint32[MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt32)];
    epicsInt64      a_int64[MAX_ARRAY_SIZE_BYTES/sizeof(epicsInt64)];
    epicsUInt64     a_uint64[MAX_ARRAY_SIZE_BYTES/sizeof(epicsUInt64)];
    epicsFloat32    a_float[MAX_ARRAY_SIZE_BYTES/sizeof(epicsFloat32)];
    epicsFloat64    a_double[MAX_ARRAY_SIZE_BYTES/sizeof(epicsFloat64)];
    char            a_string[MAX_ARRAY_SIZE_BYTES/MAX_STRING_SIZE][MAX_STRING_SIZE];
#endif
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
    int is_array;
    int old_size;
    int old_log_size;
    int new_size;
    int new_log_size;
} LOGDATA;

epicsShareFunc int caPutLogTaskStart(int config, double timeout);
epicsShareFunc void caPutLogTaskStop(void);
epicsShareFunc void caPutLogTaskSend(LOGDATA *plogData);
epicsShareFunc void caPutLogTaskShow(void);

#ifdef __cplusplus
}
#endif

#endif /*INCcaPutLogTaskh*/
