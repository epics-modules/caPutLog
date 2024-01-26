TOP = ..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DBDDEPENDS_FILES += dbTestIoc.dbd$(DEP)
TARGETS += $(COMMON_DIR)/dbTestIoc.dbd
dbTestIoc_DBD += base.dbd
CXXFLAGS=-std=c++11

# Libraries to which the test executable is linked
PROD_LIBS = caPutLog
PROD_LIBS += $(EPICS_BASE_IOC_LIBS)
PROD_SYS_LIBS_WIN32 += ws2_32

testHarness_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp

# Build test executable for JSON logger (requires EPICS base 7.0.1+)
ifdef BASE_7_0
TESTPROD_HOST += caPutJsonLogTest
caPutJsonLogTest_SRCS += caPutJsonLogTest.cpp
caPutJsonLogTest_SRCS += dbTestIoc_registerRecordDeviceDriver.cpp
testHarness_SRCS += caPutJsonLogTest.cpp
TESTFILES += $(COMMON_DIR)/dbTestIoc.dbd
TESTFILES += ../caPutJsonLogTest.db
TESTFILES += ../asg.cfg
TESTS += caPutJsonLogTest
endif

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
