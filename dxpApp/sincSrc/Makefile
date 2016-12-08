TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

INC += protobuf-c.h
INC += sinc.h
INC += sinc.pb-c.h

LIBRARY_IOC_Linux    += sinc
LIBRARY_IOC_WIN32    += sinc

sinc_SRCS += api.c

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

