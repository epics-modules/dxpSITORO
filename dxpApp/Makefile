TOP = ..
include $(TOP)/configure/CONFIG
DIRS += sincSrc

DIRS += handelSrc
handelSrc_DEPEND_DIRS = sincSrc

DIRS += src
src_DEPEND_DIRS = handelSrc

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

