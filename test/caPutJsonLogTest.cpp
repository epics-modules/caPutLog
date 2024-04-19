/* File:     caPutJsonLogTest.cpp
 * Author:   Matic Pogacnik
 * Created:  03.08.2020
 *
 * Unit tests for the Json format logger
 *
 * Modification log:
 * ----------------
 * v 1.0
 * - Initial version.
*/


// Standard library includes
#include <vector>
#include <sstream>
#include <string>
#include <cstring>
#include <algorithm>

// Epics base includes
#include <epicsMath.h>
#include <osiSock.h>
#include <envDefs.h>
#include <errlog.h>
#include <epicsString.h>
#include <db_access.h>
#include <db_access_routines.h>
#include <dbUnitTest.h>
#include <testMain.h>
#include <epicsAtomic.h>
#include <fdmgr.h>
#include <osiProcess.h>
#include <dbEvent.h>
#include <dbServer.h>
#include <asDbLib.h>
#include <iocInit.h>
#include <cadef.h>
#include <logClient.h>
#include <yajl_parse.h>

// This module includes
#include "caPutJsonLogTask.h"

// Buffer size for log server receive buffer
#define BUFFER_SIZE 1024

// Timeout for ca_pend_io, on busy machies this can be
// increased to avoid timeouts and failed tests
#define CA_PEND_IO_TIMEOUT 5.0

// Prefix to be used before Json logs
static const char* logMsgPrefix = "testPrefix";

// Events
static epicsEvent testIocReady;
static epicsEvent testLogServerReady;
static epicsEvent testThreadDone;
static epicsEvent logServerThreadDone;
static epicsEvent testLogServerMsgReady;

// Global variables
static dbEventCtx testEvtCtx;
static std::string logServerAddress;
static int stopLogServer = 0;
CaPutJsonLogTask *logger;



extern "C" {
    void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);
}

/*******************************************************************************
* Helper functions
*******************************************************************************/
template<class T>
std::string toString(T input) {

    std::ostringstream ss;
    ss << input;
    return ss.str();
}

/*******************************************************************************
* Logger server implementation
*******************************************************************************/
static void *pfdctx;
static SOCKET sock;
static SOCKET insock;
static std::string incLogMsg;

static void readFromClient(void *pParam)
{
    char recvbuf[BUFFER_SIZE];
    int recvLength;
    std::string

    memset(recvbuf, 0, BUFFER_SIZE);
    recvLength = recv(insock, recvbuf, BUFFER_SIZE, 0);
    if (recvLength > 0) {
        incLogMsg.append(recvbuf, recvLength);
        if (incLogMsg.find('\n') != std::string::npos) {
            testLogServerMsgReady.trigger();
        }
    }
}

static void acceptNewClient ( void *pParam )
{
    osiSocklen_t addrSize;
    struct sockaddr_in addr;
    int status;

    addrSize = sizeof ( addr );
    insock = epicsSocketAccept ( sock, (struct sockaddr *)&addr, &addrSize );
    testOk(insock != INVALID_SOCKET && addrSize >= sizeof (addr),
        "Log server accepted Json logger client");

    status = fdmgr_add_callback(pfdctx, insock, fdi_read,
        readFromClient,  NULL);

    if (status < 0) {
        testAbort("fdmgr_add_callback failed!");
    }
}

static void logServer(void) {

    int status;
    struct sockaddr_in serverAddr;
    struct sockaddr_in actualServerAddr;
    osiSocklen_t actualServerAddrSize;

    testDiag("Starting log server");

    // Create socket
    sock = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        testAbort("epicsSocketCreate failed.");
    }

    // We listen on a an available port.
    memset((void *)&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(0);
    status = bind (sock, (struct sockaddr *)&serverAddr,
                   sizeof (serverAddr) );
    if (status < 0) {
        testAbort("bind failed; all ports in use?");
    }

    status = listen(sock, 10);
    if (status < 0) {
        testAbort("listen failed!");
    }

    // Determine the port that the OS chose
    actualServerAddrSize = sizeof actualServerAddr;
    memset((void *)&actualServerAddr, 0, sizeof serverAddr);
    status = getsockname(sock, (struct sockaddr *) &actualServerAddr,
         &actualServerAddrSize);
    if (status < 0) {
        testAbort("Can't find port number!");
    }

    //Build string address
    logServerAddress.append("localhost:")
                    .append(toString(htons(actualServerAddr.sin_port)));

    pfdctx = (void *) fdmgr_init();
    if (status < 0) {
        testAbort("fdmgr_init failed!");
    }

    status = fdmgr_add_callback(pfdctx, sock, fdi_read,
        acceptNewClient, &serverAddr);

    if (status < 0) {
        testAbort("fdmgr_add_callback failed!");
    }

    // Signal that we are ready
    testDiag("Test log server ready");
    testLogServerReady.trigger();

    // Wait for events
    struct timeval timeout;
    timeout.tv_sec = 1; /* in seconds */
    timeout.tv_usec = 0;
    while(!epics::atomic::get(stopLogServer))
        fdmgr_pend_event(pfdctx, &timeout);

    // Signal that we finished
    logServerThreadDone.signal();
}


/*******************************************************************************
* Json handling
*******************************************************************************/
class JsonParser {
    bool inArray;

    bool waitingKey;
    std::string currentKey;

public:

    std::string jsonLogMsg;
    std::string prefix;
    std::string json;

    std::string date;
    std::string time;
    std::string host;
    std::string user;
    std::string pv;

    std::vector<std::string> newVal;
    std::vector<std::string> oldVal;
    std::map<std::string, std::string> metadata;
    int newSize;
    int oldSize;

    double min;
    double max;

    JsonParser() :
        inArray(false),
        waitingKey(true),
        newSize(-1),
        oldSize(-1)
    { }
    ~JsonParser() { }

    void parse(std::string jsonLogMsg) {

        static const yajl_callbacks json_callbacks = {
            NULL,                 /* yajl_null */
            NULL,                 /* yajl_boolean */
            JsonParser::json_integer,   /* yajl_integer */
            JsonParser::json_double,    /* yajl_double */
            NULL,                 /* yajl_number */
            JsonParser::json_string,    /* yajl_string */
            NULL,                 /* yajl_start_map */
            JsonParser::json_mapKey,                 /* yajl_map_key */
            NULL,                 /* yajl_end_map */
            JsonParser::json_arrayStart,/* yajl_start_array */
            JsonParser::json_arrayStop  /* yajl_end_array */
            };

        // Store received Json string
        this->jsonLogMsg = jsonLogMsg.substr(0, jsonLogMsg.find("\n", 0) + 1);

        // Split to prefix and json
        this->prefix = this->jsonLogMsg.substr(0, incLogMsg.find("{"));
        this->json = this->jsonLogMsg.substr(incLogMsg.find("{"), incLogMsg.size() - 1);

        // Parse Json to the object
        yajl_handle handle;
        handle = yajl_alloc(&json_callbacks, NULL, (void *) this);
        yajl_parse(handle, reinterpret_cast<const unsigned char*>(this->json.c_str()), this->json.length());
        yajl_complete_parse(handle);
        yajl_free(handle);
    }


    // Callbacks from yajl parser (called from C, must be static)
    // Not a problem as we get pointer to an instance via context pointer
    static int json_integer(void * ctx, long long integerVal) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting key, got integer callback.");
            return 0;
        }

        if (!jsonParser->currentKey.compare("new")) {
            jsonParser->newVal.push_back(toString(integerVal));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (!jsonParser->currentKey.compare("old")) {
            jsonParser->oldVal.push_back(toString(integerVal));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (!jsonParser->currentKey.compare("min")) {
            jsonParser->min = integerVal;
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("max")) {
            jsonParser->max = integerVal;
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("new-size")) {
            jsonParser->newSize = integerVal;
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("old-size")) {
            jsonParser->oldSize = integerVal;
            jsonParser->waitingKey = true;
        }
        else {
            testAbort("JsonParser: Unexpected double callback in Json");
            return 0;
        }
        return 1;
    }

    static int json_double(void * ctx, double doubleVal) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting key, got double callback.");
            return 0;
        }

        if (!jsonParser->currentKey.compare("new")) {
            jsonParser->newVal.push_back(toString(doubleVal));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (!jsonParser->currentKey.compare("old")) {
            jsonParser->oldVal.push_back(toString(doubleVal));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (!jsonParser->currentKey.compare("min")) {
            jsonParser->min = doubleVal;
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("max")) {
            jsonParser->max = doubleVal;
            jsonParser->waitingKey = true;
        }
        else {
            testAbort("JsonParser: Unexpected double callback in Json");
            return 0;
        }
        return 1;
    }

    static int json_string(void * ctx, const unsigned char * stringVal, size_t stringLen) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting key, got start of array callback.");
            return 0;
        }

        if (!jsonParser->currentKey.compare("date")) {
            jsonParser->date.assign(reinterpret_cast<const char *>(stringVal), stringLen);
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("time")) {
            jsonParser->time.assign(reinterpret_cast<const char *>(stringVal), stringLen);
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("host")) {
            jsonParser->host.assign(reinterpret_cast<const char *>(stringVal), stringLen);
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("user")) {
            jsonParser->user.assign(reinterpret_cast<const char *>(stringVal), stringLen);
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("pv")) {
            jsonParser->pv.assign(reinterpret_cast<const char *>(stringVal), stringLen);
            jsonParser->waitingKey = true;
        }
        else if (!jsonParser->currentKey.compare("new")) {
            jsonParser->newVal.push_back(std::string(reinterpret_cast<const char *>(stringVal), stringLen));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (!jsonParser->currentKey.compare("old")) {
            jsonParser->oldVal.push_back(std::string(reinterpret_cast<const char *>(stringVal), stringLen));
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else if (logger->isMetadataKey(jsonParser->currentKey)) {
            std::pair<std::map<std::string, std::string>::iterator, bool> ret;
            ret = jsonParser->metadata.insert(std::pair<std::string, std::string>(jsonParser->currentKey,
                  std::string(reinterpret_cast<const char *>(stringVal), stringLen)));
            if (ret.second == false) {
                testAbort("caPutJsonLog: fail to add property %s to json log\n", jsonParser->currentKey.c_str());
                return caPutJsonLogError;
            }
            if (!jsonParser->inArray) {
                jsonParser->waitingKey = true;
            }
        }
        else {
            testAbort("JsonParser: Unexpected string callback in Json");
            return 0;
        }
        return 1;
    }

    static int json_arrayStart(void * ctx) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting key, got start of array callback.");
            return 0;
        }

        jsonParser->inArray = true;
        jsonParser->waitingKey = false;
        return 1;
    }

    static int json_arrayStop(void * ctx) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting key, got end of array callback.");
            return 0;
        }

        jsonParser->inArray = false;
        jsonParser->waitingKey = true;
        return 1;
    }

   static int json_mapKey(void * ctx, const unsigned char * key, size_t stringLen) {
        JsonParser *jsonParser = (JsonParser*) ctx;

        if (!jsonParser->waitingKey) {
            testAbort("JsonParser: Was expecting value, got key callback.");
            return 0;
        }

        jsonParser->currentKey.assign(reinterpret_cast<const char *>(key), stringLen);
        jsonParser->waitingKey = false;
        return 1;
   } 
};


/*******************************************************************************
* Tests helpers
*******************************************************************************/
bool metadata_compare (std::map<std::string, std::string> metadataA, std::map<std::string, std::string> metadataB) {
       bool testSize = metadataA.size() == metadataB.size();
       bool testContent = std::equal(metadataA.begin(), metadataA.end(), metadataB.begin());
       return testSize && testContent;
   }

void commonTests(JsonParser &jsonParser, const char* pvname, const char * testPrefix) {

    // Test if is Json terminated with newline
    testOk(*jsonParser.jsonLogMsg.rbegin() == *"\n",
            "%s - %s", testPrefix, "New line terminator check");

    // Test prefix
    testOk(!jsonParser.prefix.compare(logMsgPrefix),
            "%s - %s", testPrefix, "Prefix check");

    // Test date format
    testOk(jsonParser.date.length() == 10 &&
            jsonParser.date.at(4) == *"-" &&
            jsonParser.date.at(7) == *"-",
            "%s - %s", testPrefix, "Date format check");

    // Test time format
    testOk(jsonParser.time.length() == 12 &&
            jsonParser.time.at(2) == *":" &&
            jsonParser.time.at(5) == *":" &&
            jsonParser.time.at(8) == *".",
            "%s - %s", testPrefix, "Time format check");

    // Test hostname
    char hostname[1024];
    gethostname(hostname, 1024);

    // asLibRoutines changes the hostnames to lower-case; some OSs are not so kind.
    testOk(!epicsStrCaseCmp(hostname, jsonParser.host.c_str()),
            "%s - %s", testPrefix, "Hostname check");

    // Test username
    char username[1024];
    osiGetUserName(username, 1024);
    testOk(!jsonParser.user.compare(username),
            "%s - %s", testPrefix, "Username check");

    testOk(!jsonParser.pv.compare(pvname),
            "%s - %s", testPrefix, "PV name check");

    //Test Metadata
    testOk(metadata_compare(jsonParser.metadata, logger->getMetadata()),
            "%s - %s - Parser(%lu) vs Logger(%lu)", testPrefix, "Metadata keys and values check",
            logger->metadataCount(), jsonParser.metadata.size());
}

template<class T>
void testDbf(const char *pv, chtype type,
        T value1, int value1Size, T value2, int value2Size, const char * testPrefix) {

    chid pchid;

    SEVCHK(ca_create_channel(pv, NULL, NULL, 0, &pchid), "ca_create_channel failed");
    SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io failed");


    // First put -> we check only new value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value1Size, pchid, (void *) value1), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run commcon tests
        commonTests(json, pv, testPrefix);

        // New Value check
        for (size_t i = 0; i < json.newVal.size(); i++) {
            if (isnan(*(value1 + i))) {
                testOk(!json.newVal.at(i).compare("Nan"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value1 + i) && *(value1 + i) > 0)) {
                testOk(!json.newVal.at(i).compare("Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value1 + i) && *(value1 + i) < 0)) {
                testOk(!json.newVal.at(i).compare("-Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else {
                testOk(!json.newVal.at(i).compare(toString(*(value1 + i))),
                        "%s - %s", testPrefix, "New value check");
            }
        }
    }

    // Second put -> we check new and old value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value2Size, pchid, (void *) value2), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run common tests
        commonTests(json, pv, testPrefix);

        // New Value check
        for (size_t i = 0; i < json.newVal.size(); i++) {
            if (isnan(*(value2 + i))) {
                testOk(!json.newVal.at(i).compare("Nan"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value2 + i)) && *(value2 + i) > 0) {
                testOk(!json.newVal.at(i).compare("Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value2 + i)) && *(value2 + i) < 0) {
                testOk(!json.newVal.at(i).compare("-Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else {
                testOk(!json.newVal.at(i).compare(toString(*(value2 + i))),
                        "%s - %s", testPrefix, "New value check");
            }
        }

        // Old Value check
        for (size_t i = 0; i < json.oldVal.size(); i++) {
            if (isnan(*(value1 + i))) {
                testOk(!json.oldVal.at(i).compare("Nan"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value1 + i) && *(value1 + i) > 0)) {
                testOk(!json.oldVal.at(i).compare("Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else if (isinf(*(value1 + i) && *(value1 + i) < 0)) {
                testOk(!json.oldVal.at(i).compare("-Infinity"),
                        "%s - %s", testPrefix, "New value check");
            }
            else {
                testOk(!json.oldVal.at(i).compare(toString(*(value1 + i))),
                        "%s - %s", testPrefix, "New value check");
            }
        }
    }
}

template<>
void testDbf<dbr_string_t>(const char *pv, chtype type,
        char *value1, int value1Size, char *value2, int value2Size, const char * testPrefix) {

    chid pchid;

    SEVCHK(ca_create_channel(pv, NULL, NULL, 0, &pchid), "ca_create_channel failed");
    SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io failed");


    // First put -> we check only new value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value1Size, pchid, (void *) value1), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run commcon tests
        commonTests(json, pv, testPrefix);

        // New Value check
        for (size_t i = 0; i < json.newVal.size(); i++) {
            testOk(!json.newVal.at(i).compare(value1 + i*MAX_STRING_SIZE),
                    "%s - %s", testPrefix, "New value check");
        }
    }

    // Second put -> we check new and old value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value2Size, pchid, (void *) value2), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run common tests
        commonTests(json, pv, testPrefix);

        // New Value check
        for (size_t i = 0; i < json.newVal.size(); i++) {
            testOk(!json.newVal.at(i).compare(value2 + i*MAX_STRING_SIZE),
                    "%s - %s", testPrefix, "New value check");
        }

        // Old Value check
        for (size_t i = 0; i < json.newVal.size(); i++) {
            testOk(!json.oldVal.at(i).compare(value1 + i*MAX_STRING_SIZE),
                "%s - %s", testPrefix, "Old value check");
        }
    }
}

template<>
void testDbf(const char *pv, chtype type,
        char *value1, int value1Size, char *value2, int value2Size, const char * testPrefix) {

    chid pchid;

    SEVCHK(ca_create_channel(pv, NULL, NULL, 0, &pchid), "ca_create_channel failed");
    SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io failed");


    // First put -> we check only new value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value1Size, pchid, (void *) value1), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run commcon tests
        commonTests(json, pv, testPrefix);

        // New Value check
        testOk(!json.newVal.at(0).compare(std::string(value1, value1Size)),
                "%s - %s", testPrefix, "New value check");
    }

    // Second put -> we check new and old value
    {
        JsonParser json;
        SEVCHK(ca_array_put(type, value2Size, pchid, (void *) value2), "ca_array_put error");
        SEVCHK(ca_pend_io (CA_PEND_IO_TIMEOUT), "ca_pend_io error");
        testDiag("Made caput, now waiting for log message to arrive (approx. 5s - 10s)");
        testLogServerMsgReady.wait();

        // Parse received Json
        json.parse(incLogMsg);
        incLogMsg.clear();

        //Run common tests
        commonTests(json, pv, testPrefix);

        // New Value check
        testOk(!json.newVal.at(0).compare(std::string(value2, value2Size)),
                "%s - %s", testPrefix, "New value check");

        // Old Value check
        testOk(!json.oldVal.at(0).compare(std::string(value1, value1Size)),
                "%s - %s", testPrefix, "New value check");
    }
}

void testMetadataHelper(std::map<std::string, std::string> metadata )
{
    char desc1[] = "Description 1";
    char desc2[] = "Description 2";
    std::map<std::string, std::string>::iterator meta_it;
    for(meta_it = metadata.begin(); meta_it != metadata.end(); meta_it++){
        logger->addMetadata(meta_it->first,meta_it->second);
    }
    testDbf<dbr_string_t>("longout_DBF_STRING.DESC", DBR_STRING, desc1, 1, desc2, 1, "Metadata test");
    testOk(metadata_compare(metadata, logger->getMetadata()),
             "Metadata test - Metadata keys and values check - Original test(%lu) vs Logger(%lu)",
             metadata.size(), logger->metadataCount());
    logger->removeAllMetadata();
}

/*******************************************************************************
* Tests
*******************************************************************************/
void runTests(void *arg) {

    //Create CA context for the thread
    ca_context_create(ca_enable_preemptive_callback);

    // Wait for test IOC to be ready
    testIocReady.wait();

    // Test DBF_STRING
    char desc1[] = "Description 1";
    char desc2[] = "Description 2";
    testDbf<dbr_string_t>("longout_DBF_STRING.DESC", DBR_STRING, desc1, 1, desc2, 1, "DBF_STRING test");

    // Test DBF_LONG
    dbr_long_t long1 = 55;
    dbr_long_t long2 = 89898;
    testDbf<dbr_long_t *>("longout_DBF_LONG.VAL", DBR_LONG, &long1, 1, &long2, 1, "DBF_LONG test");

    // Test DBF_SHORT
    dbr_short_t short1 = 10;
    dbr_short_t short2 = -9;
    testDbf<dbr_short_t *>("longout_DBF_SHORT.DISV", DBR_SHORT, &short1, 1, &short2, 1, "DBF_SHORT test");

    // Test DBF_FLOAT
    dbr_float_t float1 = 125.987;
    dbr_float_t float2 = epicsINF;
    testDbf<dbr_float_t *>("ao_DBF_FLOAT.VAL", DBR_FLOAT, &float1, 1, &float2, 1, "DBF_FLOAT test");
    float1 = epicsNAN;
    float2 = -epicsINF;
    testDbf<dbr_float_t *>("ao_DBF_FLOAT.VAL", DBR_FLOAT, &float1, 1, &float2, 1, "DBF_FLOAT test");

    // Test DBF_MENU
    char scan1[] = ".5 second";
    char scan2[] = "2 second";
    testDbf<dbr_string_t>("longout_DBF_MENU.SCAN", DBR_STRING, scan1, 1, scan2, 1, "DBF_MENU test");

    // Test DBF_ENUM
    dbr_enum_t enum1 = 10;
    dbr_enum_t enum2 = 6;
    testDbf<dbr_enum_t *>("mbbo_DBF_ENUM", DBR_ENUM, &enum1, 1, &enum2, 1, "DBF_ENUM test");

    // Test DBF_DEVICE
    char dtyp1[] = "Async Soft Channel";
    char dtyp2[] = "Soft Channel";
    testDbf<dbr_string_t>("stringout_DBF_DEVICE.DTYP", DBR_STRING, dtyp1, 1, dtyp2, 1, "DBF_DEVICE test");

    // Test DBF_LINK
    char flnk1[] = "stringout_DBF_LINK1";
    char flnk2[] = "stringout_DBF_LINK2";
    testDbf<dbr_string_t>("stringout_DBF_LINK.FLNK", DBR_STRING, flnk1, 1, flnk2, 1, "DBF_LINK test");


    // Test array DBF_DOUBLE
    dbr_double_t doubleArray1[] = {
                    1.1, 11.11, 22.22, 33.33, 44.44, 55.55, 66.66, 77.77, 88.88, 99.99,
                    111.11, 222.22, 333.33, 444.44, 555.55, 666.66, 777.77, 888.88, 999.99,
                    1.1, 11.11, 22.22, 33.33, 44.44, 55.55, 66.66, 77.77, 88.88, 99.99,
                    111.11, 222.22, 333.33, 444.44, 555.55, 666.66, 777.77, 888.88, 999.99,
                    1.1, 11.11, 22.22, 33.33, 44.44, 55.55, 66.66, 77.77, 88.88, 99.99
                    };
    int doubleArray1Len = sizeof(doubleArray1)/sizeof(doubleArray1[0]);
    dbr_double_t doubleArray2[] = {
                    19.1, 119.11, 229.22, 339.33, 449.44, 559.55, 669.66, 779.77, 889.88, 999.99,
                    1119.11, 2229.22, 3339.33, 4449.44, 5559.55, 6669.66, 7779.77, 8889.88, 9999.99,
                    19.1, 119.11, 229.22, 339.33, 449.44, 559.55, 669.66, 779.77, 889.88, 999.99,
                    1119.11, 2229.22, 3339.33, 4449.44, 5559.55, 6669.66, 7779.77, 8889.88, 9999.99,
                    19.1, 119.11, 229.22, 339.33, 449.44, 559.55, 669.66, 779.77, 889.88, 999.99,
                    1119.11, 2229.22, 3339.33, 4449.44, 5559.55, 6669.66, 7779.77, 8889.88, 9999.99,
                    19.1, 119.11, 229.22, 339.33, 449.44, 559.55, 669.66, 779.77, 889.88, 999.99,
                    1119.11, 2229.22, 3339.33, 4449.44, 5559.55, 6669.66, 7779.77, 8889.88, 9999.99
                    };
    int doubleArray2Len = sizeof(doubleArray2)/sizeof(doubleArray2[0]);
    testDbf<dbr_double_t *>("waveform_DBF_DOUBLE", DBR_DOUBLE, doubleArray1, doubleArray1Len, doubleArray2, doubleArray2Len, "DBF_DOUBLE array test");

    // Test array DBF_STRING
    char stringArray1[][MAX_STRING_SIZE] = {
                    "string",
                    "string123456789012345678901234",
                    "string1234567890123456789012345",
                    "string12345678901234567890123456",
                    "string123456789012345678901234567",
                    "string1234567890123456789012345678",
                    "string12345678901234567890123456789",
                    "string123456789012345678901234567890",
                    "string1234567890123456789012345678901",
                    "string12345678901234567890123456789012",
                    "string123456789012345678901234567890123",
                    };
    int stringArray1Len = sizeof(stringArray1)/sizeof(stringArray1[0]);
    char stringArray2[][MAX_STRING_SIZE] = {
                    "string",
                    "string123456789012345678901",
                    "string1234567890123456789012",
                    "string12345678901234567890123",
                    "string123456789012345678901234",
                    "string1234567890123456789012345",
                    "string12345678901234567890123456",
                    "string123456789012345678901234567",
                    "string1234567890123456789012345678",
                    "string12345678901234567890123456789",
                    "string123456789012345678901234567890",
                    "string1234567890123456789012345678901",
                    "string12345678901234567890123456789012",
                    "string123456789012345678901234567890123",
                    };
    int stringArray2Len = sizeof(stringArray2)/sizeof(stringArray2[0]);
    testDbf<dbr_string_t>("waveform_DBF_STRING", DBR_STRING, stringArray1[0], stringArray1Len, stringArray2[0], stringArray2Len, "DBF_STRING array test");


    // Test array DBF_CHAR
    char charArray1[] = "verylongstring1_12345678901234567890123456789012345678901234567890";
    char charArray2[] = "verylongstring2_12345678901234567890123456789012345678901234567890";
    testDbf<char *>("lso_DBF_CHAR.$", DBR_CHAR,
           &charArray1[0], strlen(charArray1), &charArray2[0], strlen(charArray1), "DBF_CHAR array (lso) test");


    // Test metadata
    std::map<std::string, std::string> metadata_test_single = {
        {"ABCDEFGHIJKLMNOPQRS", "1234567890_1234567890"}
    };
    std::map<std::string, std::string> metadata_test_escape_chars = {
        {"ABC\n##&¤67&nbsp; space&#160&&#160;\'\\n#", "CBC\n##&¤67&nbsp; space&#160&&#160;\'\\n#"}
    };
    std::map<std::string, std::string> metadata_test_multiple = {
        {"One", "1"}, {"Two", "2"}, {"Three", "3"}, {"Four", "4"}, {"Five", "5"},
        {"Six", "6"}, {"Seven", "7"}, {"Eight", "8"}, {"Nine", "9"}, {"Ten", "10"},
    };

    std::map<std::string, std::string> metadata_test_reinsert_new_val = {
        {"Five", "5"}, {"Six", "6"},
    };

    testMetadataHelper(metadata_test_single);
    testMetadataHelper(metadata_test_escape_chars);
    testMetadataHelper(metadata_test_multiple);

    //Reinsert metadata test
    logger->addMetadata("Five", "Five");
    logger->addMetadata("Six", "Six");
    testMetadataHelper(metadata_test_reinsert_new_val);

    // 1000+ elements in metadata
    std::map<std::string, std::string> metadata_test_big;
    for(int i = 0; i <=100; i++)
    {
        metadata_test_big.insert(std::pair<std::string, std::string>(std::to_string(i), std::to_string(i)));
    }
    testMetadataHelper(metadata_test_big);

    //Destroy test thread CA context
    ca_context_destroy();

    //Signal that we are done
    testThreadDone.trigger();
}


/*******************************************************************************
* IOC helper functions
*******************************************************************************/
void startIoc() {
    // eltc(0);
    if(iocBuild() || iocRun())
        testAbort("Failed to start up test database");
    if(!(testEvtCtx=db_init_events()))
        testAbort("Failed to initialize test dbEvent context");
    if(DB_EVENT_OK!=db_start_events(testEvtCtx, "CAS-test", NULL, NULL, epicsThreadPriorityCAServerLow))
        testAbort("Failed to start test dbEvent context");
    // eltc(1);
}

void stopIoc(){
    testIocShutdownOk();
}

/*******************************************************************************
* Main function
*******************************************************************************/
MAIN(caPutJsonLogTests)
{

    testPlan(499);

    // Create thread for log server
    const char * logServerThreadName = "testLogServer";
    epicsThreadCreate(logServerThreadName,
                        epicsThreadPriorityMedium,
                        epicsThreadStackMedium,
                        (EPICSTHREADFUNC) logServer,
                        NULL);
    testLogServerReady.wait();

    // Create thread for tests
    const char * testsThreadName = "testTests";
    epicsThreadCreate(testsThreadName,
                        epicsThreadPriorityMedium,
                        epicsThreadStackMedium,
                        (EPICSTHREADFUNC) runTests,
                        NULL);

    // Prepare test IOC
    epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST", "NO");
    epicsEnvSet("EPICS_CA_ADDR_LIST", "localhost");
    epicsEnvSet("EPICS_CA_SERVER_PORT", "55064");
    epicsEnvSet("EPICS_CAS_BEACON_PORT", "55065");
    epicsEnvSet("EPICS_CAS_INTF_ADDR_LIST", "localhost");

    // Start test IOC
    testDiag("Starting test IOC");
    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("../caPutJsonLogTest.db", NULL, NULL);
    iocLogPrefix(logMsgPrefix);
    asSetFilename("../asg.cfg");
    startIoc();
    logger =  CaPutJsonLogTask::getInstance();
    if (logger == NULL) testAbort("Failed to initialize logger.");
    logger->initialize(logServerAddress.c_str(), caPutJsonLogOnChange);
    testDiag("Test IOC ready");
    testIocReady.trigger();

    // Wait for tests to finish
    testThreadDone.wait();
    epics::atomic::set(stopLogServer, true);
    logServerThreadDone.wait();

    testDiag("Done");
    return testDone();
}
