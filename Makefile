#Makefile at top of application tree
TOP = .
include $(TOP)/config/CONFIG_APP
DIRS += config
DIRS += $(wildcard *App)
DIRS += $(wildcard iocBoot)
include $(TOP)/config/RULES_TOP
