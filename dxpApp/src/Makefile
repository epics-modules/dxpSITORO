TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_Linux += dxp
LIBRARY_IOC_WIN32 += dxp

DBD += dxpSupport.dbd

# The following are compiled and added to the library
dxp_SRCS += NDDxp.cpp
dxp_SRCS += dxpMED.st

dxp_LIBS += handel sinc
dxp_LIBS += asyn
dxp_LIBS += mca
dxp_LIBS += ADBase
dxp_LIBS += seq pv
dxp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application
PROD_NAME = dxpApp
PROD_IOC_Linux = $(PROD_NAME)
PROD_IOC_WIN32 = $(PROD_NAME)

DBD += $(PROD_NAME).dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
dxpApp_SRCS += dxpMain.cpp dxpApp_registerRecordDeviceDriver.cpp

dxpApp_LIBS += dxp handel
dxpApp_LIBS += mca
dxpApp_LIBS += seq pv

include $(ADCORE)/ADApp/commonDriverMakefile
$(PROD_NAME)_DBD += mcaSupport.dbd
$(PROD_NAME)_DBD += dxpSupport.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

