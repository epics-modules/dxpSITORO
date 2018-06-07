/********************************************************************
 ***                                                              ***
 ***                libsinc project file load/save                ***
 ***                                                              ***
 ********************************************************************/

/*
 * The project file subsystem allows the entire state of the card to be
 * saved and loaded from the device. The files are stored with a .siprj
 * extension by convention and are formatted as JSON internally.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <math.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <float.h> // LDBL_DIG should be defined in float.h.
#else
#include <winsock2.h>
#endif

#ifndef LDBL_DIG
    #define LDBL_DIG 17 // LDBL_DIG is used to guarantee that there is no precision loss
                        // when converting from double to string and back again.
#endif


#include "sinc.h"
#include "sinc_internal.h"
#include "jsmn.h"


/* The maximum number of channels we support. */
#define MAX_CHANNELS 36

/* The maximum length of the firmware version string. */
#define FIRMWARE_VERSION_MAX 80


/* The data needed to calibrate one channel. */
struct ChannelCalibration
{
    SincCalibrationData calibrationData;
    SincCalibrationPlot examplePulse;
    SincCalibrationPlot modelPulse;
    SincCalibrationPlot finalPulse;
};


/* The device settings are used to accumulate parameters for setting. */
struct DeviceSettings
{
    /* The parameters we need to set. */
    int                       paramsAllocated;
    int                       numParams;
    SiToro__Sinc__KeyValue   *params;

    /* Strings used by the above parameters. */
    int                       stringsAllocated;
    int                       numStrings;
    char                    **strings;

    /* Calibrations. */
    struct ChannelCalibration calib[MAX_CHANNELS];
};


/*
 * NAME:        initChannelCalibation
 * ACTION:      Initialises a channel calibration structure.
 * PARAMETERS:  struct ChannelCalibration *cc - the structure to initialise.
 */

static void initChannelCalibration(struct ChannelCalibration *cc)
{
    memset(cc, 0, sizeof(*cc));
}


/*
 * NAME:        closeChannelCalibration
 * ACTION:      Frees any storage used by channel calibration.
 * PARAMETERS:  struct ChannelCalibration *cc - the structure to initialise.
 */

static void closeChannelCalibration(struct ChannelCalibration *cc)
{
    SincSFreeCalibration(&cc->calibrationData, &cc->examplePulse, &cc->modelPulse, &cc->finalPulse);
}


/*
 * NAME:        initDeviceSettings
 * ACTION:      Initialises the DeviceSettings structure.
 * PARAMETERS:  struct DeviceSettings *settings - the structure to initialise.
 */

static void initDeviceSettings(struct DeviceSettings *settings)
{
    // Initialise the device settings struct. */
    int i;

    settings->paramsAllocated = 1000;
    settings->numParams = 0;
    settings->params = calloc((size_t)settings->paramsAllocated, sizeof(SiToro__Sinc__KeyValue));
    for (i = 0; i < settings->paramsAllocated; i++)
    {
        si_toro__sinc__key_value__init(&settings->params[i]);
    }

    /* Initialise string storage. */
    settings->stringsAllocated = 1000;
    settings->numStrings = 0;
    settings->strings = calloc((size_t)settings->stringsAllocated, sizeof(char *));

    /* Initialise calibration data. */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        initChannelCalibration(&settings->calib[i]);
    }
}


/*
 * NAME:        closeDeviceSettings
 * ACTION:      Frees any storage used by the device settings.
 * PARAMETERS:  struct DeviceSettings *settings - the structure to initialise.
 */

static void closeDeviceSettings(struct DeviceSettings *settings)
{
    int i;

    free(settings->params);

    for (i = 0; i < settings->numStrings; i++)
    {
        free(settings->strings[i]);
    }

    free(settings->strings);

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        closeChannelCalibration(&settings->calib[i]);
    }
}


/*
 * NAME:        storeDeviceSettingsString
 * ACTION:      Store a string in the device settings. This will be freed
 *              automatically when the closeDeviceSettings() is called.
 * PARAMETERS:  struct DeviceSettings *settings - the structure to initialise.
 *              const char *str - the string to store.
 * RETURNS:     char * - pointer to the newly allocated string buffer.
 */

static char *storeDeviceSettingsString(struct DeviceSettings *settings, const char *str)
{
    int   i;
    char *strBuf;

    /* Make sure there's enough space. */
    if (settings->numStrings >= settings->stringsAllocated)
    {
        int newStringsAllocated = settings->stringsAllocated * 2;
        char **mem = realloc(settings->strings, (size_t)newStringsAllocated * sizeof(char *));
        if (mem == NULL)
            return NULL;

        settings->strings = mem;

        for (i = settings->stringsAllocated; i < newStringsAllocated; i++)
        {
            settings->strings[i] = NULL;
        }

        settings->stringsAllocated = newStringsAllocated;
    }

    /* Store the string in a newly allocated bit of memory. */
    strBuf = malloc(strlen(str) + 1);
    if (strBuf == NULL)
        return NULL;

    strcpy(strBuf, str);
    settings->strings[settings->numStrings] = strBuf;
    settings->numStrings++;

    return strBuf;
}


/*
 * NAME:        readFileAsString
 * ACTION:      Load in the contents of a file and return it as a malloc()ed string.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              char **fileText - the resulting text is returned here.
 *              const char *fileName - the name of the file to read.
 * RETURNS:     bool - true on success, false otherwise.
 */

static bool readFileAsString(Sinc *sc, char **fileText, const char *fileName)
{
    FILE *f;
    size_t fileSize;
    char *readBuf;

    // Open the file.
    f = fopen(fileName, "rt");
    if (f == NULL)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't open file");
        return false;
    }

    // How long is the file?
    if (fseek(f, 0 , SEEK_END) != 0)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't seek file");
        return false;
    }

    fileSize = (size_t)ftell(f);

    // Go back to the start.
    if (fseek(f, 0 , SEEK_SET) != 0)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't seek file");
        return false;
    }

    // Allocate memory for the read buffer.
    readBuf = (char*)calloc(fileSize+1, 1);
    if (readBuf == NULL)
    {
        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Read the file.
    if (fread(readBuf, 1, fileSize, f) != fileSize)
    {
        free(readBuf);
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't read file");
        return false;
    }

    fclose(f);

    *fileText = readBuf;

    return true;
}


/*
 * NAME:        jsmnError
 * ACTION:      Display an error from the json parser.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              int errCode - the jsmn error code.
 */

static void jsmnError(Sinc *sc, int errCode)
{
    switch (errCode)
    {
    case JSMN_ERROR_NOMEM:
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY, "out of memory");
        break;

    case JSMN_ERROR_INVAL:
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "invalid character in project file");
        break;

    case JSMN_ERROR_PART:
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "incomplete json in project file");
        break;

    default:
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "unknown json parse error in project file");
        break;
    }
}


/*
 * NAME:        findParamDetails
 * ACTION:      Find a named parameter in the parameter details response.
 * PARAMETERS:  SiToro__Sinc__ListParamDetailsResponse *pdResp - the parameter details from the board.
 *              const char *key - the name of the parameter to find.
 * RETURNS:     SiToro__Sinc__ParamDetails * - the found details or NULL if not found.
 */

static SiToro__Sinc__ParamDetails *findParamDetails(SiToro__Sinc__ListParamDetailsResponse *pdResp, const char *key)
{
    size_t i;

    /* Loop through all the params. */
    for (i = 0; i < pdResp->n_paramdetails; i++)
    {
        SiToro__Sinc__ParamDetails *pd = pdResp->paramdetails[i];
        if (pd->kv != NULL && pd->kv->key && strcmp(pd->kv->key, key) == 0)
        {
            /* Found it. */
            return pd;
        }
    }

    return NULL;
}


/*
 * NAME:        tokenToCStr
 * ACTION:      Copy a token to a C string. Will be truncated if it doesn't fit the buffer.
 * PARAMETERS:  char *buf           - the C string to populate.
 *              int bufLen          - the number of bytes available in the buffer.
 *              const char *jsonStr - the source json text.
 *              jsmntok_t *token - the token info.
 */

static void tokenToCStr(char *buf, int bufLen, const char *jsonStr, jsmntok_t *token)
{
    int useLen = token->end - token->start;

    if (useLen > bufLen - 1)
    {
        useLen = bufLen - 1;
    }

    strncpy(buf, &jsonStr[token->start], (size_t)useLen);
    buf[useLen] = 0;
}


/*
 * NAME:        tokenCmp
 * ACTION:      Compare a token string to a C string.
 * PARAMETERS:  const char *jsonStr - the source json text.
 *              jsmntok_t *token - the token info.
 *              const char *str - the string to compare against.
 * RETURNS:     true if the strings are equal, false otherwise.
 */

static bool tokenCmp(const char *jsonStr, jsmntok_t *token, const char *str)
{
    size_t tokLen = (size_t)(token->end - token->start);
    if (tokLen != strlen(str))
        return false;

    return strncmp(&jsonStr[token->start], str, tokLen) == 0;
}


/*
 * NAME:        addParamToSettings
 * ACTION:      Add a single parameter to the settings to store. Uses the parameter's
 *              already known type to store the correct data type.
 * PARAMETERS:  SiToro__Sinc__ParamDetails *pd - what we know about this key.
 *              struct DeviceSettings *settings - this is where we accumulate the settings to store.
 *              int channelId - what channel this setting is for (or -1 if none).
 *              const char *key - the key that's being set.
 *              const char *valStr - the value that's being set in json text form.
 */

static void addParamToSettings(SiToro__Sinc__ParamDetails *pd, struct DeviceSettings *settings, int channelId, const char *key, const char *valStr)
{
    SiToro__Sinc__KeyValue *kv;

    /* Will it fit in the parameters vector? */
    if (settings->numParams + 1 < settings->paramsAllocated)
    {
        /* Expand the parameters vector. */
        int i;
        int newParamsAllocated = settings->paramsAllocated * 2;
        SiToro__Sinc__KeyValue *mem = realloc(settings->params, (size_t)newParamsAllocated * sizeof(SiToro__Sinc__KeyValue));

        if (mem == NULL)
            return;

        settings->params = mem;

        for (i = settings->paramsAllocated; i < newParamsAllocated; i++)
        {
            si_toro__sinc__key_value__init(&settings->params[i]);
        }
    }

    /* Set this value. */
    kv = &settings->params[settings->numParams];

    if (channelId >= 0)
    {
        kv->has_channelid = true;
        kv->channelid = channelId;
    }

    kv->key = storeDeviceSettingsString(settings, key);
    if (kv->key == NULL)
        return;

    switch (pd->kv->paramtype)
    {
    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE:
        /* Boolean. */
        kv->has_boolval = true;
        kv->boolval = strcmp(valStr, "true") == 0;
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE:
        /* Integer. */
        kv->has_intval = true;
        kv->intval = atoi(valStr);
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE:
        /* Float. */
        kv->has_floatval = true;
        kv->floatval = strtod(valStr, NULL);

        /* Clamp problems with pulse.detectionThreshold. */
        if (strcmp(key, "pulse.detectionThreshold") == 0 && kv->floatval < 0)
        {
            kv->floatval = 0.0;
        }
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE:
        /* String. */
        kv->strval = storeDeviceSettingsString(settings, valStr);
        if (kv->strval == NULL)
            return;
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE:
        /* Option. */
        kv->optionval = storeDeviceSettingsString(settings, valStr);
        if (kv->optionval == NULL)
            return;
        break;

    default:
        /* Invalid. */
        return;
    }

    settings->numParams++;
}


/*
 * NAME:        traverseJsonCalibrationVector
 * ACTION:      Parses the JSON tokens by recursive descent.
 */

static bool traverseJsonCalibrationVector(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos, jsmntok_t *keyToken, struct DeviceSettings *settings, int channelId)
{
    int i;
    struct ChannelCalibration *cc;

    jsmntok_t *tok2 = &tokens[*tokPos];
    (*tokPos)++;

    /* Disallow invalid channels. */
    if (channelId < 0 || channelId >= MAX_CHANNELS)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - invalid channel id");
        return false;
    }

    cc = &settings->calib[channelId];

    /* What key is it? */
    if (tokenCmp(jsonStr, keyToken, "calibration.data"))
    {
        /* It's the base64 encoded calibration data. */
        char calibStr64[256];
        uint8_t calibBin[256];
        size_t calibBinLen;

        /* The encoded calibration data is a base64-encoded string. */
        if (tok2->type != JSMN_STRING)
        {
            SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - calibration data must be a string");
            return false;
        }

        /* Decode from base64 to binary. */
        tokenToCStr(calibStr64, sizeof(calibStr64), jsonStr, tok2);

        calibBinLen = sizeof(calibBin);
        Base64Decode(calibStr64, strlen(calibStr64), calibBin, &calibBinLen);

        /* Put it in the calibration settings. */
        uint8_t *mem = calloc(calibBinLen, 1);
        if (mem == NULL)
        {
            SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
            return false;
        }

        cc->calibrationData.data = mem;
        memcpy(cc->calibrationData.data, calibBin, calibBinLen);
        cc->calibrationData.len = (int)calibBinLen;
    }
    else
    {
        /* The calibration pulse shapes are arrays of floats. */
        SincCalibrationPlot *plot = NULL;

        /* Select the pulse to modify. */
        if (tokenCmp(jsonStr, keyToken, "calibration.exampleShape.y"))
            plot = &cc->examplePulse;

        else if (tokenCmp(jsonStr, keyToken, "calibration.modelShape.y"))
            plot = &cc->modelPulse;

        else if (tokenCmp(jsonStr, keyToken, "calibration.finalShape.y"))
            plot = &cc->finalPulse;

        /* Is the data valid? */
        if (tok2->type != JSMN_ARRAY)
        {
            SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - calibration vector must be an array");
            return false;
        }

        /* Create the plot. */
        if (plot != NULL)
        {
            plot->len = tok2->size;
            plot->x = calloc((size_t)plot->len, sizeof(*plot->x));
            plot->y = calloc((size_t)plot->len, sizeof(*plot->y));

            /* Fill in the X values. */
            for (i = 0; i < plot->len; i++)
                plot->x[i] = i;
        }

        for (i = 0; i < tok2->size; i++)
        {
            /* Get a single calibration value at a time. */
            jsmntok_t *tok3 = &tokens[*tokPos];
            (*tokPos)++;

            if (tok3->type != JSMN_PRIMITIVE)
            {
                SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - calibration vector values much be floats");
                return false;
            }

            /* Set this value in the result vector. */
            if (plot != NULL)
            {
                char valStr[30];

                tokenToCStr(valStr, sizeof(valStr), jsonStr, tok3);
                plot->y[i] = strtod(valStr, NULL);
            }
        }
    }

    return true;
}


/*
 * NAME:        traverseJsonRegionParam
 * ACTION:      Parses the JSON tokens by recursive descent.
 */

static bool traverseJsonRegionParam(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos)
{
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_STRING)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected name of region object");
        return false;
    }

    /* It's a single token parameter. */
    //jsmntok_t *tok2 = &tokens[*tokPos];
    (void)jsonStr;
    (*tokPos)++;

    return true;
}


/*
 * NAME:        traverseJsonSingleRegion
 * ACTION:      Parses the JSON tokens by recursive descent
 */

static bool traverseJsonSingleRegion(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos)
{
    int i;
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_OBJECT)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected channel to be an object");
        return false;
    }

    for (i = 0; i < tok->size; i++)
    {
        if (!traverseJsonRegionParam(sc, jsonStr, tokens, tokPos))
            return false;
    }

    return true;
}


/*
 * NAME:        traverseJsonRegions
 * ACTION:      Parses the JSON tokens by recursive descent.
 */

static bool traverseJsonRegions(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos)
{
    int i;
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    /* Regions is an array of keys. */
    if (tok->type != JSMN_ARRAY)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - regions must be an array");
        return false;
    }

    for (i = 0; i < tok->size; i++)
    {
        /* Get a single region value. */
        if (!traverseJsonSingleRegion(sc, jsonStr, tokens, tokPos))
            return false;
    }

    return true;
}


/*
 * NAME:        traverseJsonChannelParam
 * ACTION:      Parses the JSON tokens by recursive descent
 */

static bool traverseJsonChannelParam(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos, SiToro__Sinc__ListParamDetailsResponse *deviceParams, struct DeviceSettings *settings, int *channelId)
{
    static const char *calibPrefix = "calibration.";
    size_t calibPrefixLen = strlen(calibPrefix);
    char key[256];
    char valStr[256];

    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_STRING)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected name of object");
        return false;
    }

    if ((size_t)(tok->end - tok->start) > calibPrefixLen && strncmp(&jsonStr[tok->start], calibPrefix, calibPrefixLen) == 0)
    {
        /* Get a calibration vector. */
        if (!traverseJsonCalibrationVector(sc, jsonStr, tokens, tokPos, tok, settings, *channelId))
            return false;
    }
    else if (tokenCmp(jsonStr, tok, "regions"))
    {
        /* Get a regions section. */
        if (!traverseJsonRegions(sc, jsonStr, tokens, tokPos))
            return false;
    }
    else
    {
        /* It's a single token parameter. */
        jsmntok_t *tok2 = &tokens[*tokPos];
        (*tokPos)++;

        /* Get the key and value as normal strings. */
        tokenToCStr(key, sizeof(key), jsonStr, tok);
        tokenToCStr(valStr, sizeof(valStr), jsonStr, tok2);

        if (strcmp(key, "_channel") == 0 || strcmp(key, "_channelId") == 0)
        {
            /* Get the channel id. */
            if (tok2->type != JSMN_PRIMITIVE)
            {
                SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - channel id must be a number");
                return false;
            }

            *channelId = atoi(valStr);
        }
        else
        {
            /* Get the parameter details for this parameter. */
            SiToro__Sinc__ParamDetails *pd = findParamDetails(deviceParams, key);
            if (pd != NULL && (!pd->has_instrumentlevel || !pd->instrumentlevel) && pd->has_settable && pd->settable)
            {
                addParamToSettings(pd, settings, *channelId, key, valStr);
            }
        }
    }

    return true;
}


/*
 * NAME:        traverseJsonChannel
 * ACTION:      Parses the JSON tokens by recursive descent
 */

static bool traverseJsonChannel(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos, SiToro__Sinc__ListParamDetailsResponse *deviceParams, struct DeviceSettings *settings)
{
    int channelId = 0;
    int i;
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_OBJECT)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected channel to be an object");
        return false;
    }

    for (i = 0; i < tok->size; i++)
    {
        if (!traverseJsonChannelParam(sc, jsonStr, tokens, tokPos, deviceParams, settings, &channelId))
            return false;
    }

    return true;
}


/*
 * NAME:        traverseJsonInstrumentParam
 * ACTION:      Parses the JSON tokens by recursive descent
 */

static bool traverseJsonInstrumentParam(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos, SiToro__Sinc__ListParamDetailsResponse *deviceParams, struct DeviceSettings *settings, char *saveFileFirmwareVersion)
{
    SiToro__Sinc__ParamDetails *pd;
    int i;
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_STRING)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected name of object");
        return false;
    }

    jsmntok_t *tok2 = &tokens[*tokPos];
    (*tokPos)++;

    if (tokenCmp(jsonStr, tok, "channels"))
    {
        /* A channel configuration item. */
        if (tok2->type != JSMN_ARRAY)
        {
            SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - 'channels' should be an array");
            return false;
        }

        for (i = 0; i < tok2->size; i++)
        {
            if (!traverseJsonChannel(sc, jsonStr, tokens, tokPos, deviceParams, settings))
                return false;
        }
    }
    else
    {
        /* An instrument level key/value. */
        char key[256];
        char valStr[256];

        /* Get the key and value as normal strings. */
        tokenToCStr(key, sizeof(key), jsonStr, tok);
        tokenToCStr(valStr, sizeof(valStr), jsonStr, tok2);

        /* Get the parameter details for this parameter. */
        pd = findParamDetails(deviceParams, key);
        if (pd != NULL && pd->has_instrumentlevel && pd->instrumentlevel)
        {
            if (pd->has_settable && pd->settable)
            {
                addParamToSettings(pd, settings, -1, key, valStr);
            }
            else if (strcmp(key, "instrument.firmwareVersion") == 0 && pd->kv->strval)
            {
                /* Take note of the firmware version separately so we can do parameter
                 * upgrades based on the old firmware version. */
                memset(saveFileFirmwareVersion, 0, FIRMWARE_VERSION_MAX);
                strncpy(saveFileFirmwareVersion, pd->kv->strval, FIRMWARE_VERSION_MAX-1);
            }
        }
    }

    return true;
}


/*
 * NAME:        traverseJsonTopLevel
 * ACTION:      Parses the JSON tokens by recursive descent
 */

static bool traverseJsonTopLevel(Sinc *sc, const char *jsonStr, jsmntok_t *tokens, int *tokPos, SiToro__Sinc__ListParamDetailsResponse *deviceParams, struct DeviceSettings *settings, char *saveFileFirmwareVersion)
{
    if (tokPos == NULL)
    {
        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__INVALID_REQUEST);
        return false;
    }

    int i;
    jsmntok_t *tok = &tokens[*tokPos];
    (*tokPos)++;

    if (tok->type != JSMN_OBJECT)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "bad project file - expected object at top level");
        return false;
    }

    for (i = 0; i < tok->size; i++)
    {
        if (!traverseJsonInstrumentParam(sc, jsonStr, tokens, tokPos, deviceParams, settings, saveFileFirmwareVersion))
            return false;
    }

    return true;
}


/*
 * NAME:        setDeviceSettings
 * ACTION:      Set all the device settings from the DeviceSettings struct.
 * PARAMETERS:  Sinc *sc                        - the sinc connection.
 *              struct DeviceSettings *settings - the settings to set.
 *              const char *fromFirmwareVersion - the firmware version the parameters are from.
 * RETURNS:     bool - true on success.
 */

static bool setDeviceSettings(Sinc *sc, struct DeviceSettings *settings, const char *fromFirmwareVersion)
{
    int i;

    /* Set all the parameters in one hit. */
    if (!SincSetAllParams(sc, -1, settings->params, settings->numParams, fromFirmwareVersion))
        return false;

    /* Set the calibration for each channel. */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        struct ChannelCalibration *cc = &settings->calib[i];
        if (cc->calibrationData.len != 0 || cc->examplePulse.len > 0 || cc->modelPulse.len > 0 || cc->finalPulse.len > 0)
        {
            if (!SincSetCalibration(sc, i, &cc->calibrationData, &cc->examplePulse, &cc->modelPulse, &cc->finalPulse))
                return false;
        }
    }

    return true;
}


/*
 * NAME:        SincProjectLoad
 * ACTION:      Loads the device settings from a file and restores them to the device.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              const char *fileName - the name of the file to load from.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincProjectLoad(Sinc *sc, const char *fileName)
{
    jsmn_parser parser;
    struct DeviceSettings settings;
    SiToro__Sinc__ListParamDetailsResponse *definedParams = NULL;
    char saveFileFirmwareVersion[FIRMWARE_VERSION_MAX];

    // Read the file.
    char *jsonStr = NULL;
    size_t jsonLen = 0;
    if (!readFileAsString(sc, &jsonStr, fileName))
        return false;

    jsonLen = strlen(jsonStr);

    // How many tokens do we need?
    jsmn_init(&parser);
    int numTokens = jsmn_parse(&parser, jsonStr, jsonLen, NULL, 0);
    if (numTokens < 0)
    {
        jsmnError(sc, numTokens);
        free(jsonStr);
        return false;
    }

    // Parse the json.
    jsmntok_t *tokens = calloc((size_t)numTokens, sizeof(jsmntok_t));
    if (tokens == NULL)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY, "out of memory");
        free(jsonStr);
        return false;
    }

    jsmn_init(&parser);
    int tokensParsed = jsmn_parse(&parser, jsonStr, jsonLen, tokens, (unsigned int)numTokens);
    if (tokensParsed < 0)
    {
        jsmnError(sc, tokensParsed);
        free(jsonStr);
        free(tokens);
        return false;
    }

    // Get the list of parameters on the device.
    if (!SincListParamDetails(sc, 0, NULL, &definedParams))
    {
        free(jsonStr);
        free(tokens);
        return false;
    }

    // Initialise the device settings struct. */
    initDeviceSettings(&settings);

    // Traverse the json.
    int tokPos = 0;
    if (!traverseJsonTopLevel(sc, jsonStr, tokens, &tokPos, definedParams, &settings, saveFileFirmwareVersion))
    {
        free(jsonStr);
        si_toro__sinc__list_param_details_response__free_unpacked(definedParams, NULL);
        closeDeviceSettings(&settings);
        free(tokens);
        return false;
    }

    free(tokens);

    // Set the new settings.
    if (!setDeviceSettings(sc, &settings, saveFileFirmwareVersion))
    {
        free(jsonStr);
        si_toro__sinc__list_param_details_response__free_unpacked(definedParams, NULL);
        closeDeviceSettings(&settings);
        return false;
    }

    /* Clean up. */
    free(jsonStr);
    si_toro__sinc__list_param_details_response__free_unpacked(definedParams, NULL);
    closeDeviceSettings(&settings);

    return true;
}


/*
 * NAME:        saveParamIndent
 * ACTION:      Output some indent to the JSON file.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              int indent - the number of levels of indent to use.
 */

static void saveParamIndent(FILE *jsonFile, int indent)
{
    int i;

    for (i = 0; i < indent; i++)
        fprintf(jsonFile, "  ");
}


/*
 * NAME:        saveParamStr
 * ACTION:      Save an string parameter in JSON form.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              const char *key - the key string.
 *              const char *value - the string value to output.
 *              int indent - the number of levels of indent to use.
 *              bool comma - true to output a comma at the end.
 */

static void saveParamStr(FILE *jsonFile, const char *key, const char *value, int indent, bool comma)
{
    saveParamIndent(jsonFile, indent);
    fprintf(jsonFile, "\"%s\" : \"%s\"%s\n", key, value, comma ? "," : "");
}


/*
 * NAME:        saveParamInt
 * ACTION:      Save an int parameter in JSON form.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              const char *key - the key string.
 *              int value - the int value to output.
 *              int indent - the number of levels of indent to use.
 *              bool comma - true to output a comma at the end.
 */

static void saveParamInt(FILE *jsonFile, const char *key, int value, int indent, bool comma)
{
    saveParamIndent(jsonFile, indent);
    fprintf(jsonFile, "\"%s\" : %d%s\n", key, value, comma ? "," : "");
}


/*
 * NAME:        saveKeyValue
 * ACTION:      Save a key/value pair in JSON form.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              const char *key - the key string.
 *              SincCalibrationPlot *plot - the plot to output.
 *              int indent - the number of levels of indent to use.
 *              bool comma - true to output a comma at the end.
 */

static void saveSincPlot(FILE *jsonFile, const char *key, SincCalibrationPlot *plot, int indent, bool comma)
{
    int i;

    /* Output the key line. */
    saveParamIndent(jsonFile, indent);
    fprintf(jsonFile, "\"%s\" : [\n", key);

    /* Output the values. */
    saveParamIndent(jsonFile, indent+1);
    for (i = 0; i < plot->len; i++)
    {
        double fractPart, intPart;
        fractPart = modf(plot->y[i], &intPart);
        if (fractPart == 0.0)
        {
            fprintf(jsonFile, "%ld", (long)intPart);
        }
        else
        {
            fprintf(jsonFile, "%.*g", LDBL_DIG, plot->y[i]);
        }

        if (i < plot->len - 1)
            fprintf(jsonFile, ",");
    }

    fprintf(jsonFile, "\n");

    /* Add the closing brace. */
    saveParamIndent(jsonFile, indent);
    fprintf(jsonFile, "]");

    if (comma)
        fprintf(jsonFile, ",");

    fprintf(jsonFile, "\n");
}


/*
 * NAME:        saveKeyValue
 * ACTION:      Save a key/value pair in JSON form.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              SiToro__Sinc__KeyValue *kv - the key/value pair to write
 *              int indent - the number of levels of indent to use.
 *              bool comma - true to output a comma at the end.
 */

static void saveKeyValue(FILE *jsonFile, SiToro__Sinc__KeyValue *kv, int indent, bool comma)
{
    char *ch;

    /* Output the key. */
    saveParamIndent(jsonFile, indent);
    fprintf(jsonFile, "\"%s\" : ", kv->key);

    /* Output the value (of any type). */
    switch (kv->paramtype)
    {
    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE:
        fprintf(jsonFile, "%ld", (long)kv->intval);
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE:
    {
        double fractPart, intPart;
        fractPart = modf(kv->floatval, &intPart);
        if (fractPart == 0.0)
        {
            fprintf(jsonFile, "%ld", (long)intPart);
        }
        else
        {
            fprintf(jsonFile, "%.*g", LDBL_DIG, kv->floatval);
        }
        break;
    }
    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE:
        if (kv->boolval)
            fprintf(jsonFile, "true");
        else
            fprintf(jsonFile, "false");
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE:
        fprintf(jsonFile, "\"");

        /* Output the string in escaped form. */
        for (ch = kv->strval; *ch != 0; ch++)
        {
            if (*ch == '"')
            {
                putc('\\', jsonFile);
            }

            putc(*ch, jsonFile);
        }

        fprintf(jsonFile, "\"");
        break;

    case SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE:
        fprintf(jsonFile, "\"%s\"", kv->optionval);
        break;

    default:
        fprintf(jsonFile, "null");
        break;
    }

    /* Output a newline. */
    if (comma)
        fprintf(jsonFile, ",");

    fprintf(jsonFile, "\n");
}


/*
 * NAME:        getDeviceIp
 * ACTION:      Get the IP address of the device we're connected to in ASCII form.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              char *buf - the text buffer to write to.
 *              int bufLen - the size of the text buffer.
 */

static bool getDeviceIp(Sinc *sc, char *buf, int bufLen)
{
#ifndef _WIN32
    socklen_t len;
    struct sockaddr_storage addr;

    len = sizeof(addr);
    if (getpeername(sc->fd, (struct sockaddr*)&addr, &len) < 0)
    {
        SincReadErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't get device address");
        return false;
    }

    // deal with both IPv4 and IPv6:
    if (addr.ss_family == AF_INET)
    {
        /* AF_INET. */
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        inet_ntop(AF_INET, &s->sin_addr, buf, (socklen_t)bufLen);
    }
    else
    {
        /* AF_INET6. */
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        inet_ntop(AF_INET6, &s->sin6_addr, buf, (socklen_t)bufLen);
    }
#else
    strcpy(buf, "");
#endif

    return true;
}


/*
 * NAME:        getNumChannels
 * ACTION:      Get the number of channels from the device.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 * RETURNS:     int - the channel id or -1 on error.
 */

static int getNumChannels(Sinc *sc)
{
    /* Get the number of channels. */
    SiToro__Sinc__GetParamResponse *resp = NULL;
    int channelId;

    if (!SincGetParam(sc, 0, (char *)"instrument.numChannels", &resp, NULL))
        return -1;

    /* Return the result. */
    if (resp->n_results < 1 || !resp->results[0]->has_intval)
    {
        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);
        SincWriteErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED, "invalid response when getting the number of channels from the device");
        return -1;
    }

    channelId = (int)resp->results[0]->intval;
    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    return channelId;
}


/*
 * NAME:        saveChannel
 * ACTION:      Saves the channel level settings.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              SiToro__Sinc__ListParamDetailsResponse *pdResp - the parameter details to save.
 *              SincCalibrationData *calibData  - the calibration data is stored here.
 *              SincPlot *example, model, final - the pulse shapes are set here.
 *              int channelId - the channel id of this channel.
 *              int comma - true to output a comma at the end of the data.
 */

static void saveChannel(FILE *jsonFile, SiToro__Sinc__ListParamDetailsResponse *pdResp, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final, int channelId, bool comma)
{
    /* Output the header. */
    size_t i;
    size_t last = 0;
    size_t calibLoc = 0;

    fprintf(jsonFile, "    {\n");

    /* Save the channel id. */
    saveParamInt(jsonFile, "_channelId", channelId, 3, true);

    /* Find out which parameter will be the last one to be output. */
    for (i = 0; i < pdResp->n_paramdetails; i++)
    {
        SiToro__Sinc__ParamDetails *pd = pdResp->paramdetails[i];
        if (!pd->has_instrumentlevel || !pd->instrumentlevel)
        {
            last = i;

            /* Remember a location to insert the calibration data. */
            if (strcmp(pd->kv->key, "calibration") < 0)
                calibLoc = i;
        }
    }

    /* Output the instrument parameters. */
    for (i = 0; i < pdResp->n_paramdetails; i++)
    {
        SiToro__Sinc__ParamDetails *pd = pdResp->paramdetails[i];
        if ((!pd->has_instrumentlevel || !pd->instrumentlevel) && pd->has_settable && pd->settable)
        {
            /* Output this channel parameter. */
            saveKeyValue(jsonFile, pd->kv, 3, i < last);
        }

        if (calibLoc == i && calibData->data != NULL)
        {
            /* Output the calibration data. */
            char calibDataStr[1024];
            Base64Encode(calibData->data, (size_t)calibData->len, calibDataStr, sizeof(calibDataStr));
            saveParamStr(jsonFile, "calibration.data", calibDataStr, 3, true);
            saveSincPlot(jsonFile, "calibration.exampleShape.y", example, 3, true);
            saveSincPlot(jsonFile, "calibration.modelShape.y", model, 3, true);
            saveSincPlot(jsonFile, "calibration.finalShape.y", final, 3, true);
        }
    }

    /* Output the footer. */
    fprintf(jsonFile, "    }");
    if (comma)
        fprintf(jsonFile, ",");

    fprintf(jsonFile, "\n");
}


/*
 * NAME:        saveInstrument
 * ACTION:      Saves the instrument level settings.
 * PARAMETERS:  FILE *jsonFile - the JSON file to write to.
 *              SiToro__Sinc__ListParamDetailsResponse *pdResp - the parameter details to save.
 */

static bool saveInstrument(FILE *jsonFile, SiToro__Sinc__ListParamDetailsResponse *pdResp)
{
    /* Output the header. */
    size_t i;
    size_t last = 0;

    /* Find out which parameter will be the last one to be output. */
    for (i = 0; i < pdResp->n_paramdetails; i++)
    {
        SiToro__Sinc__ParamDetails *pd = pdResp->paramdetails[i];
        if (pd->has_instrumentlevel && pd->instrumentlevel)
        {
            last = i;
        }
    }

    /* Output the instrument parameters. */
    for (i = 0; i < pdResp->n_paramdetails; i++)
    {
        SiToro__Sinc__ParamDetails *pd = pdResp->paramdetails[i];
        if (pd->has_instrumentlevel && pd->instrumentlevel)
        {
            saveKeyValue(jsonFile, pd->kv, 1, i < last);
        }
    }

    return true;
}


/*
 * NAME:        SincProjectSave
 * ACTION:      Saves the device settings to a file from the device.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              const char *fileName - the name of the file to save to.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincProjectSave(Sinc *sc, const char *fileName)
{
    FILE *jsonFile = NULL;
    SiToro__Sinc__ListParamDetailsResponse *pdResp = NULL;
    SincCalibrationData calibData;
    SincCalibrationPlot example;
    SincCalibrationPlot model;
    SincCalibrationPlot final;
    int numChannels = 0;
    int i;
#ifndef _WIN32
	char deviceAddress[INET6_ADDRSTRLEN];
#else
	char deviceAddress[20];
#endif

    /* Get the device's IP address. */
    if (!getDeviceIp(sc, deviceAddress, sizeof(deviceAddress)))
        return false;

    /* Open the file. */
    jsonFile = fopen(fileName, "wt");
    if (jsonFile == NULL)
    {
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "can't open project file %s for writing - %s", fileName, strerror(errno));
        SincWriteErrorSetMessage(sc, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED, errMsg);

        goto errorExit;
    }

    /* Output the header. */
    fprintf(jsonFile, "{\n");
    saveParamStr(jsonFile, "_fileType", "SiToro Project", 1, true);
    saveParamStr(jsonFile, "address", deviceAddress, 1, true);
    fprintf(jsonFile, "  \"channels\" : [\n");

    /* Get the number of channels. */
    numChannels = getNumChannels(sc);
    if (numChannels < 0)
        goto errorExit;

    /* Go through the channels. */
    for (i = 0; i < numChannels; i++)
    {
        /* Read the device settings for this channel. */
        if (!SincListParamDetails(sc, i, (char *)"", &pdResp))
            goto errorExit;

        calibData.data = NULL;
        calibData.len = 0;
        if (!SincGetCalibration(sc, i, &calibData, &example, &model, &final))
        {
            calibData.data = NULL;
        }

        /* Save this channel's data. */
        saveChannel(jsonFile, pdResp, &calibData, &example, &model, &final, i, i < numChannels-1);

        /* Free the channel data. */
        si_toro__sinc__list_param_details_response__free_unpacked(pdResp, NULL);
        SincSFreeCalibration(&calibData, &example, &model, &final);
        pdResp = NULL;
    }

    fprintf(jsonFile, "  ],\n");

    /* Read the instrument settings. */
    if (!SincListParamDetails(sc, 0, (char *)"", &pdResp))
        goto errorExit;

    /* Output the instrument level data and recurse down to the channels. */
    if (!saveInstrument(jsonFile, pdResp))
        goto errorExit;

    /* Output the footer. */
    fprintf(jsonFile, "}\n");

    /* Close the file. */
    fclose(jsonFile);
    si_toro__sinc__list_param_details_response__free_unpacked(pdResp, NULL);

    return true;

    /* Error cleanup and exit. */
errorExit:
    if (pdResp != NULL)
        si_toro__sinc__list_param_details_response__free_unpacked(pdResp, NULL);

    SincSFreeCalibration(&calibData, &example, &model, &final);

    if (jsonFile != NULL)
        fclose(jsonFile);

    return false;
}
