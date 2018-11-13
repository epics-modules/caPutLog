TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

LIBRARY_IOC = caPutLog

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

caPutLog_LIBS += $(EPICS_BASE_IOC_LIBS)
caPutLog_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
