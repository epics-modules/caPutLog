TOP = ..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

DBDDEPENDS_FILES += dbTestIoc.dbd$(DEP)
TARGETS += $(COMMON_DIR)/dbTestIoc.dbd
dbTestIoc_DBD += base.dbd

# Libraries to which the test executable is linked
PROD_LIBS += Com
PROD_LIBS += dbCore
PROD_LIBS += dbRecStd
PROD_LIBS += ca
PROD_LIBS += caPutLog

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