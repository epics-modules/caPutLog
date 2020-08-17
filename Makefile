#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure
DIRS += caPutLogApp
DIRS += test

caPutLogApp_DEPEND_DIRS = configure
test_DEPEND_DIRS = caPutLogApp

# Allow 'make docs' but don't otherwise descend into it
ifeq ($(MAKECMDGOALS),docs)
  DIRS += docs
endif

include $(TOP)/configure/RULES_TOP
