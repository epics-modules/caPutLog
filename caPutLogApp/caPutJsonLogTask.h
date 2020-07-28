/* File:     caPutJsonLogTask.h
 * Author:   Matic Pogacnik
 * Created:  21.07.2020
 *
 * Contains definitions of a CaPutJsonLogTask which is capable of logging CA put
 * changes to a remote server and a PV in a JSON format.
 *
 * Modification log:
 * ----------------
 * v 1.0
 * - Initial version
*/

#ifndef INCcaPutJsonLogh
#define INCcaPutJsonLogh 1

// Standard library imports
#include <string>
#include <atomic>

// Epics base imports
#include <envDefs.h>
#include <logClient.h>
#include <freeList.h>
#include <asTrapWrite.h>
#include <epicsMessageQueue.h>
#include <dbAddr.h>
#include <epicsThread.h>

// This module imports
#include "caPutLogTask.h"

// Status return values
#define caPutJsonLogSuccess     0
#define caPutJsonLogError       -1

// For parameter 'config' of caPutJsonLogInit and caPutJsonLogReconfigure
#define caPutJsonLogNone        -1  /* no logging (disable) */
#define caPutJsonLogOnChange    0   /* log only on value change */
#define caPutJsonLogAll         1   /* log all puts */
#define caPutJsonLogAllNoFilter 2   /* log all puts no filtering on same PV*/

#define FREE_LIST_SIZE 1000

#ifdef __cplusplus

class CaPutJsonLogTask {
public:
    /*
     * Data
     */
    static const int default_port = 7011;


    /*
     * Methods
     */
    // Class methods
    static CaPutJsonLogTask* getInstance() noexcept;
    int initialize(const char* address, int config);
    void addPutToQueue(LOGDATA * plogData);
    void caPutJsonLogTask(void *arg); //Must be public, called from C

    int start();
    int stop();

    // Logger config methods
    int reconfigure(int config);
    int report(int level);

private:
    /*
     * Data
     */
    // Class variables
    static CaPutJsonLogTask *instance;

    // Logger configuration
    const char * address;
    std::atomic_int config;

    // Interthread communication
    epicsMessageQueue caPutJsonLogQ;

    // Working thread
    epicsThreadId threadId;
    std::atomic_bool taskStopper;

    //Logging to a server
    logClientId caPutJsonLogClient;

    // Logging to a PV
    DBADDR caPutJsonLogPV;
    DBADDR *pCaPutJsonLogPV;

    /*
     * Methods
     */
    // Class methods (Do not allow public constructors - class is designed as singleton)
    CaPutJsonLogTask();
    virtual ~CaPutJsonLogTask();
    CaPutJsonLogTask(const CaPutJsonLogTask&);
    CaPutJsonLogTask(const CaPutJsonLogTask&&);

    // Logger logic
    int buildJsonMsg(const VALUE *pold_value, const LOGDATA *pLogData,
            int burst, const VALUE *pmin, const VALUE *pmax, int config);

    // Logging to a server
    int configureServerLogging(const char* address);
    void logToServer(std::string &msg);

    // Logging to a PV
    int configurePvLogging();
    void logToPV(std::string &msg);

    // Logger helper methods
    void calculateMin(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);
    void calculateMax(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);
    bool compareValue(const VALUE *pa, const VALUE *pb, short type);
    int  fieldVal2Str(char *pbuf, size_t buflen, const VALUE *pval, short type, int index);
};


extern "C" {
#endif /*__cplusplus */

/*
 * C interface functions (Called from base)
 */
void caddPutToQueue(LOGDATA * plogData);
void caPutJsonLogWorker(void *arg);
void caPutJsonLogExit(void *arg);

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /* INCcaPutJsonLogh */
