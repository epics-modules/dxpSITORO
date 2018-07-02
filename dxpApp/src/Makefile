TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC_Linux += dxpSITORO
LIBRARY_IOC_WIN32 += dxpSITORO

DBD += dxpSITOROSupport.dbd

# The following are compiled and added to the library
dxpSITORO_SRCS += NDDxp.cpp
dxpSITORO_SRCS += dxpMED.st

dxpSITORO_LIBS += handelSITORO
dxpSITORO_LIBS += asyn
dxpSITORO_LIBS += mca
dxpSITORO_LIBS += ADBase
dxpSITORO_LIBS += seq pv
dxpSITORO_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application
PROD_NAME = dxpSITOROApp
PROD_IOC_Linux = $(PROD_NAME)
PROD_IOC_WIN32 = $(PROD_NAME)

DBD += $(PROD_NAME).dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
$(PROD_NAME)_SRCS += dxpMain.cpp $(PROD_NAME)_registerRecordDeviceDriver.cpp

$(PROD_NAME)_LIBS += dxpSITORO handelSITORO
$(PROD_NAME)_LIBS += mca
$(PROD_NAME)_LIBS += seq pv

include $(ADCORE)/ADApp/commonDriverMakefile
$(PROD_NAME)_DBD += mcaSupport.dbd
$(PROD_NAME)_DBD += dxpSITOROSupport.dbd

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

