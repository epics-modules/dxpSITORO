TOP = ..
include $(TOP)/configure/CONFIG

DIRS += handel

DIRS += src
src_DEPEND_DIRS = handel

#DIRS += handel/tests
#testSrc_DEPEND_DIRS = handel

DIRS += op

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

