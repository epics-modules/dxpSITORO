TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

INC += inc/handel.h
INC += inc/xia_common.h
INC += inc/handeldef.h
INC += inc/handel_errors.h
INC += inc/handel_constants.h
INC += inc/handel_generic.h
INC += inc/md_generic.h

USR_INCLUDES += -I$(TOP)/dxpApp/handel/inc -I$(TOP)/dxpApp/handel/sincSrc -I$(TOP)/dxpApp/handel/miniz

USR_CFLAGS_Linux += -std=c99 
USR_CFLAGS_WIN32 += -DWIN32 -DWIN32_LEAN_AND_MEAN

ifeq ($(SHARED_LIBRARIES),YES)
  USR_CFLAGS_WIN32 += -DHANDEL_MAKE_DLL
endif

LIBRARY_IOC_Linux    += handelSITORO
LIBRARY_IOC_WIN32    += handelSITORO

SRC_DIRS += $(TOP)/dxpApp/handel/src
handelSITORO_SRCS += handel.c
handelSITORO_SRCS += handel_detchan.c
handelSITORO_SRCS += handel_dyn_detector.c
handelSITORO_SRCS += handel_dyn_module.c
handelSITORO_SRCS += handel_file.c
handelSITORO_SRCS += handel_run_params.c
handelSITORO_SRCS += handel_system.c
handelSITORO_SRCS += handel_dbg.c
handelSITORO_SRCS += handel_dyn_default.c
handelSITORO_SRCS += handel_dyn_firmware.c
handelSITORO_SRCS += handel_fdd_shim.c
handelSITORO_SRCS += handel_log.c
handelSITORO_SRCS += handel_run_control.c
handelSITORO_SRCS += handel_sort.c
handelSITORO_SRCS += xia_assert.c
handelSITORO_SRCS += xia_file.c
handelSITORO_SRCS += xia_sio.c
handelSITORO_SRCS += md_shim.c
handelSITORO_SRCS += falconx_mm.c
handelSITORO_SRCS += falconxn_psl.c
handelSITORO_SRCS += psl.c

handelSITORO_SRCS_Linux += handel_md_threads_posix.c
handelSITORO_SRCS_WIN32 += handel_md_threads_windows.c

SRC_DIRS += $(TOP)/dxpApp/handel/libsinc-c
handelSITORO_SRCS += api.c
handelSITORO_SRCS += base64.c
handelSITORO_SRCS += blocking.c
handelSITORO_SRCS += command.c
handelSITORO_SRCS += decode.c
handelSITORO_SRCS += encapsulation.c
handelSITORO_SRCS += encode.c
handelSITORO_SRCS += readmessage.c
handelSITORO_SRCS += request.c
handelSITORO_SRCS += sinc.pb-c.c
handelSITORO_SRCS += socket.c
handelSITORO_SRCS += protobuf-c.c

SRC_DIRS += $(TOP)/dxpApp/handel/miniz
handelSITORO_SRCS += miniz.c
handelSITORO_SRCS += miniz_tdef.c
handelSITORO_SRCS += miniz_tinfl.c

handelSITORO_SYS_LIBS_WIN32 += ws2_32
handelSITORO_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

