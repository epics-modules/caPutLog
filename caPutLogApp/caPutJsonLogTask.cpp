/* File:     caPutJsonLogTask.cpp
 * Author:   Matic Pogacnik
 * Created:  21.07.2020
 *
 * Implementation of a CaPutJsonLogTask class.
 * For more information refer to the header file.
 *
 * Modification log:
 * ----------------
 * v 1.0
 * - Initial version.
*/

// Standard library imports
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

#ifdef _WIN32
#define strtok_r strtok_s
#endif

// Epics Base imports
#include <epicsStdio.h>
#include <envDefs.h>
#include <epicsString.h>
#include <asLib.h>
#include <logClient.h>
#include <epicsThread.h>
#include <epicsAtomic.h>
#include <dbAccessDefs.h>
#include <epicsMath.h>
#include <epicsExit.h>
#include <cantProceed.h>
#include <yajl_gen.h>

// This module imports
#define epicsExportSharedSymbols
#include "caPutLogAs.h"
#include "caPutLogTask.h"
#include "caPutJsonLogTask.h"


#define isDbrNumeric(type) ((type) > DBR_STRING && (type) <= DBR_ENUM)

int caPutLogJsonMsgQueueSize = 1000;
static const ENV_PARAM EPICS_CA_JSON_PUT_LOG_ADDR = {epicsStrDup("EPICS_CA_JSON_PUT_LOG_ADDR"), epicsStrDup("")};

CaPutJsonLogTask * CaPutJsonLogTask::instance = NULL;


CaPutJsonLogTask *CaPutJsonLogTask::getInstance()
{
    if (instance == NULL) {
        try{
            instance = new CaPutJsonLogTask();
        }
        catch (...) {
            errlogSevPrintf(errlogMajor, "caPutJsonLog: Failed to construct CA put JSON logger\n");
            return NULL;
        }
    }
    return instance;
}

CaPutJsonLogTask::CaPutJsonLogTask()
    : caPutJsonLogQ(caPutLogJsonMsgQueueSize, sizeof(LOGDATA *)),
        threadId(NULL),
        taskStopper(false),
        clients(NULL),
        clientsMutex(),
        pCaPutJsonLogPV(NULL)
{ }

CaPutJsonLogTask::~CaPutJsonLogTask()
{
    clientItem *client, *nextclient;
    epicsMutex::guard_t G(clientsMutex);
    nextclient = clients;
    clients = NULL;
    while (nextclient) {
        client = nextclient;
        nextclient = client->next;
        free(client);
    }
}

caPutJsonLogStatus CaPutJsonLogTask::reconfigure(caPutJsonLogConfig config)
{
    if ((config < caPutJsonLogNone)
        || (config > caPutJsonLogAllNoFilter)) {
        errlogSevPrintf(errlogMinor, "caPutJsonLog: invalid config request, setting to default 'caPutJsonLogAll'\n");
        epics::atomic::set(this->config, caPutJsonLogAll);
    } else {
        epics::atomic::set(this->config, config);
    }
    return caPutJsonLogSuccess;
}

caPutJsonLogStatus CaPutJsonLogTask::report(int level)
{

    if (clients != NULL) {
        clientItem *client;
        epicsMutex::guard_t G(clientsMutex);
        for (client = clients; client; client = client->next) {
            logClientShow(client->caPutJsonLogClient, level);
        }
        return caPutJsonLogSuccess;
    }
    else {
        errlogSevPrintf(errlogMinor, "caPutJsonLog: no clients initialised\n");
        return caPutJsonLogError;
    }
}

caPutJsonLogStatus CaPutJsonLogTask::addMetadata(std::string property, std::string value)
{
    std::pair<std::map<std::string, std::string>::iterator, bool> ret;
    ret = metadata.insert(std::pair<std::string, std::string>(property,value));
    if ( ret.second == false ) {
        metadata.erase(property);
        ret = metadata.insert(std::pair<std::string, std::string>(property,value));
        if (ret.second == false) {
            errlogSevPrintf(errlogMinor, "caPutJsonLog: fail to add property %s to json log\n", property.c_str());
            return caPutJsonLogError;
        }
    }
    errlogSevPrintf(errlogInfo, "caPutJsonLog: add property %s with value %s to json log\n", property.c_str(), value.c_str());
    return caPutJsonLogSuccess;
}

bool CaPutJsonLogTask::isMetadataKey(std::string property)
{
    return metadata.count(property) > 0;
}

void CaPutJsonLogTask::removeAllMetadata()
{
    metadata.clear();
}

size_t CaPutJsonLogTask::metadataCount()
{
    return metadata.size();
}

std::map<std::string, std::string> CaPutJsonLogTask::getMetadata()
{
    return metadata;
}

caPutJsonLogStatus CaPutJsonLogTask::initialize(const char* addresslist, caPutJsonLogConfig config)
{
    caPutJsonLogStatus status;

    // Store passed configuration parameters
    this->reconfigure(config);

    // Check if user enabled the logger
    if (config == caPutJsonLogNone) {
        errlogSevPrintf(errlogInfo, "caPutJsonLog: disabled\n");
        return caPutJsonLogSuccess;
    }

    //Configure PV logging
    this->configurePvLogging();

    // Initialize server logging
    if (!addresslist || !addresslist[0]) {
        addresslist = envGetConfigParamPtr(&EPICS_CA_JSON_PUT_LOG_ADDR);
    }
    if (addresslist == NULL) {
        errlogSevPrintf(errlogMajor, "caPutJsonLog: server address not specified\n");
        return caPutJsonLogError;
    }

    char *addresslistcopy1, *addresslistcopy2;
    addresslistcopy2 = addresslistcopy1 = epicsStrDup(addresslist);
    char *saveptr;
    while (true) {
        char *address = strtok_r(addresslistcopy1, " \t\n\r", &saveptr);
        if (!address) break;
        addresslistcopy1 = NULL;
        configureServerLogging(address);
    }
    free(addresslistcopy2);
    if (!clients)
        return caPutJsonLogError;

    // Start logger if not done already
    if (!threadId) {
        status = this->start();
        if (status != caPutJsonLogSuccess) {
            return status;
        }

        // Initialize caPutLogAs
        status = static_cast<caPutJsonLogStatus>(caPutLogAsInit(caddPutToQueue, NULL));
        if (status != caPutJsonLogSuccess) {
            errlogSevPrintf(errlogMinor, "caPutJsonLog: failed to configure Access security\n");
            return caPutJsonLogError;
        }

        // Register exit handler
        epicsAtExit(caPutJsonLogExit, NULL);
    }

    return caPutJsonLogSuccess;
}

caPutJsonLogStatus CaPutJsonLogTask::start()
{
    // Check if Access security is enabled
    if (!asActive) {
        errlogSevPrintf(errlogMajor, "caPutJsonLog: access security disabled, exiting now\n");
        return caPutJsonLogError;
    }

    // Create logging thread
    epics::atomic::set(this->taskStopper,  false);
    const char * threadName = "caPutJsonLog";
    threadId = epicsThreadCreate(threadName,
                                    epicsThreadPriorityLow,
                                    epicsThreadGetStackSize(epicsThreadStackSmall),
                                    (EPICSTHREADFUNC) caPutJsonLogWorker,
                                    NULL);
    if (!threadId) {
        errlogSevPrintf(errlogFatal,"caPutJsonLog: thread creation failed\n");
        return caPutJsonLogError;
    }
    return caPutJsonLogSuccess;
}

caPutJsonLogStatus CaPutJsonLogTask::stop()
{
    // Send signal to stop the logger worker thread
    epics::atomic::set(this->taskStopper,  true);

    // Deregister Access Security trap
    caPutLogAsStop();

    return caPutJsonLogSuccess;
}

caPutJsonLogStatus CaPutJsonLogTask::configureServerLogging(const char* address)
{
    int status;
    struct sockaddr_in saddr;
    clientItem** pclient;

    // Parse the address
    epicsMutex::guard_t G(clientsMutex);
    for (pclient = &clients; *pclient; pclient = &(*pclient)->next) {
        if (strcmp(address, (*pclient)->address) == 0) {
            fprintf (stderr, "caPutJsonLog: address %s already configured\n", address);
            return caPutJsonLogSuccess;
        }
    }

    status = aToIPAddr(address, this->default_port, &saddr);
    if (status < 0) {
        errlogSevPrintf(errlogMajor, "caPutJsonLog: bad address or host name\n");
        return caPutJsonLogError;
    }

    // Create log client
    *pclient = (clientItem*)callocMustSucceed(1,sizeof(clientItem)+strlen(address),"caPutJsonLog");
    strcpy((*pclient)->address, address);
    (*pclient)->caPutJsonLogClient = logClientCreate(saddr.sin_addr, ntohs(saddr.sin_port));
    if (!(*pclient)->caPutJsonLogClient) {
        fprintf (stderr, "caPutJsonLog: cannot create logClient %s\n", address);
        free(*pclient);
        *pclient = NULL;
        return caPutJsonLogError;
    }
    return caPutJsonLogSuccess;
}

caPutJsonLogStatus CaPutJsonLogTask::configurePvLogging()
{
    char *caPutJsonLogPVEnv;

    // Get name of a PV to which to log
    caPutJsonLogPVEnv = std::getenv("EPICS_AS_PUT_JSON_LOG_PV");

    // Variable is not set, return success (Logging to a PV is disabled)
    if (!caPutJsonLogPVEnv){
        this->pCaPutJsonLogPV = NULL;
        return caPutJsonLogSuccess;
    }

    // Get pointer to a PV structure
    long getpv_st;
    this->pCaPutJsonLogPV = &this->caPutJsonLogPV;
    getpv_st = dbNameToAddr(caPutJsonLogPVEnv, this->pCaPutJsonLogPV);

    if (getpv_st) {
        this->pCaPutJsonLogPV = NULL;
            errlogSevPrintf(errlogMajor,
                "caPutJsonLog: PV for CA Put Logging not found, logging to PV disabled\n");
        return caPutJsonLogError;
    }
    return caPutJsonLogSuccess;
}

void CaPutJsonLogTask::caPutJsonLogTask(void *arg)
{

    bool sent = false, burst = false;
    LOGDATA *pcurrent, *pnext;
    VALUE old_value, max_value, min_value;
    VALUE *pold=&old_value, *pmax=&max_value, *pmin=&min_value;

    // Receive 1st message
    this->caPutJsonLogQ.receive(&pcurrent, sizeof(LOGDATA *));
    std::memcpy(pold, &pcurrent->old_value, sizeof(VALUE));

    if (pcurrent->new_size > 0 && pcurrent->type != DBR_CHAR) {
        std::memcpy(pmax, &pcurrent->new_value.value, sizeof(VALUE));
        std::memcpy(pmin, &pcurrent->new_value.value, sizeof(VALUE));
    }

    // Main loop of the logger, which accepts the caput changes and process them
    while (!(bool)epics::atomic::get(this->taskStopper))
    {
        int msgSize;

        // Receive new put with timeout
        msgSize = this->caPutJsonLogQ.receive(&pnext, sizeof(LOGDATA *), 5.0);

        /* Timeout */
        if (msgSize == -1) {
            // If we have have unsent message and timeout occurred, send the cached change
            if (!sent) {
                buildJsonMsg(pold, pcurrent, burst, pmin, pmax);
                std::memcpy(pold, &pcurrent->new_value.value, sizeof(VALUE));
                sent = true;
                burst = false;
            }
        }

        // Message is not the correct size
        else if (msgSize != sizeof(LOGDATA *)) {
            errlogSevPrintf(errlogMinor, "caPutJsonLog: discarding incomplete log data message\n");
        }

        // Previous and new PV are the same and we are applying the burst filter
        else if ((pnext->pfield == pcurrent->pfield)
                    && (epics::atomic::get(this->config) != caPutJsonLogAllNoFilter)
                    && (pcurrent->type != DBR_CHAR)) {

            // Free "old" value
            caPutLogDataFree(pcurrent);
            pcurrent = pnext;

            // First message after logging
            if (sent) {
                // Set new initial max & min values
                std::memcpy(pmax, &pcurrent->new_value.value, sizeof(VALUE));
                std::memcpy(pmin, &pcurrent->new_value.value, sizeof(VALUE));

                sent = false;
                burst = false;
            }
            // Multiple puts within timeout
            else {
                if (isDbrNumeric(pcurrent->type) && pcurrent->new_size == 1) {
                    burst = true;
                    calculateMax(pmax, &pcurrent->new_value.value, pmax, pcurrent->type);
                    calculateMin(pmin, &pcurrent->new_value.value, pmin, pcurrent->type);
                }
            }
        }

        // We log every change
        else {
            if (!sent) {
                buildJsonMsg(pold, pcurrent, burst, pmin, pmax);
                sent = true;
            }

            caPutLogDataFree(pcurrent);
            pcurrent = pnext;

            /* Set new old_value */
            std::memcpy(pold, &pcurrent->old_value, sizeof(VALUE));

            /* Set new max & min values */
            std::memcpy(pmax, &pcurrent->new_value.value, sizeof(VALUE));
            std::memcpy(pmin, &pcurrent->new_value.value, sizeof(VALUE));

            sent = false;
            burst = false;
        }
    }
    epics::atomic::set(this->taskStopper,  false);
    errlogSevPrintf(errlogInfo, "caPutJsonLog: log task exiting\n");
}

void CaPutJsonLogTask::addPutToQueue(LOGDATA * plogData)
{
    if (this->caPutJsonLogQ.trySend(&plogData, sizeof(LOGDATA *))) {
        errlogSevPrintf(errlogMinor, "caPutJsonLog: message queue overflow\n");
        caPutLogDataFree(plogData);
    }
}


#define CALL_YAJL_FUNCTION_AND_CHECK_STATUS(flag, call) \
    { \
    flag = call; \
    if (flag != yajl_gen_status_ok) { \
        errlogSevPrintf(errlogMinor, "caPutJsonLog: JSON generation error\n"); \
        yajl_gen_free(handle); \
        return caPutJsonLogError; \
    } \
    }

caPutJsonLogStatus CaPutJsonLogTask::buildJsonMsg(const VALUE *pold_value, const LOGDATA *pLogData,
                                bool burst, const VALUE *pmin, const VALUE *pmax)
{
    // Intermediate message build buffer
    // The longest message for the buffer can occur in the lso/lsi records which
    // is defined with MAX_ARRAY_SIZE_BYTES, if this is less then 40 then
    // stringin / stringout are the limits
    const size_t interBufferSize = MAX_STRING_SIZE + 1 > MAX_ARRAY_SIZE_BYTES + 1
                            ? MAX_STRING_SIZE + 1
                            : MAX_ARRAY_SIZE_BYTES + 1;
    unsigned char interBuffer[interBufferSize];
    yajl_gen_status status;

    // Configure yajl generator
    yajl_gen handle = yajl_gen_alloc(NULL);
    if (handle == NULL) {
        errlogSevPrintf(errlogMinor, "caPutJsonLog: failed to allocate yajl handler\n");
        return caPutJsonLogError;
    }

    // Open json root map
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_map_open(handle));

    // Add date parameter
    const unsigned char str_date[] = "date";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_date,
                            strlen(reinterpret_cast<const char *>(str_date))));
    epicsTimeToStrftime(reinterpret_cast<char *>(interBuffer), interBufferSize, "%Y-%m-%d",
        &pLogData->new_value.time);
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                            strlen(reinterpret_cast<char *>(interBuffer))));

    // Add time of the day parameter
    const unsigned char str_time[] = "time";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_time,
                            strlen(reinterpret_cast<const char *>(str_time))));
    epicsTimeToStrftime(reinterpret_cast<char *>(interBuffer), interBufferSize, "%H:%M:%S.%03f",
        &pLogData->new_value.time);
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                            strlen(reinterpret_cast<char *>(interBuffer))));

    // Add hostname
    const unsigned char str_host[] = "host";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_host,
                            strlen(reinterpret_cast<const char *>(str_host))));
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle,
                            reinterpret_cast<const unsigned char *>(pLogData->hostid),
                            strlen(pLogData->hostid)));

    // Add system user
    const unsigned char str_user[] = "user";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_user,
                            strlen(reinterpret_cast<const char *>(str_user))));
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle,
                            reinterpret_cast<const unsigned char *>(pLogData->userid),
                            strlen(pLogData->userid)));

    // Add metadata
    std::map<std::string, std::string>::iterator meta_it;
    for(meta_it = metadata.begin(); meta_it != metadata.end(); meta_it++){
            CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle,
                            reinterpret_cast<const unsigned char *>(meta_it->first.c_str()),
                            meta_it->first.length()));
            CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle,
                            reinterpret_cast<const unsigned char *>(meta_it->second.c_str()),
                            meta_it->second.length()));
    }

    // Add PV name
    const unsigned char str_pvName[] = "pv";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_pvName,
                            strlen(reinterpret_cast<const char *>(str_pvName))));
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle,
                            reinterpret_cast<const unsigned char *>(pLogData->pv_name),
                            strlen(pLogData->pv_name)));

    // Add new PV value */
    const unsigned char str_newVal[] = "new";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_newVal,
                            strlen(reinterpret_cast<const char *>(str_newVal))));

    // Open Json array if we have array value
    if (pLogData->is_array) {
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_array_open(handle));
    }

    // We have string
    if (pLogData->type == DBR_CHAR){
        fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
                &pLogData->new_value.value, DBR_CHAR, 0);
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                            strlen(reinterpret_cast<const char *>(interBuffer))));
    }
    // Arrays and scalars (all except DBR_CHAR)
    else {
        for (int i = 0; i < pLogData->new_log_size; i++) {
            if (this->testForSpecialValues(&pLogData->new_value.value, pLogData->type, i) == svNan){
                const unsigned char str_Nan[] = "Nan";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_Nan,
                            strlen(reinterpret_cast<const char *>(str_Nan))));
            }
            else if (this->testForSpecialValues(&pLogData->new_value.value, pLogData->type, i) == svPinf){
                const unsigned char str_pinf[] = "Infinity";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_pinf,
                            strlen(reinterpret_cast<const char *>(str_pinf))));
            }
            else if (this->testForSpecialValues(&pLogData->new_value.value, pLogData->type, i)== svNinf){
                const unsigned char str_ninf[] = "-Infinity";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_ninf,
                            strlen(reinterpret_cast<const char *>(str_ninf))));
            }
            else {
                fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
                            &pLogData->new_value.value, pLogData->type, i);
                if (pLogData->type == DBR_STRING) {
                    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                                strlen(reinterpret_cast<char *>(interBuffer))));
                } else {
                    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_number(handle,
                                reinterpret_cast<const char *>(interBuffer),
                                strlen(reinterpret_cast<char *>(interBuffer))));
                }
            }
        }
    }
    //  Close Json array and add new size, but only if we have array
    if (pLogData->is_array) {
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_array_close(handle));
        const unsigned char str_newSize[] = "new-size";
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_newSize,
                            strlen(reinterpret_cast<const char*>(str_newSize))));
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_integer(handle, pLogData->new_size));
    }

    // Add old PV value */
    const unsigned char str_oldVal[] = "old";
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_oldVal,
                            strlen(reinterpret_cast<const char *>(str_oldVal))));
    // Open Json array if we have array value
    if (pLogData->is_array) {
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_array_open(handle));
    }
    // We have string
    if (pLogData->type == DBR_CHAR){
        fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
                pold_value, DBR_CHAR, 0);
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                            strlen(reinterpret_cast<const char *>(interBuffer))));
    }
    // Arrays and scalars (all except DBR_CHAR)
    else {
        for (int i = 0; i < pLogData->old_log_size; i++) {
            if (this->testForSpecialValues(pold_value, pLogData->type, i) == svNan){
                const unsigned char str_Nan[] = "Nan";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_Nan,
                            strlen(reinterpret_cast<const char *>(str_Nan))));
            }
            else if (this->testForSpecialValues(pold_value, pLogData->type, i) == svPinf){
                const unsigned char str_pinf[] = "Infinity";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_pinf,
                            strlen(reinterpret_cast<const char *>(str_pinf))));
            }
            else if (this->testForSpecialValues(pold_value, pLogData->type, i) == svNinf){
                const unsigned char str_ninf[] = "-Infinity";
                CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_ninf,
                            strlen(reinterpret_cast<const char *>(str_ninf))));
            }
            else {
                fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
                            pold_value, pLogData->type, i);
                if (pLogData->type == DBR_STRING) {
                    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, interBuffer,
                                strlen(reinterpret_cast<char *>(interBuffer))););
                } else {
                    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_number(handle,
                                reinterpret_cast<const char *>(interBuffer),
                                strlen(reinterpret_cast<char *>(interBuffer))));
                }
            }
        }
    }
    // Close Json array and add new size, but only if we have array
    if (pLogData->is_array) {
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_array_close(handle));
        const unsigned char str_oldSize[] = "old-size";
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_oldSize,
                        strlen(reinterpret_cast<const char*>(str_oldSize))));
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_integer(handle, pLogData->old_size));
    }

    // Add minium and maximum values in case of a burst
    if (burst && isDbrNumeric(pLogData->type)
                && !pLogData->is_array) {
        // Add min value
        const unsigned char str_minVal[] = "min";
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_minVal,
                        strlen(reinterpret_cast<const char*>(str_minVal))));
        fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
                    pmin, pLogData->type, 0);
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_number(handle,
                        reinterpret_cast<const char *>(interBuffer),
                        strlen(reinterpret_cast<char *>(interBuffer))));

        // Add max value
        const unsigned char str_maxVal[] = "max";
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_string(handle, str_maxVal,
                        strlen(reinterpret_cast<const char*>(str_maxVal))););
        fieldVal2Str(reinterpret_cast<char *>(interBuffer), interBufferSize,
            pmax, pLogData->type, 0);
        CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_number(handle,
                        reinterpret_cast<const char *>(interBuffer),
                        strlen(reinterpret_cast<char *>(interBuffer))));
    }

    /* Close root map */
    CALL_YAJL_FUNCTION_AND_CHECK_STATUS(status, yajl_gen_map_close(handle));

    /* Get JSON as NULL terminated cstring */
    const unsigned char * buf;
    size_t len = 0;
    yajl_gen_get_buf(handle, &buf, &len);

    /* Get a JSON as a string */
    std::string json (reinterpret_cast<const char *>(buf));

    /* First log to a PV so we can append new line later for the logging to a server */
    this->logToPV(json);
    this->logToServer(json.append("\n"));
    yajl_gen_free(handle);
    return caPutJsonLogSuccess;
}

void CaPutJsonLogTask::logToServer(std::string &msg)
{
    clientItem* client;
    epicsMutex::guard_t G(clientsMutex);
    for (client = clients; client; client = client->next) {
        logClientSend (client->caPutJsonLogClient, msg.c_str());
    }
}

void CaPutJsonLogTask::logToPV(std::string &msg)
{
    if (this->pCaPutJsonLogPV != NULL) {
        long status;

        // Waveform record
        if (this->pCaPutJsonLogPV->field_type == DBR_CHAR) {
            status = dbPutField(this->pCaPutJsonLogPV, DBR_CHAR, msg.c_str(), msg.length());
        }
        // Lso/lsi records
        else {
            // As of EPICS base 7.0.4 this still clips at 40 characters
            // Addint .$ or .VAL$ to the PV name, will use the waveform case and
            // write the whole string correctly
             status = dbPutField(this->pCaPutJsonLogPV, DBR_STRING, msg.c_str(), 1);
        }

        if (status) {
            errlogSevPrintf(errlogMajor,
                "caPutJsonLog: dbPutField to Log PV failed, status = %ld\n", status);
        }
    }
}

void CaPutJsonLogTask::calculateMin(VALUE *pres, const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
        case DBR_CHAR:
            pres->v_int8 = std::min(pa->v_int8, pb->v_int8);
            return;
        case DBR_UCHAR:
            pres->v_uint8 = std::min(pa->v_uint8, pb->v_uint8);
            return;
        case DBR_SHORT:
            pres->v_int16 = std::min(pa->v_int16, pb->v_int16);
            return;
        case DBR_USHORT:
        case DBR_ENUM:
            pres->v_uint16 = std::min(pa->v_uint16, pb->v_uint16);
            return;
        case DBR_LONG:
            pres->v_int32 = std::min(pa->v_int32, pb->v_int32);
            return;
        case DBR_ULONG:
            pres->v_uint32 = std::min(pa->v_uint32, pb->v_uint32);
            return;
        case DBR_INT64:
            pres->v_int64 = std::min(pa->v_int64, pb->v_int64);
            return;
        case DBR_UINT64:
            pres->v_uint64 = std::min(pa->v_uint64, pb->v_uint64);
            return;
        case DBR_FLOAT:
            pres->v_float = std::min(pa->v_float, pb->v_float);
            return;
        case DBR_DOUBLE:
            pres->v_double = std::min(pa->v_double, pb->v_double);
            return;
    }
}

void CaPutJsonLogTask::calculateMax(VALUE *pres, const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
        case DBR_CHAR:
            pres->v_int8 = std::max(pa->v_int8, pb->v_int8);
            return;
        case DBR_UCHAR:
            pres->v_uint8 = std::max(pa->v_uint8, pb->v_uint8);
            return;
        case DBR_SHORT:
            pres->v_int16 = std::max(pa->v_int16, pb->v_int16);
            return;
        case DBR_USHORT:
        case DBR_ENUM:
            pres->v_uint16 = std::max(pa->v_uint16, pb->v_uint16);
            return;
        case DBR_LONG:
            pres->v_int32 = std::max(pa->v_int32, pb->v_int32);
            return;
        case DBR_ULONG:
            pres->v_uint32 = std::max(pa->v_uint32, pb->v_uint32);
            return;
        case DBR_INT64:
            pres->v_int64 = std::max(pa->v_int64, pb->v_int64);
            return;
        case DBR_UINT64:
            pres->v_uint64 = std::max(pa->v_uint64, pb->v_uint64);
            return;
        case DBR_FLOAT:
            pres->v_float = std::max(pa->v_float, pb->v_float);
            return;
        case DBR_DOUBLE:
            pres->v_double = std::max(pa->v_double, pb->v_double);
            return;
    }
}

bool CaPutJsonLogTask::compareValue(const VALUE *pa, const VALUE *pb, short type)
{
    switch (type) {
        case DBR_CHAR:
            return (pa->v_int8 == pb->v_int8);
        case DBR_UCHAR:
            return (pa->v_uint8 == pb->v_uint8);
        case DBR_SHORT:
            return (pa->v_int16 == pb->v_int16);
        case DBR_USHORT:
        case DBR_ENUM:
            return (pa->v_uint16 == pb->v_uint16);
        case DBR_LONG:
            return (pa->v_int32 == pb->v_int32);
        case DBR_ULONG:
            return (pa->v_uint32 == pb->v_uint32);
        case DBR_INT64:
            return (pa->v_int64 == pb->v_int64);
        case DBR_UINT64:
            return (pa->v_uint64 == pb->v_uint64);
        case DBR_FLOAT:
            return (pa->v_float == pb->v_float);
        case DBR_DOUBLE:
            return (pa->v_double == pb->v_double);
        case DBR_STRING:
            return (0 == strcmp(pa->v_string, pb->v_string));
        default:
            return 0;
    }
}

int CaPutJsonLogTask::fieldVal2Str(char *pbuf, size_t buflen, const VALUE *pval, short type, int index)
{
    switch (type) {
        case DBR_CHAR:
            return epicsSnprintf(pbuf, buflen, "%s", ((char *)pval));
        case DBR_UCHAR:
            return epicsSnprintf(pbuf, buflen, "%d", ((epicsUInt8 *)pval)[index]);
        case DBR_SHORT:
            return epicsSnprintf(pbuf, buflen, "%hd", ((epicsInt16 *)pval)[index]);
        case DBR_USHORT:
        case DBR_ENUM:
            return epicsSnprintf(pbuf, buflen, "%hu", ((epicsUInt16 *)pval)[index]);
        case DBR_LONG:
            return epicsSnprintf(pbuf, buflen, "%d", ((epicsInt32 *)pval)[index]);
        case DBR_ULONG:
            return epicsSnprintf(pbuf, buflen, "%u", ((epicsUInt32 *)pval)[index]);
        case DBR_FLOAT:
            return epicsSnprintf(pbuf, buflen, "%g", ((epicsFloat32 *)pval)[index]);
        case DBR_DOUBLE:
            return epicsSnprintf(pbuf, buflen, "%g", ((epicsFloat64 *)pval)[index]);
        case DBR_INT64:
            return epicsSnprintf(pbuf, buflen, "%lld", ((epicsInt64 *)pval)[index]);
        case DBR_UINT64:
            return epicsSnprintf(pbuf, buflen, "%llu", ((epicsUInt64 *)pval)[index]);
        case DBR_STRING:
            return epicsSnprintf(pbuf, buflen, "%s", ((char *)pval) + index * MAX_STRING_SIZE);
        default:
            errlogSevPrintf(errlogMajor,
                "caPutJsonLog: failed to convert PV value to a string representation\n");
            return caPutJsonLogError;
    }
}

specialValues CaPutJsonLogTask::testForSpecialValues(const VALUE *pval, short type, int index) {

    epicsFloat64 d;

    switch (type) {
        case DBR_FLOAT:
            d = ((epicsFloat32 *)pval)[index];
            break;
        case DBR_DOUBLE:
            d = ((epicsFloat64 *)pval)[index];
            break;
        default:
            return svNormal;
    }

    if (isnan(d)) return svNan;
    else if (isinf(d) && d > 0) return svPinf;
    else if (isinf(d) && d < 0) return svNinf;
    else return svNormal;
}


/* Called from C */

void caddPutToQueue(LOGDATA * plogData)
{
    CaPutJsonLogTask *instance = CaPutJsonLogTask::getInstance();
    return instance->addPutToQueue(plogData);
}
void caPutJsonLogWorker(void *arg)
{
    CaPutJsonLogTask *instance = CaPutJsonLogTask::getInstance();
    instance->caPutJsonLogTask(arg);
}
void caPutJsonLogExit(void *arg)
{
    CaPutJsonLogTask *instance = CaPutJsonLogTask::getInstance();
    instance->stop();
}
