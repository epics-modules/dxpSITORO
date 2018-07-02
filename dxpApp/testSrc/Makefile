TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library


USR_CFLAGS_Linux += -std=c99 

# Test applications
PROD_IOC_Linux     += hd-board-info
PROD_IOC_WIN32     += hd-board-info
hd-board-info_SRCS += hd-board-info.c

PROD_IOC_Linux     += hd-serial-num
PROD_IOC_WIN32     += hd-serial-num
hd-serial-num_SRCS += hd-serial-num.c

PROD_IOC_Linux           += hd-det-characterize
PROD_IOC_WIN32           += hd-det-characterize
hd-det-characterize_SRCS += hd-det-characterize.c

PROD_IOC_Linux     += hd-dc-pulses
PROD_IOC_WIN32     += hd-dc-pulses
hd-dc-pulses_SRCS  += hd-dc-pulses.c

PROD_IOC_Linux     += hd-adc-trace
PROD_IOC_WIN32     += hd-adc-trace
hd-adc-trace_SRCS  += hd-adc-trace.c

PROD_IOC_Linux     += hd-mca
PROD_IOC_WIN32     += hd-mca
hd-mca_SRCS        += hd-mca.c

PROD_IOC_Linux     += hd-mca-preset
PROD_IOC_WIN32     += hd-mca-preset
hd-mca-preset_SRCS += hd-mca-preset.c

PROD_IOC_Linux      += hd-mca-preset1
PROD_IOC_WIN32      += hd-mca-preset1
hd-mca-preset1_SRCS += hd-mca-preset1.c

PROD_IOC_Linux     += hd-mm1
PROD_IOC_WIN32     += hd-mm1
hd-mm1_SRCS        += hd-mm1.c

PROD_IOC_Linux     += hd-mm1-trace
PROD_IOC_WIN32     += hd-mm1-trace
hd-mm1-trace_SRCS  += hd-mm1-trace.c

PROD_IOC_Linux     += hd-run-spec
PROD_IOC_WIN32     += hd-run-spec
hd-run-spec_SRCS   += hd-run-spec.c

PROD_IOC_Linux     += hd-get-acq
PROD_IOC_WIN32     += hd-get-acq
hd-get-acq_SRCS    += hd-get-acq.c

PROD_IOC_Linux     += hd-set-acq
PROD_IOC_WIN32     += hd-set-acq
hd-set-acq_SRCS    += hd-set-acq.c

PROD_IOC_Linux      += hd-save-system
PROD_IOC_WIN32      += hd-save-system
hd-save-system_SRCS += hd-save-system.c

PROD_IOC_Linux     += hd-sca
PROD_IOC_WIN32     += hd-sca
hd-sca_SRCS        += hd-sca.c

PROD_IOC_Linux     += hd-connected
PROD_IOC_WIN32     += hd-connected
hd-connected_SRCS  += hd-connected.c

PROD_LIBS += handelSITORO

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

