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

LIBRARY_IOC_Linux  += sinc
LIBRARY_IOC_WIN32  += sinc

sinc_SRCS += api.c
sinc_SRCS += blocking.c
sinc_SRCS += command.c
sinc_SRCS += decode.c
sinc_SRCS += encapsulation.c
sinc_SRCS += encode.c
sinc_SRCS += readmessage.c
sinc_SRCS += request.c
sinc_SRCS += sinc.pb-c.c
sinc_SRCS += socket.c
sinc_SRCS += protobuf-c.c

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

