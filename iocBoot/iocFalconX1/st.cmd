< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DXP_SITORO)/dbd/dxpSITOROApp.dbd")
dxpSITOROApp_registerRecordDeviceDriver(pdbbase)

# Prefix for all records
epicsEnvSet("PREFIX", "dxpSITORO:")
# The port name for the detector
epicsEnvSet("PORT",   "DXP1")
# The queue size for all plugins
epicsEnvSet("QSIZE",  "20")
# The maximum image width; used to set the maximum size for this driver and for row profiles in the NDPluginStats plugin
epicsEnvSet("XSIZE",  "2048")
# The maximum image height; used to set the maximum size for this driver and for column profiles in the NDPluginStats plugin
epicsEnvSet("YSIZE",  "2048")
# The maximum number of time series points in the NDPluginStats plugin
epicsEnvSet("NCHANS", "2048")
# The maximum number of frames buffered in the NDPluginCircularBuff plugin
epicsEnvSet("CBUFFS", "500")
# The maximum number of threads for plugins which can run in multiple threads
epicsEnvSet("MAX_THREADS", "8")
# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

asynSetMinTimerPeriod(0.001)

# The default callback queue in EPICS base is only 2000 bytes. 
# The dxp detector system needs this to be larger to avoid the error message: 
# "callbackRequest: cbLow ring buffer full" 
# right after the epicsThreadSleep at the end of this script
callbackSetQueueSize(4000)

# Setup for save_restore
< ../save_restore.cmd
save_restoreSet_status_prefix("$(PREFIX)")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(PREFIX)")

# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("falconxn1.ini")
xiaStartSystem

# DXPConfig(serverName, ndetectors, maxBuffers, maxMemory)
NDDxpConfig($(PORT),  1, 0, 0)

dbLoadTemplate("1element.substitutions")

# Create a standard arrays plugin. driver.
NDStdArraysConfigure("Image1", 20, 0, "$(PORT)", 0, 0, 0, 0, 0, 5)
# This creates a waveform large enough for max. buffer size (~4.5 MB) times 4 detectors
# This waveform only allows transporting 8-bit images
dbLoadRecords("NDStdArrays.template", "P=$(PREFIX),R=image1:,PORT=Image1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT),TYPE=Int32,FTVL=LONG,NELEMENTS=20000000")

< $(ADCORE)/iocBoot/commonPlugins.cmd

#xiaSetLogLevel(4)
#asynSetTraceMask $(PORT) 0 0x11
#asynSetTraceIOMask $(PORT) 0 2

iocInit

seq dxpMED, "P=$(PREFIX), DXP=dxp, MCA=mca, N_DETECTORS=1, N_SCAS=4"

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX)")

### Start the saveData task.
saveData_Init("saveData.req", "P=$(PREFIX)")

