TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

LIBRARY_IOC = caPutLog

USR_CPPFLAGS += -DUSE_TYPED_RSET

caPutLog_SRCS += caPutLogTask.c
caPutLog_SRCS += caPutLogAs.c
caPutLog_SRCS += caPutLogClient.c
caPutLog_SRCS += caPutLog.c
caPutLog_SRCS += caPutLogShellCommands.c

# API for the IOC
INC = caPutLog.h

# formerly internal API exposed for non-IOC
# servers (like the CA-Gateway)
INC += caPutLogTask.h
INC += caPutLogAs.h

DBD += caPutLog.dbd

# Add support for json format and arrays
# This requires EPICS base version 7.0.1 or higher
ifdef BASE_7_0
USR_CPPFLAGS += -DJSON_AND_ARRAYS_SUPPORTED
caPutLog_SRCS += caPutJsonLogTask.cpp
caPutLog_SRCS += caPutJsonLogShellCommands.cpp
INC += caPutJsonLogTask.h
DBD += caPutJsonLog.dbd
endif


caPutLog_LIBS += $(EPICS_BASE_IOC_LIBS)
caPutLog_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
