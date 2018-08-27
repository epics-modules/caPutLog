#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure
DIRS += caPutLogApp

caPutLogApp_DEPEND_DIRS = configure

# Allow 'make docs' but don't otherwise descend into it
ifeq ($(MAKECMDGOALS),docs)
  DIRS += docs
endif

include $(TOP)/configure/RULES_TOP
