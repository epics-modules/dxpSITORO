TOP = ..
include $(TOP)/configure/CONFIG

DIRS += handelSrc

DIRS += testSrc
testSrc_DEPEND_DIRS = handelSrc

DIRS += src
src_DEPEND_DIRS = handelSrc

DIRS += op

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

