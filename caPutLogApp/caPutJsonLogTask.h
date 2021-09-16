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

// Epics base imports
#include <logClient.h>
#include <epicsMessageQueue.h>
#include <dbAddr.h>
#include <epicsThread.h>

// Includes from this module
#include "caPutLogTask.h"

// Status return values
enum caPutJsonLogStatus {
    caPutJsonLogSuccess = 0,
    caPutJsonLogError   = -1
};

// Configuration options
enum caPutJsonLogConfig {
    caPutJsonLogNone        = -1, /* no logging (disable) */
    caPutJsonLogOnChange    =  0, /* log only on value change */
    caPutJsonLogAll         =  1, /* log all puts */
    caPutJsonLogAllNoFilter =  2  /* log all puts no filtering on same PV*/
};

enum specialValues {
    svNormal,
    svNan,
    svNinf,
    svPinf
};

#ifdef __cplusplus

/**
 * @brief Implementation of EPICS Channel Access put logger. Log messages are produced
 * in JSON format:
 * \code{.txt}
 * <iocLogPrefix>{"date": "<dd>-<mm>-<yyyy>"","time":"<hh>:<mm>:<ss>","host":"<client hostname>","user":"<client username>","pv":"<pv name>","new":<new value>,"old":"<old value>"}\n
 * \endcode
 * Burst puts add extra JSON properties (supported only for numberic scalar puts):
 *  - "min": Which is a minimum value inside the burst period
 *  - "max": Represents maximum value inside the burst of puts
 * Array puts add following JSON properties:
 *  - "new-size": new array length of array (in case of a lso/lsi record this is string length)
 *  - "old-size": old array length of array (in case of a lso/lsi record this is string length)
 *
 * Implementation registers trap inside the Access security trap via "caPutLogAs.h" interface. After each caput our
 * callback method is called which add a `LOGDATA` structure to the queue. On the logger thread we take the messages and
 * check fo the bursts (with timeout). When all required information is known we generate JSON messsage, which is then
 * send to the logging server and PV if configured.
 *
 * This class is designed as a singleton object.
 *
 * For the instructions how to use the utility please refer to the user manual.
 *
 */
class epicsShareClass CaPutJsonLogTask {
public:

    // Default port to be used if not specified by the user
    static const int default_port = 7011;

    /**
     * @brief Get the singleton Instance object.
     *
     * @return CaPutJsonLogTask* Pointer to the only instance of the CaPutJsonLogTask object.
     */
    static CaPutJsonLogTask* getInstance();

    /**
     * @brief Initialize the object.
     *
     * @param address IP address or hostname of the log server. Can include a port number after a colon,
     *           if port number is not specified, default value will be used.
     * @param config Configuration paramteter. Valid value are -1 <= config <= 2.
     * @return caPutJsonLogStatus Status code.
     */
    caPutJsonLogStatus initialize(const char* address, caPutJsonLogConfig config);

    /**
     * @brief Add a put details packed as a ::LOGDATA structure to a queue for processing.
     *      This method registered as a callback inside the caPutLog.h.
     *
     * @param plogData ::LOGDATA structure holding details about caput.
     */
    void addPutToQueue(LOGDATA * plogData);

    /**
     * @brief Main loop of the logger. This method is started in a separate thread.
     *      It takes a caput messages from a queue process them and calls buildJsonMsg().
     *
     * @param arg Parameter is not used. Could be in the feature.
     */
    void caPutJsonLogTask(void *arg); //Must be public, called from C

    /**
     * @brief Start the logging.
     *
     * @return int Status code.
     */
    caPutJsonLogStatus start();

    /**
     * @brief Stop the logging.
     *
     * @return int Status code.
     */
    caPutJsonLogStatus stop();

    /**
     * @brief Reconfigure the logging.
     *
     * @param config New configuration. Valid value are -1 <= config <= 2. Invalid
     * value will default to 1 "caPutJsonLogAll".
     * @return int Status code.
     */
    caPutJsonLogStatus reconfigure(caPutJsonLogConfig config);

    /**
     * @brief Print report client logger information to the console.
     *
     * @param level Level of details to be printed.
     * @return int Status code.
     */
    caPutJsonLogStatus report(int level);

private:

    // Singelton instance of this class.
    static CaPutJsonLogTask *instance;

    // Logger configuration
    int config; // To modify or read this value only epicsAtomic methods should be used

    // Interthread communication
    epicsMessageQueue caPutJsonLogQ;

    // Working thread
    epicsThreadId threadId;
    int taskStopper; // To modify or read this value only epicsAtomic methods should be used

    //Logging to a list of servers
    struct clientItem {
        logClientId caPutJsonLogClient;
        struct clientItem *next;
        char address[1];
    } *clients;
    epicsMutex clientsMutex;

    // Logging to a PV
    DBADDR caPutJsonLogPV;
    DBADDR *pCaPutJsonLogPV;


    // Class methods (Do not allow public constructors - class is designed as singleton)
    CaPutJsonLogTask();
    virtual ~CaPutJsonLogTask();
    CaPutJsonLogTask(const CaPutJsonLogTask&);

    // Commeted as move constructor is c++11 feature, but we want compile on older versions as well
    // CaPutJsonLogTask(const CaPutJsonLogTask&&);

    /**
     * @brief Build a JSON string from and call logToServer() and logToPV() methods to log a message.
     *
     * @param pold_value Pointer to a ::VALUE structure holding an old PV value.
     * @param pLogData Pointer to a ::LOGDATA structure holding new value and other meta databa about the put.
     * @param burst Boolean value. It determinate if put was a burst of values.
     * @param pmin Pointer to a ::VALUE structure holding an min value if burst is true.
     * @param pmax Pointer to a ::VALUE structure holding an max value if burst is true.
     * @return int Status code.
     */
    caPutJsonLogStatus buildJsonMsg(const VALUE *pold_value, const LOGDATA *pLogData,
            bool burst, const VALUE *pmin, const VALUE *pmax);

    /**
     * @brief Configure logging to a server.
     *
     * @param address IP address or hostname of the logging server.
     * @return int Status code.
     */
    caPutJsonLogStatus configureServerLogging(const char* address);

    /**
     * @brief This method will send a message to the configured log server.
     *
     * @param msg Message to be send.
     */
    void logToServer(std::string &msg);

    /**
     * @brief Configure logging to a PV.
     *
     * @return int Status code.
     */
    caPutJsonLogStatus configurePvLogging();

    /**
     * @brief Method will write a message to the configured PV.
     *
     * @param msg Message to be written.
     */
    void logToPV(std::string &msg);

    /**
     * @brief Calculate minimum value of two ::VALUE unions.
     *
     * @param pres ::VALUE structure where result should be written.
     * @param pa ::VALUE structure of the first value to be compared.
     * @param pb ::VALUE structure of the second value to be compared.
     * @param type EPICS DRB_* type stored in the input structures.
     */
    void calculateMin(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);

    /**
     * @brief Calculate maximum value of two ::VALUE unions.
     *
     * @param pres ::VALUE structure where result should be written.
     * @param pa ::VALUE structure of the first value to be compared.
     * @param pb ::VALUE structure of the second value to be compared.
     * @param type EPICS DRB_* type stored in the input structures.
     */
    void calculateMax(VALUE *pres, const VALUE *pa, const VALUE *pb, short type);

    /**
     * @brief Compare values in two ::VALUE structures if they are the same.
     *
     * @param pa First ::VALUE structure to be compared.
     * @param pb Second ::VALUE structure to be compared.
     * @param type EPICS DRB_* type stored in the input structures.
     * @return true If values are the same.
     * @return false  If values are not the same.
     */
    bool compareValue(const VALUE *pa, const VALUE *pb, short type);

    /**
     * @brief Get a string representation of the value stored in the ::VALUE.
     *
     * @param pbuf Pointer to a character buffer where the output should be written.
     * @param buflen Length of the `pbuf`.
     * @param pval Pointer to the ::VALUE structure to be transformed to the string representation.
     * @param type EPICS DRB_* type stored in the input structure.
     * @param index Index of the element in case of an array. For scalar values this must be 0.
     * @return int Status return code.
     */
    int  fieldVal2Str(char *pbuf, size_t buflen, const VALUE *pval, short type, int index);

    /**
     * @brief Check for special value (nan, positive/negative infinity) in ::VALUE structure.
     *
     * @param pval ::VALUE to be checked.
     * @param type EPICS DRB_* type stored in the input ::VALUE.
     * @param index Index of the element in case of an array. For scalar values this must be 0.
     * @return specialValues
     */
    specialValues testForSpecialValues(const VALUE *pval, short type, int index);

};


extern "C" {
#endif /*__cplusplus */

/*
 * C interface functions (Called from base)
 */
epicsShareFunc void caddPutToQueue(LOGDATA * plogData);
epicsShareFunc void caPutJsonLogWorker(void *arg);
epicsShareFunc void caPutJsonLogExit(void *arg);
epicsShareExtern int caPutLogJsonMsgQueueSize;
#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /* INCcaPutJsonLogh */
