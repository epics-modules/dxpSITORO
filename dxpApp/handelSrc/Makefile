TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

INC += handel.h

# This application can be built on Linux (EPP, USB) WIN32 (EPP, USB, PXI), and vxWorks (CAMAC)
LIBRARY_IOC_Linux    += handel
LIBRARY_IOC_WIN32    += handel

handel_SRCS += falconx_mm.c
handel_SRCS += falconx_psl.c
handel_SRCS += falconxn_psl.c
handel_SRCS += handel.c
handel_SRCS += handel_dbg.c
handel_SRCS += handel_detchan.c

handel_SYS_LIBS_WIN32    += setupapi User32
handel_SYS_LIBS_cygwin32 += setupapi User32
handel_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

