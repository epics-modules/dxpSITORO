TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

INC += handel.h
INC += xia_common.h
INC += handeldef.h
INC += handel_errors.h
INC += handel_constants.h
INC += handel_generic.h
INC += md_generic.h

USR_CFLAGS_Linux += -std=c99 
USR_CFLAGS_WIN32 += -DWIN32 

ifeq ($(SHARED_LIBRARIES),YES)
  USR_CFLAGS_WIN32 += -DSITORO_EXPORTS
else
  USR_CFLAGS_WIN32 += -DSITORO_NO_IMPORTEXPORT
endif

LIBRARY_IOC_Linux    += handel
LIBRARY_IOC_WIN32    += handel

handel_SRCS += handel.c
handel_SRCS += handel_detchan.c
handel_SRCS += handel_dyn_detector.c
handel_SRCS += handel_dyn_module.c
handel_SRCS += handel_file.c
handel_SRCS += handel_run_params.c
handel_SRCS += handel_system.c
handel_SRCS += handel_dbg.c
handel_SRCS += handel_dyn_default.c
handel_SRCS += handel_dyn_firmware.c
handel_SRCS += handel_fdd_shim.c
handel_SRCS += handel_log.c
handel_SRCS += handel_run_control.c
handel_SRCS += handel_sort.c
handel_SRCS += xia_assert.c
handel_SRCS += xia_file.c
handel_SRCS += xia_sio.c
handel_SRCS += md_shim.c
handel_SRCS += miniz.c
handel_SRCS += falconx_mm.c
handel_SRCS += falconxn_psl.c
handel_SRCS += psl.c
handel_SRCS += sitoro-stub.c

handel_SRCS_Linux += handel_md_threads_posix.c
handel_SRCS_WIN32 += handel_md_threads_windows.c

handel_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

