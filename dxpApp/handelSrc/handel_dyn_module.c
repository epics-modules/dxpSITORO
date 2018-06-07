/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2011 XIA LLC
 * All rights reserved
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the
 *     above copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *   * Neither the name of XIA LLC
 *     nor the names of its contributors may be used to endorse
 *     or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "xia_handel.h"
#include "xia_handel_structures.h"
#include "xia_module.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"

/*
 * Define the head of the Modules LL
 */
static Module *xiaModuleHead = NULL;


static const char *MODULE_NULL_STRING = "null";
#define MODULE_NULL_STRING_LEN  (strlen(MODULE_NULL_STRING) + 1)


/* This structure represents a module_type name and allows for multiple aliases
 * to the same name. This reduces the amount of code that needs to be modified
 * elsewhere in Handel.
 */
typedef struct _ModName {
    const char *alias;
    const char *actual;
} ModName_t;


static ModName_t KNOWN_MODS[] = {
    { "falconx", "falconx" },
    { "falconxn", "falconxn" },
    { "falconx1", "falconxn" },
    { "falconx4", "falconxn" },
    { "falconx8", "falconxn" },
};


HANDEL_STATIC int HANDEL_API xiaProcessInterface(Module *chosen,
                                                 const char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaProcessFirmware(Module *chosen,
                                                const char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaProcessDefault(Module *chosen,
                                               const char *name, void *value);
HANDEL_STATIC int _addAlias(Module *chosen, int idx, void *value);
HANDEL_STATIC int _addDetector(Module *chosen, int idx, void *value);
HANDEL_STATIC int HANDEL_API xiaGetIFaceInfo(Module *chosen,
                                             const char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetChannel(Module *chosen,
                                           const char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetFirmwareInfo(Module *chosen,
                                                const char *name, void *value);
HANDEL_STATIC int HANDEL_API xiaGetDefault(Module *chosen,
                                           const char *name, void *value);
HANDEL_STATIC boolean_t HANDEL_API xiaIsSubInterface(char *name);
HANDEL_STATIC int HANDEL_API xiaGetAlias(Module *chosen, int chan, void *value);
HANDEL_STATIC int HANDEL_API xiaGetDetector(Module *chosen, int chan, void *value);
HANDEL_STATIC int HANDEL_API xiaGetModuleType(Module *chosen, void *value);
HANDEL_STATIC int HANDEL_API xiaGetNumChans(Module *chosen, void *value);
HANDEL_STATIC int HANDEL_API xiaSetDefaults(Module *module);
HANDEL_STATIC Token HANDEL_API xiaGetNameToken(const char *name);
HANDEL_STATIC int HANDEL_API xiaMergeDefaults(const char *output,
                                              const char *input1, const char *input2);

/* This will be the new style for static routines in Handel (10/03) */
HANDEL_STATIC int  _initModule(Module *module, const char *alias);
HANDEL_STATIC int  _initDefaults(Module *module);
HANDEL_STATIC int  _initChannels(Module *module);
HANDEL_STATIC int  _initDetectors(Module *module);
HANDEL_STATIC int  _initDetectorChans(Module *module);
HANDEL_STATIC int  _initFirmware(Module *module);
HANDEL_STATIC int  _initCurrentFirmware(Module *module);
HANDEL_STATIC int  _initMultiState(Module *module);
HANDEL_STATIC int  _initChanAliases(Module *module);
HANDEL_STATIC int  _addModuleType(Module *module, void *type, const char *name);
HANDEL_STATIC int  _addNumChans(Module *module, void *nChans, const char *name);
HANDEL_STATIC int  _addChannel(Module *module, void *val, const char *name);
HANDEL_STATIC int  _addFirmware(Module *module, void *val, const char *name);
HANDEL_STATIC int  _addDefault(Module *module, void *val, const char *name);
HANDEL_STATIC int  _addData(Module *module, void *val, const char *name);
HANDEL_STATIC int  _addInterface(Module *module, void *val, const char *name);
HANDEL_STATIC int  _doAddModuleItem(Module *module, void *data,
                                    unsigned int i, const char *name);
HANDEL_STATIC int  _splitIdxAndType(const char *str, int *idx, char *type);
HANDEL_STATIC int  _parseDetectorIdx(const char *str, int *idx, char *alias);

/* This array should have the string at some index correspond to the Interface
 * constant that that index represents. If a new interface is added a new
 * string should be added and the row size of interfaceStr should increase
 * by one. They should match definition in xia_module.h
 */
static const char *interfaceStr[] = {
    "none",
    "INET",
};

/* This array is mainly used to compare names with the possible sub-interface
 * values. This should be update every time a new interface is added.
 */
static const char *subInterfaceStr[] = {
    "inet_address",
    "inet_port",
    "inet_timeout",
};


static ModItem_t items[] = {
    {"module_type",        _addModuleType, FALSE_},
    {"number_of_channels", _addNumChans,   TRUE_},
    {"channel",            _addChannel,    TRUE_},
    {"firmware",           _addFirmware,   TRUE_},
    {"default",            _addDefault,    TRUE_},
    {"data",               _addData,       TRUE_},
    {"interface",          _addInterface,  TRUE_},
    {"inet_address",       _addInterface,  TRUE_},
    {"inet_port",          _addInterface,  TRUE_},
    {"inet_timeout",       _addInterface,  TRUE_},
};

#define NUM_ITEMS (sizeof(items) / sizeof(items[0]))


static ModInitFunc_t inits[] = {
    _initChannels,
    _initDefaults,
    _initDetectors,
    _initDetectorChans,
    _initFirmware,
    _initCurrentFirmware,
    _initMultiState,
    _initChanAliases,

};

#define NUM_INITS (sizeof(inits) / sizeof(inits[0]))


static AddChanType_t chanTypes[] = {
    { "alias",    _addAlias },
    { "detector", _addDetector },

};

#define NUM_CHAN_TYPES (sizeof(chanTypes) / sizeof(chanTypes[0]))


/*****************************************************************************
 *
 * This routine creates a new Module entry w/ the unique name alias
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaNewModule(const char *alias)
{
    int status = XIA_SUCCESS;

    Module *current = NULL;


    /* If HanDeL isn't initialized, go ahead and call it... */
    if (!isHandelInit)
    {
        status = xiaInitHandel();
        if (status != XIA_SUCCESS)
        {
            fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
            exit(XIA_INITIALIZE);
        }

        xiaLog(XIA_LOG_WARNING, "xiaNewModule",
               "HanDeL initialized silently");
    }

    if ((strlen(alias) + 1) > MAXALIAS_LEN)
    {
        status = XIA_ALIAS_SIZE;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewModule",
               "Alias contains too many characters");
        return status;
    }

    /* Does the module alias already exist? */
    current = xiaFindModule(alias);
    if (current != NULL)
    {
        status = XIA_ALIAS_EXISTS;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewModule",
               "Alias %s already in use", alias);
        return status;
    }

    /* Initialize linked-list or add to it */
    current = xiaGetModuleHead();
    if (current == NULL) {

        xiaModuleHead = (Module *)handel_md_alloc(sizeof(Module));
        current = xiaModuleHead;

    } else {

        while (current->next != NULL) {

            current = current->next;
        }

        current->next = (Module *)handel_md_alloc(sizeof(Module));
        current = current->next;
    }

    /* Did we actually allocate the memory? */
    if (current == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewModule",
               "Unable to allocate memory for Module alias %s", alias);
        return status;
    }

    status = _initModule(current, alias);

    if (status != XIA_SUCCESS) {
        /* XXX: Need to do some cleanup here! */

        xiaLog(XIA_LOG_ERROR, status, "xiaNewModule",
               "Error initializing new module");
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adds a module item.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaAddModuleItem(const char *alias,
                                              const char *name, void *value)
{
    int status;

    unsigned int i;
    unsigned int nItems = (unsigned int)NUM_ITEMS;

    Module *m = NULL;


    /* Verify the arguments */
    if (alias == NULL) {
        status = XIA_NULL_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddModuleItem",
               "NULL 'alias' passed into function");
        return status;
    }

    if (name == NULL) {
        status = XIA_NULL_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddModuleItem",
               "NULL 'name' passed into function");
        return status;
    }

    if (value == NULL) {
        status = XIA_NULL_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddModuleItem",
               "NULL 'value' passed into function");
        return status;
    }

    m = xiaFindModule(alias);

    if (m == NULL) {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddModuleItem",
               "Alias '%s' does not exist in Handel", alias);
        return status;
    }

    for (i = 0; i < nItems; i++) {
        if (STRNEQ(name, items[i].name)) {
            status = _doAddModuleItem(m, value, i, name);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaAddModuleItem",
                       "Error adding item '%s' to module '%s'", name, m->alias);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    xiaLog(XIA_LOG_ERROR, XIA_UNKNOWN_ITEM, "xiaAddModuleItem",
           "Unknown item '%s' for module '%s'", name, m->alias);

    return XIA_UNKNOWN_ITEM;
}


/*****************************************************************************
 *
 * This routine sets up the modules.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaSetupModules(void)
{
    int status;

    Module *module = xiaGetModuleHead();

    if (module == NULL) {
        status = XIA_NO_MODULE;
        xiaLog(XIA_LOG_ERROR, status, "xiaSetupModules",
               "No modules");
        return status;
    }

    while (module != NULL) {
        /* Expect psl funcs are set up in _addModuleType. */
        ASSERT(module->psl);

        status = module->psl->setupModule(module);

        if (status != XIA_SUCCESS) {
            module->psl = NULL;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetupModules",
                   "Unable to setup module %s.", module->alias);
            return status;
        }

        module = getListNext(module);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine ends the modules.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaEndModules(void)
{
    Module *module = xiaGetModuleHead();

    while (module != NULL) {
        if (module->psl) {
            module->psl->endModule(module);
            module->psl = NULL;
        }
        module = getListNext(module);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine either returns a pointer to a valid Module entry or NULL (if
 * a valid entry is not found).
 *
 *****************************************************************************/
HANDEL_SHARED Module* HANDEL_API xiaFindModule(const char *alias)
{
    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    Module *current = NULL;

    ASSERT(alias != NULL);
    ASSERT(strlen(alias) < (MAXALIAS_LEN - 1));

    /* Convert the name to lowercase */
    for (i = 0; i < (unsigned int)strlen(alias); i++)
    {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* Walk through the linked-list until we find it or we run out of elements */
    current = xiaGetModuleHead();

    while (current != NULL)
    {
        if (STREQ(strtemp, current->alias))
        {
            return current;
        }

        current = current->next;
    }

    return NULL;
}


/*****************************************************************************
 *
 * This routine handles the parsing of the sub-interface elements and also
 * checks to see if an interface has been defined. If an interface doesn't
 * exist, then the routine creates a new interface before adding data to it.
 * If the specified data is for the wrong interface then the routine returns
 * an error.
 *
 * This routine assumes that a valid name is being passed to it. It doesn't
 * error check the name. It just ignores it if invalid.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessInterface(Module *chosen,
                                                 const char *name, void *value)
{
    int status;

    const char *interface_ = NULL;

    ASSERT(chosen != NULL);
    ASSERT(name != NULL);
    ASSERT(value != NULL);

    /* This is a hack to acknowledge the fact that xiaProcessInterface() now
     * deals with more then just the sub-interface elements.
     */
    if (STRNEQ(name, "interface")) {
        interface_ = (char *)value;

    } else {
        interface_ = "";
    }

    if (STREQ(name, "inet_address") ||
        STREQ(name, "inet_port")    ||
        STREQ(name, "inet_timeout") ||
        STREQ(interface_, "inet")) {
        /* Check that this module is really a INET */
        if ((chosen->interface_->type != INET)  &&
            (chosen->interface_->type != NO_INTERFACE)) {
            status = XIA_WRONG_INTERFACE;
            xiaLog(XIA_LOG_ERROR, status, "xiaProcessInterface",
                   "Item %s is not a valid element of the current interface", name);
            return status;
        }

        /* See if we need to create a new interface */
        if (chosen->interface_->type == NO_INTERFACE) {
            chosen->interface_->type = INET;
            chosen->interface_->info.inet =
                (Interface_Inet *)handel_md_alloc(sizeof(Interface_Inet));
            if (chosen->interface_->info.inet == NULL) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaProcessInterface",
                            "Unable to allocate memory for INET");
                return status;
            }

            chosen->interface_->info.inet->address = NULL;
            chosen->interface_->info.inet->port    = 0;
            chosen->interface_->info.inet->timeout = 0;
        }

        if (STREQ(name, "inet_address")) {
            chosen->interface_->info.inet->address =
                (char *) handel_md_alloc(strlen(value) + 1);
            if (chosen->interface_->info.inet->address == NULL) {
                handel_md_free(chosen->interface_->info.inet);
                chosen->interface_->info.inet = NULL;
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaProcessInterface",
                       "Unable to allocate memory for INET address");
                return status;
            }
            strcpy(chosen->interface_->info.inet->address, (char*) value);
        }
        else if (STREQ(name, "inet_port")) {
            chosen->interface_->info.inet->port = *((unsigned int*) value);
        }
        else if (STREQ(name, "inet_timeout")) {
            chosen->interface_->info.inet->timeout = *((unsigned int*) value);
        }
    }
    else {
        status = XIA_MISSING_INTERFACE;
        xiaLog(XIA_LOG_ERROR, status, "xiaProcessInterface",
               "'%s' is a member of an unknown interface", name);
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adds an alias value to the Module information. It is
 * responsible for allocating memory if the channels[] is NULL. This routine
 * assumes that the data passed to it is valid and consistient with the rest
 * of HanDeL's universe.
 *
 *****************************************************************************/
HANDEL_STATIC int _addAlias(Module *chosen, int idx, void *value)
{
    int status;
    int detChan;

    if (chosen->channels == NULL)
    {
        /* I should make a point here that the -1 I use in memset is actually cast into
         * an unsigned int/char by memset, so hopefully sign-extension will win...
         */
        chosen->channels =
            (int *)handel_md_alloc(chosen->number_of_channels * sizeof(int));
        if (chosen->channels == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "_addAlias",
                   "Unable to allocate memory for channels (%d)",
                   chosen->number_of_channels);
            return status;
        }

        memset(chosen->channels, -1, chosen->number_of_channels);
    }

    detChan = *((int *)value);

    if (detChan == -1)
    {
        /* This isn't really a do anything...I just don't want it to do anything
         * but modify the Mod struct info, which happens later. This is more of
         * a skip the next part thing...
         */
    } else {
        /* This handles the case where this routine has been called by
         * xiaModifyModuleItem(). This is a little hack, but I don't think
         * it's too deplorable. Let's just call it 'defensive programming'.
         */
        if (chosen->channels[idx] > -1)
        {
            status = xiaRemoveDetChan(chosen->channels[idx]);
            chosen->channels[idx] = -1;
            /* The status will be checked a little farther down. (After the add
             * routine is called.
             */
        }

        if (!xiaIsDetChanFree(detChan))
        {
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "_addAlias",
                   "detChan %d is invalid", *((int *)value));
            return status;
        }

        status = xiaAddDetChan(SINGLE, detChan, (void *)chosen->alias);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "_addAlias",
                   "Error adding detChan %d", detChan);
            return status;
        }
    }

    chosen->channels[idx] = detChan;

    return XIA_SUCCESS;
}

/**
 * This routine associates a detector alias and detector channel to a
 * logical module channel. @a value is a string detector channel alias
 * {detector alias}:{n}.
 *
 * A check is made to verify that the alias actually exists.
 *
 * This routine assumes that the data passed to it is valid and
 * consistent with the rest of Handel's universe. It allocates memory
 * for the module's detector aliases array if needed.
 */
HANDEL_STATIC int _addDetector(Module *chosen, int idx, void *value)
{
    int status;
    int i;
    int didx;

    char alias[MAXALIAS_LEN];


    Detector *detector = NULL;


    status = _parseDetectorIdx((char *)value, &didx, alias);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addDetector",
               "Error parsing '%s'", (char *)value);
        return status;
    }

    ASSERT(didx >= 0);

    detector = xiaFindDetector(alias);

    if (detector == NULL) {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "_addDetector",
               "Detector alias: '%s' does not exist", alias);
        return status;
    }

    if (chosen->detector == NULL) {
        chosen->detector =
            (char **)handel_md_alloc(chosen->number_of_channels * sizeof(char *));
        if (chosen->detector == NULL) {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "_addDetector",
                   "Unable to allocate memory for chosen->detector");
            return status;
        }

        for (i = 0; i < (int)chosen->number_of_channels; i++) {
            chosen->detector[i] = (char *)handel_md_alloc(MAXALIAS_LEN * sizeof(char));
            if (chosen->detector[i] == NULL) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addDetector",
                       "Unable to allocate memory for chosen->detector[i]");
                return status;
            }
        }
    }

    strcpy(chosen->detector[idx], alias);

    /* Check that didx is valid... */
    if (didx >= (int)detector->nchan) {
        status = XIA_BAD_CHANNEL;
        xiaLog(XIA_LOG_ERROR, status, "_addDetector",
               "Specified physical detector channel is invalid");
        return status;
    }

    if (chosen->detector_chan == NULL) {
        chosen->detector_chan =
            (int *)handel_md_alloc(chosen->number_of_channels * sizeof(int));
        if (chosen->detector_chan == NULL) {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "_addDetector",
                   "Unable to allocate memory for chosen->detector_chan");
            return status;
        }
    }

    chosen->detector_chan[idx] = didx;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses the complete string to determine which actions need
 * to be taken w/r/t the firmware information for the module. The name string
 * passed into this routine is certain to at least contain "firmware" as the
 * first 8 characters and the routine operates on that assumption.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessFirmware(Module *chosen,
                                                const char *name, void *value)
{
    int status;
    int i;
    int j;
    int idx;

    char *sidx;
    char strtemp[5];

    FirmwareSet *firmware = NULL;

    firmware = xiaFindFirmware((char *)value);
    if (firmware == NULL) {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaProcessFirmware",
               "Firmware alias %s is invalid", (char *)value);
        return status;
    }

    /* One _possbile_ problem is that memory will be allocated even if the name
     * turns out to be bad. Now, technically, this isn't really that bad since
     * the memory needs to be allocated at some point in time...
     */
    if (chosen->firmware == NULL)
    {
        chosen->firmware =
            (char **)handel_md_alloc(chosen->number_of_channels * sizeof(char *));
        if (chosen->firmware == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaProcessFirmware",
                   "Unable to allocate memory for chosen->firmware");
            return status;
        }

        for (i = 0; i < (int)chosen->number_of_channels; i++)
        {
            chosen->firmware[i] = (char *)handel_md_alloc(MAXALIAS_LEN * sizeof(char));
            if (chosen->firmware[i] == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaProcessFirmware",
                       "Unable to allocate memory for chosen->firmware[%d]", i);
                return status;
            }
        }
    }


    /* Determine if the name string is "firmware_set_all" or "firmware_set_chan{n}" */
    sidx = strrchr(name, '_');

    if (STREQ(sidx + 1, "all"))
    {
        for (j = 0; j < (int)chosen->number_of_channels; j++)
        {
            strcpy(chosen->firmware[j], (char *)value);
        }

        return XIA_SUCCESS;
    }

    /* At this point we have a "chan{n}" name or a bad name. This is done as brute
     * force since I can't think of a clever solution right now. Any takers?
     */
    strncpy(strtemp, sidx + 1, 4);
    strtemp[4] = '\0';

    if (STREQ(strtemp, "chan"))
    {
        idx = atoi(sidx + 5);

        if ((idx >= (int)chosen->number_of_channels) ||
            (idx < 0))
        {
            status = XIA_BAD_CHANNEL;
            xiaLog(XIA_LOG_ERROR, status, "xiaProcessFirmware",
                   "Specified channel is invalid");
            return status;
        }

        strcpy(chosen->firmware[idx], (char *)value);

    } else {

        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaProcessFirmware",
               "Invalid name: %s", name);
        return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine parses the complete string to determine which actions need
 * to be taken w/r/t the default information for the module. The name string
 * passed into this routine is certain to at least contain "default" as the
 * first 7 characters and the routine operates on that assumption.
 *
*****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaProcessDefault(Module *chosen,
                                               const char *name, void *value)
{
    int status;
    int j;
    int idx;

    char *sidx;
    char strtemp[5];

    XiaDefaults *defaults = NULL;

    xiaLog(XIA_LOG_DEBUG, "xiaProcessDefault",
           "Preparing to find default %s", (char *)value);

    defaults = xiaFindDefault((char *)value);
    if (defaults == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaProcessDefault",
               "Defaults alias %s is invalid", (char *)value);
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaProcessDefault",
           "name = %s", name);

    /* Determine if the name string is "defaults_all" or "defaults_chan{n}" */
    sidx = strrchr(name, '_');

    if (STREQ(sidx + 1, "all"))
    {
        for (j = 0; j < (int)chosen->number_of_channels; j++)
        {
            status = xiaMergeDefaults(chosen->defaults[j],
                                      chosen->defaults[j], (char *) value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaProcessDefault",
                       "Error merging default %s into default %s",
                       (char *) value, chosen->defaults[j]);
                return status;
            }
        }

        return XIA_SUCCESS;
    }

    /* At this point we have a "chan{n}" name or a bad name. This is done as brute
     * force since I can't think of a clever solution right now. Any takers?
     */
    strncpy(strtemp, sidx + 1, 4);
    strtemp[4] = '\0';

    if (STREQ(strtemp, "chan"))
    {
        idx = atoi(sidx + 5);

        xiaLog(XIA_LOG_DEBUG, "xiaProcessDefault",
               "idx = %d", idx);

        if ((idx >= (int)chosen->number_of_channels) ||
            (idx < 0))
        {
            status = XIA_BAD_CHANNEL;
            xiaLog(XIA_LOG_ERROR, status, "xiaProcessDefault",
                   "Specified channel is invalid");
            return status;
        }

        xiaLog(XIA_LOG_DEBUG, "xiaProcessDefault",
               "name = %s, new value = %s, old value = %s",
               name, (char *) value, chosen->defaults[idx]);

        status = xiaMergeDefaults(chosen->defaults[idx],
                                  chosen->defaults[idx], (char *) value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaProcessDefault",
                   "Error merging default %s into default %s",
                   (char *) value, chosen->defaults[idx]);
            return status;
        }

    } else {

        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaProcessDefault",
               "Invalid name: %s", name);
        return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine merges 2 default lists into an output list.  The output list
 * can be the same as the input.  List 2 is added to list 1, overriding any
 * common values with the entries in list 2.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaMergeDefaults(const char *output, const char *input1,
                                              const char *input2)
{
    int status;

    XiaDefaults *inputDefaults1 = NULL;
    XiaDefaults *inputDefaults2 = NULL;

    XiaDaqEntry *current = NULL;

    /* Get all the default pointers */
    inputDefaults1 = xiaFindDefault(input1);
    inputDefaults2 = xiaFindDefault(input2);

    /* copy input1 into the output, iff different */
    if (!STREQ(output, input1)) {
        current = inputDefaults1->entry;
        while (current != NULL) {
            status = xiaAddDefaultItem(output, current->name,
                                       (void *) &(current->data));

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaMergeDefaults",
                       "Error adding default %s (value = %.3f) to alias %s",
                       current->name, current->data, output);
                return status;
            }

            current = current->next;
        }
    }

    /* Now overwrite with all the values in input2 */
    current = inputDefaults2->entry;
    while (current != NULL) {
        status = xiaAddDefaultItem(output, current->name,
                                   (void *) &(current->data));

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaMergeDefaults",
                   "Error adding default %s (value = %.3f) to alias %s",
                   current->name, current->data, output);
            return status;
        }

        current = current->next;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies information about a module pointed to by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaModifyModuleItem(const char *alias,
                                                 const char *name, void *value)
{
    int status;

    /* Basically a wrapper that filters out module items that we would rather
     * the user not modify once the module has been defined.
     */
    if (STREQ(name, "module_type") ||
        STREQ(name, "number_of_channels"))
    {
        status = XIA_NO_MODIFY;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyModuleItem",
               "Name: %s can not be modified", name);
        return status;
    }

    status = xiaAddModuleItem(alias, name, value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyModuleItem",
               "Error modifying module item: %s", name);
        return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine retrieves the value specified by name from the Module
 * entry specified by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetModuleItem(const char *alias,
                                              const char *name, void *value)
{
    int status;

    Token nameTok = BAD_TOK;

    Module *chosen = NULL;


    chosen = xiaFindModule(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetModuleItem",
               "Alias %s has not been created", alias);
        return status;
    }

    nameTok = xiaGetNameToken(name);

    switch(nameTok)
    {
        default:
        case BAD_TOK:
            status = XIA_BAD_NAME;
            break;

        case MODTYP_TOK:
            status = xiaGetModuleType(chosen, value);
            break;

        case NUMCHAN_TOK:
            status = xiaGetNumChans(chosen, value);
            break;

        case INTERFACE_TOK:
            status = xiaGetIFaceInfo(chosen, name, value);
            break;

        case CHANNEL_TOK:
            status = xiaGetChannel(chosen, name, value);
            break;

        case FIRMWARE_TOK:
            status = xiaGetFirmwareInfo(chosen, name, value);
            break;

        case DEFAULT_TOK:
            status = xiaGetDefault(chosen, name, value);
            break;
    }

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetModuleItem",
               "Unable to get value of %s", name);
        return status;
    }

    return status;
}


/**********
 * This routine returns the # of modules currently defined
 * in the system. Uses brute force for now until we
 * develop the appropriate manager for the LL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumModules(unsigned int *numModules)
{
    unsigned int count = 0;

    Module *current = xiaGetModuleHead();


    while (current != NULL) {

        count++;
        current = getListNext(current);
    }

    *numModules = count;

    return XIA_SUCCESS;
}


/**********
 * This routine returns the aliases of all of the modules
 * currently defined in the system. Assumes that the
 * calling routine has allocated the proper amount of
 * memory in "modules".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetModules(char *modules[])
{
    int i;

    Module *current = xiaGetModuleHead();


    for (i = 0; current != NULL; current = getListNext(current), i++) {

        strcpy(modules[i], current->alias);
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is similar to xiaGetModules(), except that
 * it only returns a single alias since VB can't pass a
 * string array into a DLL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetModules_VB(unsigned int index, char *alias)
{
    int status;

    unsigned int curIdx = 0;

    Module *current = xiaGetModuleHead();


    for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

        if (curIdx == index) {

            strcpy(alias, current->alias);

            return XIA_SUCCESS;
        }
    }

    status = XIA_BAD_INDEX;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetModules_VB",
           "Index = %u is out of range for the modules list", index);
    return status;
}


/*****************************************************************************
 *
 * This routine returns a Token corresponding to the appropriate Module item
 * specified by the name. If the name doesn't match any of the known Module
 * items, then BAD_TOK is returned. The token types are enumerated in
 * xia_module.h.
 *
 *****************************************************************************/
HANDEL_STATIC Token HANDEL_API xiaGetNameToken(const char *name)
{
    size_t len;

    char *tmpName;
    char *sidx;

    Token token = BAD_TOK;

    len = strlen(name);
    tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
    strcpy(tmpName, name);

    /* Do the simple tests first and then the tougher ones */
    if (STREQ(tmpName, "module_type"))
    {
        token = MODTYP_TOK;

    } else if (STREQ(tmpName, "interface") ||
               xiaIsSubInterface(tmpName)) {

        token = INTERFACE_TOK;

    } else if (STREQ(tmpName, "number_of_channels")) {

        token = NUMCHAN_TOK;

    } else {

        sidx = strrchr(tmpName, '_');

        if (sidx != NULL)
        {
            sidx[0] = '\0';
        }

        if (STREQ(tmpName, "firmware_set"))
        {
            token = FIRMWARE_TOK;

        } else if (STREQ(tmpName, "default")) {

            token = DEFAULT_TOK;

        } else if (strncmp(tmpName, "channel", 7) == 0) {

            token = CHANNEL_TOK;
        }
    }

    handel_md_free((void *)tmpName);
    return token;
}

/*****************************************************************************
 *
 * This routine returns a boolean_t indicating if the passed in name matches
 * any of the known subInterface elements defined in subInterfaceStr[].
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsSubInterface(char *name)
{
    int i;

    int len = N_ELEMS(subInterfaceStr);


    ASSERT(name);


    for (i = 0; i < len; i++)
    {
        if (STREQ(name, subInterfaceStr[i]))
        {
            return TRUE_;
        }
    }

    return FALSE_;
}

/*****************************************************************************
 *
 * This routine takes a valid interface/sub-interface name and sets value
 * equal to the value as defined in the module pointed to by chosen. It
 * returns an error if the name corresponds to a sub-interface element that
 * while being valid overall is invalid within the _currently_ defined
 * interface.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaGetIFaceInfo(Module *chosen, const char *name, void *value)
{
    int status = XIA_SUCCESS;

    if (STREQ(name, "interface")) {
        strcpy((char *)value, interfaceStr[chosen->interface_->type]);
    } else {
        if (chosen->interface_->type == INET) {
            if (STREQ(name, "inet_address")) {
                strcpy((char*) value, chosen->interface_->info.inet->address);
            } else if (STREQ(name, "inet_port")) {
                *((unsigned int *)value) = chosen->interface_->info.inet->port;
            } else if (STREQ(name, "inet_timeout")) {
                *((unsigned int *)value) = chosen->interface_->info.inet->timeout;
            } else {
                status = XIA_BAD_NAME;
                xiaLog(XIA_LOG_ERROR, status, "xiaGetIFaceInfo",
                       "Invalid INET parameter: %s", name);
            }
        } else {
            status = XIA_NO_INTERFACE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetIFaceInfo", "No interface");
        }
    }

    return status;
}


/*****************************************************************************
 *
 * This routine retrieves information about the channel (detector, alias or
 * gain) and sets value equal to it. Assumes that the first 7 characters
 * match "channel" and that 1 underscore is present in name. This routine
 * does more parsing on the name.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetChannel(Module *chosen,
                                           const char *name, void *value)
{
    int status;
    size_t len;
    int chan;

    char *sidx;
    char *tmpName;

    len = strlen(name);
    tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
    strcpy(tmpName, name);

    sidx = strchr(tmpName, '_');
    sidx[0] = '\0';

    chan = atoi(tmpName + 7);

    /* Are we getting an alias, detector or gain value? */
    if (STREQ(sidx + 1, "alias"))
    {
        status = xiaGetAlias(chosen, chan, value);

    } else if (STREQ(sidx + 1, "detector")) {

        status = xiaGetDetector(chosen, chan, value);

    } else {

        status = XIA_BAD_NAME;
    }

    /* We're done with tmpName at this point and we are going to use some
     * conditionals later, so let's free the memory at this point and save
     * ourselves the effort of having to repeat the free.
     */
    handel_md_free((void *)tmpName);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetChannel",
               "Error getting module information");
        return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the alias for channel chan for the
 * specified module. This routine verifies that chan is within the
 * proper range.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetAlias(Module *chosen, int chan, void *value)
{
    int status;

    if ((unsigned int)chan >= chosen->number_of_channels)
    {
        status = XIA_BAD_CHANNEL;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetAlias",
               "Specified channel is out-of-range");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaGetAlias",
           "chosen = %p, chan = %d, alias = %d",
           (void *)chosen, chan, chosen->channels[chan]);

    *((int *)value) = chosen->channels[chan];

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the detector alias specified for chan.
 * This routine verfies that chan is within the proper range.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetDetector(Module *chosen, int chan, void *value)
{
    int status;

    /* It's MAXALIAS_LEN + 5 since I want to build a string that has the
     * detector alias name (which can be up to MAXALIAS_LEN chars) concatenated
     * with a colon (+1), up to 3 digits (+3) and termination (+1).
     */
    char strTemp[MAXALIAS_LEN + 5];

    if ((unsigned int)chan >= chosen->number_of_channels)
    {
        status = XIA_BAD_CHANNEL;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDetector",
               "Specified channel is out-of-range.");
        return status;
    }

    sprintf(strTemp, "%s:%d%c", chosen->detector[chan],
            chosen->detector_chan[chan], '\0');

    strcpy((char *)value, strTemp);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves information about the firmware for the specified
 * module. It assumes that name is, at the very least, "firmware_set". An
 * error is reported if it equals "firmware_set_all" since that is not a
 * valid choice.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetFirmwareInfo(Module *chosen,
                                                const char *name, void *value)
{
    int status;
    size_t len;
    int chan;

    char *tmpName;
    char *sidx;

    if (STREQ(name, "firmware_set_all"))
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareInfo",
               "Must specify channel to retrieve firmware info. from");
        return status;
    }

    len = strlen(name);
    tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
    strcpy(tmpName, name);

    sidx = strrchr(tmpName, '_');

    if (strncmp(sidx + 1, "chan", 4) != 0)
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareInfo",
               "Invalid name");

        handel_md_free((void *)tmpName);

        return status;
    }

    chan = atoi(sidx + 1 + 4);

    handel_md_free((void *)tmpName);

    if ((unsigned int)chan >= chosen->number_of_channels)
    {
        status = XIA_BAD_CHANNEL;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareInfo",
               "Specified channel is out-of-range");

        return status;
    }

    strcpy((char *)value, chosen->firmware[chan]);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves information about the default for the specified
 * module. It assumes that name is, at the very least, "default". An
 * error is reported if it equals "default_all" since that is not a
 * valid choice.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetDefault(Module *chosen,
                                           const char *name, void *value)
{
    int status;
    size_t len;
    int chan;

    char *tmpName;
    char *sidx;

    if (STREQ(name, "default_all"))
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDefault",
               "Must specify channel to retrieve default info. from");
        return status;
    }

    len = strlen(name);
    tmpName = (char *)handel_md_alloc((len + 1) * sizeof(char));
    strcpy(tmpName, name);

    sidx = strrchr(tmpName, '_');

    if (strncmp(sidx + 1, "chan", 4) != 0)
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDefault",
               "Invalid name");

        handel_md_free((void *)tmpName);

        return status;
    }

    sscanf(sidx + 5, "%d", &chan);

    if ((unsigned int)chan >= chosen->number_of_channels)
    {
        status = XIA_BAD_CHANNEL;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDefault",
               "Specified channel is out-of-range");

        handel_md_free((void *)tmpName);

        return status;
    }

    strcpy((char *)value, chosen->defaults[chan]);

    handel_md_free((void *)tmpName);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the module type for the specified module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetModuleType(Module *chosen, void *value)
{
    strcpy((char *)value, chosen->type);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine sets value equal to the number of channels for the specified
 * module.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetNumChans(Module *chosen, void *value)
{
    *((unsigned int *)value) = chosen->number_of_channels;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine removes a Module entry specifed by alias.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveModule(const char *alias)
{
    int status;

    unsigned int i;

    Module *prev    = NULL;
    Module *next    = NULL;
    Module *current = NULL;

    xiaLog(XIA_LOG_INFO, "xiaRemoveModule",
           "Removing %s", alias);

    current = xiaGetModuleHead();

    if (current != NULL)
        next = current->next;

    /* Iterate until we do (or don't) find the Module we are looking for */
    while (current && !STREQ(alias, current->alias))
    {
        prev = current;
        current = next;
        next = current->next;
    }

    if (current == NULL &&
        next    == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
               "Alias %s does not exist", alias);
        return status;
    }

    if (current->detector != NULL)
    {
        for (i = 0; i < current->number_of_channels; i++)
        {
            /* Clean up each channel, for products that use ch.pslData. */
            if (current->ch[i].pslData && current->psl) {
                status = current->psl->endDetChan(current->channels[i], NULL, current);
                if (status != XIA_SUCCESS) {
                    xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                           "Error ending channel %s:%d",
                           current->alias, i);
                }
            }

            /* Clean up the detector, for products that use
             * detector->pslData. This should be reworked to only
             * remove detectors that are not referenced by any module.
             */
            status = xiaRemoveDetector(current->detector[i]);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                       "Error removing detector %s",
                       current->detector[i]);

                /* We continue since we'll leak memory if we return. */
            }

        }

        for (i = 0; i < current->number_of_channels; i++)
            handel_md_free((void *)current->detector[i]);
    }

    handel_md_free((void *)current->detector);
    handel_md_free((void *)current->detector_chan);

    if (current->channels != NULL) {
        for (i = 0; i < current->number_of_channels; i++)
        {
            if (current->channels[i] != -1)
            {
                status = xiaRemoveDetChan(current->channels[i]);

                if (status != XIA_SUCCESS) {

                    xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                           "Error removing detChan member %d",
                           current->channels[i]);

                    /* We continue since we'll leak memory if we return. */
                }
            }
        }
    }

    handel_md_free(current->channels);
    current->channels = NULL;

    if (current->firmware != NULL)
    {
        for (i = 0; i < current->number_of_channels; i++)
        {
            status = xiaRemoveFirmware(current->firmware[i]);

            if (status != XIA_SUCCESS) {

                xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                       "Error removing firmware for modChan %u", i);

                /* We continue since we'll leak memory if we return. */
            }

            handel_md_free((void *)current->firmware[i]);
        }
    }

    handel_md_free((void *)current->firmware);

    if (current->defaults != NULL)
    {
        for (i = 0; i < current->number_of_channels; i++)
        {
            status = xiaRemoveDefault(current->defaults[i]);

            if (status != XIA_SUCCESS) {

                xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                       "Error removing values associated with modChan %u", i);

                /* We continue since we'll leak memory if we return. */
            }

            handel_md_free((void *)current->defaults[i]);
        }
    }

    handel_md_free((void *)current->defaults);
    handel_md_free((void *)current->currentFirmware);

    /* Free up any multichannel info that was allocated */
    if (current->isMultiChannel) {
        handel_md_free(current->state->runActive);
        handel_md_free(current->state);
    }

    /* If the type isn't set, then there is no
     * chance that any of the type-specific data is set
     * like the SCA data.
     */
    if (current->type != NULL) {
        if (current->ch != NULL) {
            if (current->psl) {
                for (i = 0; i < current->number_of_channels; i++) {
                    status = current->psl->freeSCAs(current, (int) i);

                    if (status != XIA_SUCCESS) {

                        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveModule",
                               "Error removing SCAs from modChan '%u', alias '%s'",
                               i, current->alias);

                        /* We continue since we'll leak memory if we return. */
                    }
                }
            }

            for (i = 0; i < current->number_of_channels; i++) {
                if (current->ch[i].data.data != NULL) {
                    handel_md_free(current->ch[i].data.data);
                }
            }

            handel_md_free(current->ch);
        }

        handel_md_free(current->type);
    }

    /*
     * Clean the interface.
     */
    if (current->interface_ != NULL) {
        if (current->interface_->type == INET) {
            if (current->interface_->info.inet->address != NULL) {
                handel_md_free((void*) current->interface_->info.inet->address);
            }
            handel_md_free(current->interface_->info.inet);
        }
        handel_md_free(current->interface_);
    }

    if (current->psl)
        current->psl->endModule(current);

    current->type = NULL;
    current->psl = NULL;

    if (current == xiaGetModuleHead())
    {
        xiaModuleHead = current->next;

    } else {

        prev->next = current->next;
    }

    handel_md_free(current->alias);
    current->alias = NULL;

    /* This (freeing the module) was previously absent, which I think was causing a
     * small (16-bit) memory leak.
     */
    handel_md_free(current);

    /* XXX If this is the last module, i.e. the # of detChans besides "-1" and
     * any "SET"s is 0, then release the rest of the detChan list.
     */

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine removes all Modules
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveAllModules(void)
{
    Module* current = xiaGetModuleHead();

    while (current != NULL)
    {
        xiaRemoveModule(current->alias);

        current = xiaGetModuleHead();
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine returns the module channel associated with the specified
 * detChan. Remember that the module channel value is relative to the module!
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetModChan(int detChan)
{
    int status = 0;
    int modChan = 0;
    char *modAlias = NULL;
    Module *module = NULL;

    modAlias = xiaGetAliasFromDetChan(detChan);

    module = xiaFindModule(modAlias);

    status = xiaGetAbsoluteChannel(detChan, module, &modChan);

    if (status != XIA_SUCCESS) {
        /* We really shouldn't get here...Need a better way to deal with this.
         * Maybe use signed int so that we can set this to -1 if there is a
         * problem.
         */
        modChan = 999;
    }

    return modChan;
}


/*****************************************************************************
 *
 * This routine returns the module logical detector channel associated with the
 * specified detChan.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetModDetectorChan(int detChan)
{
    int detectorChannel = 999;
    Module* module = NULL;
    int status;

    status = xiaFindModuleAndDetector(detChan, &module, NULL);

    if (status == XIA_SUCCESS) {
        int modChan = xiaGetModChan(detChan);
        if (modChan != 999)
            detectorChannel = module->detector_chan[modChan];
    }

    return detectorChannel;
}

/*****************************************************************************
 *
 * This routine sets the NULL defaults for a module to the default set
 * defined for the specified type.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetDefaults(Module *module)
{
    unsigned int m;
    unsigned int n;
    unsigned int k;
    unsigned int numDefaults;

    int j;
    int defLen;
    int status;

    char alias[MAXALIAS_LEN];
    char ignore[MAXALIAS_LEN];

    const char *tempAlias = "temporary_defaults";

    char **defNames = NULL;

    double *defValues = NULL;

    XiaDefaults *defaults = NULL;
    XiaDefaults *tempDefaults = NULL;

    XiaDaqEntry *current = NULL;

    byte_t *nameBlk = NULL;

    ASSERT(module);
    ASSERT(module->psl);

    if (!module->type || !module->psl) {
        xiaLog(XIA_LOG_ERROR, XIA_UNKNOWN_BOARD, "xiaSetDefaults",
               "No board type.");
        return XIA_UNKNOWN_BOARD;
    }

    numDefaults = module->psl->getNumDefaults();

    module->defaults = handel_md_alloc(module->number_of_channels *
                                       sizeof(*(module->defaults)));

    if (!module->defaults) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaSetDefaults",
               "Unable to allocate %d bytes for module->defaults.",
               (int) (module->number_of_channels * sizeof(*(module->defaults))));
        return XIA_NOMEM;
    }

    for (n = 0; n < module->number_of_channels; n++) {
        module->defaults[n] = handel_md_alloc(MAXALIAS_LEN);

        if (!module->defaults[n]) {
            xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaSetDefaults",
                   "Unable to allocate %d bytes for module->defaults[%u].",
                   MAXALIAS_LEN, n);
            return XIA_NOMEM;
        }

        memset(module->defaults[n], 0, MAXALIAS_LEN);
    }

    defNames = handel_md_alloc(numDefaults * sizeof(*defNames));

    if (!defNames) {
        /* Don't teardown memory allocated for a Module struct. That
         * cleanup should happen elsewhere.
         */
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaSetDefaults",
               "Unable to allocate %d bytes for defNames.",
               numDefaults * MAXITEM_LEN);
        return XIA_NOMEM;
    }

    nameBlk = handel_md_alloc(numDefaults * MAXITEM_LEN);

    if (!nameBlk) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaSetDefaults",
               "Unable to allocate %d bytes for nameBlk.",
               numDefaults * MAXITEM_LEN);
        return XIA_NOMEM;
    }

    memset(nameBlk, 0, numDefaults * MAXITEM_LEN);

    for (k = 0; k < numDefaults; k++) {
        defNames[k] = (char *)(nameBlk + (k * MAXITEM_LEN));
    }

    defValues = handel_md_alloc(numDefaults * sizeof(*defValues));

    if (!defValues) {
        handel_md_free(defNames);
        handel_md_free(nameBlk);

        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaSetDefaults",
               "Unable to allocate %d bytes for defValues.",
               (int) (numDefaults * sizeof(*defValues)));
        return XIA_NOMEM;
    }

    for (k = 0; k < numDefaults; k++) {
        defValues[k] = 0.0;
    }

    status = module->psl->getDefaultAlias(ignore, defNames, defValues);

    if (status != XIA_SUCCESS) {
        handel_md_free(defNames);
        handel_md_free(nameBlk);
        handel_md_free(defValues);

        xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
               "Error getting default alias information");
        return status;
    }

    /* The user no longer controls the defaults dynamically. This
     * is being done by Handel now. We create the default for
     * each channel based on the alias and modChan number.
     */

    for (m = 0; m < module->number_of_channels; m++) {

        sprintf(alias, "defaults_%s_%u", module->alias, m);

        defaults = xiaFindDefault(alias);

        /* If defaults is non-NULL then it means that it was loaded via
         * xiaLoadSystem() and is valid. Removing a module now removes all of
         * the defaults associated with that module so we no longer need to
         * worry about the case where the defaults are hanging around from a
         * prev. module definition.
         */
        /* if defaults == NULL, then we need to create a new list, else we just
         * go and create new entries iff there is no entry with a name matching
         * one returned from the PSL
         */
        if (defaults == NULL)
        {
            status = xiaNewDefault(alias);

            if (status != XIA_SUCCESS) {
                handel_md_free(defNames);
                handel_md_free(nameBlk);
                handel_md_free(defValues);

                xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                       "Error creating default with alias %s", alias);
                return status;
            }

            defaults = xiaFindDefault(alias);
        }

        /* We must copy the original defaults into a temporary default list,
         * then we will fill the original list with the values from the PSL.
         * Then we copy the temporary values into the original (thus
         * overwriting the values that were originally in the list).  Finally
         * we remove the temporary list
         */
        status = xiaNewDefault(tempAlias);

        if (status != XIA_SUCCESS) {
            handel_md_free(defNames);
            handel_md_free(nameBlk);
            handel_md_free(defValues);

            xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                   "Error creating %s default list", tempAlias);
            return status;
        }

        tempDefaults = xiaFindDefault(tempAlias);

        /* copy the original into the temporary */
        current = defaults->entry;
        while (current != NULL)
        {
            status =
                xiaAddDefaultItem(tempAlias, current->name, (void *) &(current->data));

            if (status != XIA_SUCCESS)
            {
                handel_md_free(defNames);
                handel_md_free(nameBlk);
                handel_md_free(defValues);

                xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                       "Error adding default %s (value = %.3f) to alias %s",
                       current->name, current->data, tempAlias);
                return status;
            }

            current = current->next;
        }

        /* now fill the original with the defaults from the PSL */
        defLen = (int)numDefaults;

        for (j = 0; j < defLen; j++)
        {
            status = xiaAddDefaultItem(alias, defNames[j], (void *)&(defValues[j]));

            if (status != XIA_SUCCESS)
            {
                handel_md_free(defNames);
                handel_md_free(nameBlk);
                handel_md_free(defValues);

                xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                       "Error adding default %s (value = %.3f) to alias %s",
                       defNames[j], defValues[j], alias);
                return status;
            }
        }

        /* Finally re-write the original values into the original list */
        current = tempDefaults->entry;
        while (current != NULL)
        {
            status =
                xiaAddDefaultItem(alias, current->name, (void *) &(current->data));

            if (status != XIA_SUCCESS)
            {
                handel_md_free(defNames);
                handel_md_free(nameBlk);
                handel_md_free(defValues);

                xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                       "Error adding default %s (value = %.3f) to alias %s",
                       current->name, current->data, alias);
                return status;
            }

            current = current->next;
        }

        /* Remove our TEMPORARY defaults list */
        status = xiaRemoveDefault(tempAlias);

        if (status != XIA_SUCCESS)
        {
            handel_md_free(defNames);
            handel_md_free(nameBlk);
            handel_md_free(defValues);

            xiaLog(XIA_LOG_ERROR, status, "xiaSetDefaults",
                   "Error removing the %s list", tempAlias);
            return status;
        }

        /* set the module entry for this channel to this default list */
        strcpy(module->defaults[m], alias);
    }

    /* clean up our memory */
    handel_md_free(defNames);
    handel_md_free(nameBlk);
    handel_md_free(defValues);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine gets the detector type string.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetDetectorType(Detector* detector, char* type)
{
    ASSERT(detector);
    ASSERT(type);

    switch (detector->type) {
        case XIA_DET_RESET:
            strcpy(type, "RESET");
            break;

        case XIA_DET_RCFEED:
            strcpy(type, "RC");
            break;

        default:
        case XIA_DET_UNKNOWN:
            xiaLog(XIA_LOG_ERROR, XIA_DET_UNKNOWN, "xiaSetDetectorType",
                   "No detector type specified for detector %s.", detector->alias);
            return XIA_MISSING_TYPE;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine either returns a pointer to a valid Module entry or NULL (if
 * a valid entry is not found) given a Detector alias.
 *
 *****************************************************************************/
HANDEL_SHARED Module* HANDEL_API xiaFindModuleFromDetAlias(const char *alias)
{
    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    Module *current = NULL;

    ASSERT(alias != NULL);
    ASSERT(strlen(alias) < (MAXALIAS_LEN - 1));

    /* Convert the name to lowercase */
    for (i = 0; i < (unsigned int)strlen(alias); i++) {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* Walk through the linked-list until we find it or we run out of elements */
    current = xiaGetModuleHead();

    while (current != NULL) {
        int detChan;

        for (detChan = 0; detChan < (int) current->number_of_channels; ++detChan) {
            if (STREQ(strtemp, current->detector[detChan])) {
                return current;
            }
        }

        current = current->next;
    }

    return NULL;
}


/*****************************************************************************
 *
 * This routine returns a detector channel index given a detector alias.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaFindDetIndexFromDetAlias(const char *alias)
{
    size_t i;
    int d;

    char strtemp[MAXALIAS_LEN];

    Module* module = xiaFindModuleFromDetAlias(alias);

    ASSERT(alias != NULL);
    ASSERT(strlen(alias) < (MAXALIAS_LEN - 1));

    if (module == NULL) {
        return XIA_NO_ALIAS;
    }

    /* Convert the name to lowercase */
    for (i = 0; i < strlen(alias); i++)
    {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    for (d = 0; d < (int) module->number_of_channels; d++) {
        if (STREQ(strtemp, module->detector[d])) {
            return d;
        }
    }

    return -1;
}

/*****************************************************************************
 *
 * This routine either returns a pointer to a valid Module entry and Detector
 * entry given a detector channel.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaFindModuleAndDetector(int detChan,
                                                      Module** module,
                                                      Detector** detector)
{
    int status;
    const char* modAlias;
    int modChan;

    Module* mod = NULL;
    Detector* det = NULL;

    if (module)
        *module = NULL;
    if (detector)
        *detector = NULL;

    modAlias = xiaGetAliasFromDetChan(detChan);

    if (modAlias == NULL) {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaFindModuleAndDetector",
               "detChan %d is not connected to a valid module", detChan);
        return status;
    }

    mod = xiaFindModule(modAlias);
    if (mod == NULL) {
        status = XIA_INVALID_DETCHAN;
        return status;
    }

    modChan = xiaGetModChan(detChan);

    if (modChan == 999) {
        status = XIA_INVALID_DETCHAN;
        return status;
    }

    if (mod->psl == NULL) {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaFindModuleAndDetector",
               "detChan %d module is not correctly set up", detChan);
        return status;
    }

    if (detector) {
        char * detAlias = mod->detector[modChan];

        det = xiaFindDetector(detAlias);
        if (det == NULL) {
            status = XIA_INVALID_DETCHAN;
            return status;
        }
    }

    if (module)
        *module = mod;
    if (detector)
        *detector = det;

    return XIA_SUCCESS;
}


/**********
 * This determines the absolute channel # of the
 * specified detChan in the specified module. For a
 * n-channel module, this value is between 0 and n-1.
 **********/
HANDEL_SHARED int HANDEL_API xiaGetAbsoluteChannel(int detChan, Module *module,
                                                   int *chan)
{
    int i;


    for (i = 0; i < (int) module->number_of_channels; i++) {
        if (module->channels[i] == detChan) {
            *chan = i;
            return XIA_SUCCESS;
        }
    }

    return XIA_BAD_CHANNEL;
}


/**********
 * Tags all of the "runActive" elements of the specified module
 **********/
HANDEL_SHARED int HANDEL_API xiaTagAllRunActive(Module *module, boolean_t state)
{
    unsigned int i;


    /* An ASSERT should go here indicating
     * that module is indeed a multichannel variant
     */

    /* We also might want to put some sort of
     * check in here to see if any of the values are
     * already 'state', which could indicate some sort
     * of weird error condition.
     */
    for (i = 0; i < module->number_of_channels; i++) {
        module->state->runActive[i] = state;
    }

    return XIA_SUCCESS;
}


/** Initialize the members of @a module. This routine does not allocate the
 * memory for the base @a module instance.
 */
HANDEL_STATIC int _initModule(Module *module, const char *alias)
{
    size_t aliasLen;


    ASSERT(module);
    ASSERT(alias);


    aliasLen = strlen(alias) + 1;

    module->alias = handel_md_alloc(aliasLen);

    if (!module->alias) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initModule",
               "Error allocating %d bytes for module 'alias'.", (int) aliasLen);
        return XIA_NOMEM;
    }

    strncpy(module->alias, alias, aliasLen);

    module->interface_ = handel_md_alloc(sizeof(*(module->interface_)));

    if (!module->interface_) {
        handel_md_free(module->alias);
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initModule",
               "Error allocating %lu bytes for module "
               "'interface'.", sizeof(*(module->interface_)));
        return XIA_NOMEM;
    }

    module->interface_->type = NO_INTERFACE;

    module->type               = NULL;
    module->number_of_channels = 0;
    module->channels           = NULL;
    module->detector           = NULL;
    module->detector_chan      = NULL;
    module->firmware           = NULL;
    module->defaults           = NULL;
    module->currentFirmware    = NULL;
    module->isValidated        = FALSE_;
    module->isMultiChannel     = FALSE_;
    module->state              = NULL;
    module->next               = NULL;
    module->ch                 = NULL;
    module->psl                = NULL;
    module->pslData            = NULL;

    return XIA_SUCCESS;
}


HANDEL_STATIC int _addModuleType(Module *module, void *type, const char *name)
{
    int status;

    unsigned int i;

    size_t n;

    char *requested = (char *)type;

    UNUSED(name);

    ASSERT(module != NULL);
    ASSERT(type != NULL);


    if (module->type != NULL) {
        status = XIA_TYPE_REDIRECT;
        xiaLog(XIA_LOG_ERROR, status, "_addModuleType",
               "Module '%s' already has type '%s'", module->alias, module->type);
        return status;
    }

    status = XIA_UNKNOWN_BOARD;

    for (i = 0; i < N_KNOWN_MODS; i++) {
        if (STREQ(KNOWN_MODS[i].alias, requested)) {
            int pslStatus;

            /* This must be NULL when this call is made. If not NULL it means a setup
             *  call with an end call. */
            ASSERT(module->psl == NULL);

            pslStatus = xiaGetPSLHandlers(KNOWN_MODS[i].actual, &module->psl);

            if (pslStatus != XIA_SUCCESS) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addModuleType",
                       "Error locating board type");
                return status;
            }

            n = strlen(KNOWN_MODS[i].actual) + 1;

            module->type = (char *)handel_md_alloc(n);

            if (module->type == NULL) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addModuleType",
                       "Error allocating module->type");
                return status;
            }

            strncpy(module->type, KNOWN_MODS[i].actual, n);
            status = XIA_SUCCESS;
            break;
        }
    }

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addModuleType",
               "Error finding module type '%s'", requested);
        return status;
    }

    return XIA_SUCCESS;
}


/**********
 * This particular routine is quite important: once we know
 * the number of channels in the system we have a green light
 * to allocate the rest of the memory in the module
 * structure.
 **********/
HANDEL_STATIC int _addNumChans(Module *module, void *nChans, const char *name)
{
    int status;

    unsigned int n        = *((unsigned int *)nChans);
    unsigned int numInits = (unsigned int)NUM_INITS;
    unsigned int i;


    UNUSED(name);

    ASSERT(module != NULL);
    ASSERT(nChans != NULL);


    /* There should be some limits on n, no? Or do we leave that to the PSL
     * verification step? It seems that the sooner we can give feedback, the
     * better off we will be.
     */

    module->number_of_channels = n;

    for (i = 0; i < numInits; i++) {
        status = inits[i](module);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "_addNumChans",
                   "Error initializing module '%s' memory (i = %u)",
                   module->alias, i);
            return status;
        }
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _doAddModuleItem(Module *module, void *data,
                                   unsigned int i, const char *name)
{
    int status;


    ASSERT(module != NULL);
    ASSERT(data != NULL);
    ASSERT(i < (unsigned int)NUM_ITEMS);


    if (items[i].needsBT) {

        if (module->type == NULL) {
            status = XIA_NEEDS_BOARD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "_doAddModuleItem",
                   "Item '%s' requires the module ('%s') board_type to be set first",
                   items[i].name, module->alias);
            return status;
        }
    }

    status = items[i].f(module, data, name);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_doAddModuleItem",
               "Error adding module item '%s' to module '%s'",
               items[i].name, module->alias);
        return status;
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initDefaults(Module *module)
{
    int status;


    ASSERT(module != NULL);


    /* Right now, we are just calling the xiaSetDefaults()
     * routine, which probably should be reworked and
     * put into this routine.
     */
    status = xiaSetDefaults(module);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_initDefaults",
               "Error initializing defaults for module '%s'", module->alias);
        return status;
    }

    return XIA_SUCCESS;
}


/**********
 * Initialize the channel structure
 **********/
HANDEL_STATIC int _initChannels(Module *module)
{
    unsigned int i;

    size_t nBytes = module->number_of_channels * sizeof(Channel_t);


    ASSERT(module != NULL);


    module->ch = (Channel_t *)handel_md_alloc(nBytes);

    if (!module->ch) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initChannels",
               "Error allocating %ld bytes for module->ch", (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        module->ch[i].n_sca = 0;
        module->ch[i].sca_lo = NULL;
        module->ch[i].sca_hi = NULL;
        module->ch[i].data.data = NULL;
        module->ch[i].data.length = 0;
        module->ch[i].pslData = NULL;
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initDetectors(Module *module)
{
    unsigned int i;

    size_t nBytes       = module->number_of_channels * sizeof(char *);
    size_t nBytesMaxStr = MAXALIAS_LEN * sizeof(char);


    ASSERT(module != NULL);


    module->detector = (char **)handel_md_alloc(nBytes);

    if (!module->detector) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initDetectors",
               "Error allocating %ld bytes for module->detector", (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        module->detector[i] = (char *)handel_md_alloc(nBytesMaxStr);

        if (!module->detector[i]) {
            xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initDetectors",
                   "Error allocating %ld bytes for module->detector[%u]",
                   (long)nBytesMaxStr, i);
            return XIA_NOMEM;
        }

        strncpy(module->detector[i], MODULE_NULL_STRING, MODULE_NULL_STRING_LEN);
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initDetectorChans(Module *module)
{
    unsigned int i;

    size_t nBytes = module->number_of_channels * sizeof(int);


    ASSERT(module != NULL);


    module->detector_chan = (int *)handel_md_alloc(nBytes);

    if (!module->detector_chan) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initDetectorChans",
               "Error allocating %ld bytes for module->detector_chan",
               (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        module->detector_chan[i] = -1;
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initFirmware(Module *module)
{
    unsigned int i;

    size_t nBytes       = module->number_of_channels * sizeof(char *);
    size_t nBytesMaxStr = MAXALIAS_LEN * sizeof(char);


    ASSERT(module != NULL);


    module->firmware = (char **)handel_md_alloc(nBytes);

    if (!module->firmware) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initFirmware",
               "Error allocating %ld bytes for module->firmware", (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        module->firmware[i] = (char *)handel_md_alloc(nBytesMaxStr);

        if (!module->firmware[i]) {
            xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initFirmware",
                   "Error allocating %ld bytes for module->detector[%u]",
                   (long)nBytesMaxStr, i);
            return XIA_NOMEM;
        }

        strncpy(module->firmware[i], MODULE_NULL_STRING, MODULE_NULL_STRING_LEN);
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initCurrentFirmware(Module *module)
{
    unsigned int i;

    size_t nBytes = module->number_of_channels * sizeof(CurrentFirmware);


    ASSERT(module != NULL);


    module->currentFirmware = (CurrentFirmware *)handel_md_alloc(nBytes);

    if (!module->currentFirmware) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initCurrentFirmware",
               "Error allocating %ld bytes for module->currentFirmware",
               (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        strncpy(module->currentFirmware[i].currentFiPPI,     MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
        strncpy(module->currentFirmware[i].currentUserFiPPI, MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
        strncpy(module->currentFirmware[i].currentDSP,       MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
        strncpy(module->currentFirmware[i].currentUserDSP,   MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
        strncpy(module->currentFirmware[i].currentMMU,       MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
        strncpy(module->currentFirmware[i].currentSysFPGA,   MODULE_NULL_STRING,
                MODULE_NULL_STRING_LEN);
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int _initMultiState(Module *module)
{
    unsigned int i;

    size_t nBytes     = module->number_of_channels * sizeof(MultiChannelState);
    size_t nBytesFlag = module->number_of_channels * sizeof(boolean_t);


    ASSERT(module != NULL);


    if (module->number_of_channels > 1) {
        module->isMultiChannel = TRUE_;

        module->state = (MultiChannelState *)handel_md_alloc(nBytes);

        if (!module->state) {
            xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initMultiState",
                   "Error allocating %ld bytes for module->state", (long)nBytes);
            return XIA_NOMEM;
        }

        module->state->runActive = (boolean_t *)handel_md_alloc(nBytesFlag);

        if (!module->state->runActive) {
            xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initMultiState",
                   "Error allocating %ld bytes for module->state->runActive",
                   (long)nBytesFlag);
            return XIA_NOMEM;
        }

        for (i = 0; i < module->number_of_channels; i++) {
            module->state->runActive[i] = FALSE_;
        }

    } else {
        module->isMultiChannel = FALSE_;
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is responsible for handling any of the items
 * that start with a 'channel' in their name, such as
 * "channel{n}_alias" and "channel{n}_detector".
 **********/
HANDEL_STATIC int _addChannel(Module *module, void *val, const char *name)
{
    int status;

    int i;
    int nChanTypes = NUM_CHAN_TYPES;
    int idx = 0;

    char type[MAXALIAS_LEN];


    ASSERT(module != NULL);
    ASSERT(val != NULL);
    ASSERT(name != NULL);


    /* 1) Parse off and verify channel number */
    status = _splitIdxAndType(name, &idx, type);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addChannel",
               "Error parsing channel item '%s'", name);
        return status;
    }

    if (idx >= (int) module->number_of_channels) {
        xiaLog(XIA_LOG_ERROR, status, "_addChannel",
               "Parsed channel '%u' > number channels in module '%u'",
               idx, module->number_of_channels);
        return XIA_BAD_NAME;
    }

    /* 2) Deal w/ specific: alias, detector, gain */
    for (i = 0; i < nChanTypes; i++) {
        if (STRNEQ(chanTypes[i].name, type)) {
            status = chanTypes[i].f(module, idx, val);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "_addChannel",
                       "Error adding '%s' type to channel %u", type, idx);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    /* _splitIdxAndType does some verification of the
     * type already, so something unusual has happened.
     */
    FAIL();
    /* Shouldn't return this value because of the FAIL */
    return XIA_UNKNOWN;
}


HANDEL_STATIC int _splitIdxAndType(const char *str, int *idx, char *type)
{
    size_t i;
    size_t nChanTypes = NUM_CHAN_TYPES;

    const char *channel    = NULL;
    char *underscore = NULL;


    ASSERT(str != NULL);
    ASSERT(idx != NULL);
    ASSERT(type != NULL);


    /* NULL out '_' so we have two separate
     * strings to manipulate. String 1
     * will be channel{n} and String 2
     * will be the type of channel item.
     */
    underscore = strrchr(str, '_');

    if (underscore == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_NAME, "_splitIdxAndType",
               "Malformed item string: '%s'. Missing '_'", str);
        return XIA_BAD_NAME;
    }

    underscore[0] = '\0';

    channel = str;

    /* This ASSERT covers an impossible
     * condition since the assumption
     * is that the calling routine has
     * verified that the first part of the
     * string is "channel".
     */
    ASSERT(STRNEQ(channel, "channel"));

    sscanf(channel + CHANNEL_IDX_OFFSET, "%u", idx);

    /* Do some simple verification here?
     * The other option is to require that
     * the calling routine verify the returned
     * values, which I don't think is appropriate
     * in this case.
     */
    underscore++;
    strncpy(type, underscore, strlen(underscore) + 1);

    for (i = 0; i < nChanTypes; i++) {
        if (STRNEQ(type, chanTypes[i].name)) {
            return XIA_SUCCESS;
        }
    }

    xiaLog(XIA_LOG_ERROR, XIA_BAD_NAME, "_splitIdxAndType",
           "Unknown channel item type: '%s'", type);

    return XIA_BAD_NAME;
}


HANDEL_STATIC int _initChanAliases(Module *module)
{
    unsigned int i;

    size_t nBytes = module->number_of_channels * sizeof(int);


    ASSERT(module != NULL);


    module->channels = (int *)handel_md_alloc(nBytes);

    if (!module->channels) {
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "_initChanAliases",
               "Error allocating %ld bytes for module->channels", (long)nBytes);
        return XIA_NOMEM;
    }

    for (i = 0; i < module->number_of_channels; i++) {
        module->channels[i] = -1;
    }

    return XIA_SUCCESS;
}


/**
 * Parses the index of the detector channel from strings
 * of the form "{detector alias}:{n}". Assumes that
 * the calling function has allocated memory for "alias".
 */
HANDEL_STATIC int _parseDetectorIdx(const char *str, int *idx, char *alias)
{
    char *colon = NULL;


    ASSERT(str != NULL);
    ASSERT(idx != NULL);
    ASSERT(alias != NULL);


    colon = strrchr(str, ':');

    if (colon == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_VALUE, "_parseDetectorIdx",
               "Malformed detector string: '%s'. Missing ':'", str);
        return XIA_BAD_VALUE;
    }

    /* XXX: nulling out the color as a side effect in order to get the
     * length of preceding alias? Couldn't we use (str-colon)?
     */
    colon[0] = '\0';
    colon++;

    sscanf(colon, "%d", idx);

    strncpy(alias, str, strlen(str) + 1);

    return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessFirmware().
 * In the future, xiaProcessFirmware() should be refactored into
 * _addFirmware().
 **********/
HANDEL_STATIC int  _addFirmware(Module *module, void *val, const char *name)
{
    int status;


    ASSERT(module != NULL);
    ASSERT(val != NULL);
    ASSERT(name != NULL);


    status = xiaProcessFirmware(module, name, val);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addFirmware",
               "Error adding firmware '%s' to module '%s'",
               (char *)val, module->alias);
        return status;
    }

    return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessDefault().
 * In the future, xiaProcessDefault() should be refactored into
 * _addDefault().
 **********/
HANDEL_STATIC int  _addDefault(Module *module, void *val, const char *name)
{
    int status;


    ASSERT(module != NULL);
    ASSERT(val != NULL);
    ASSERT(name != NULL);


    status = xiaProcessDefault(module, name, val);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addDefault",
               "Error adding default '%s' to module '%s'",
               (char *)val, module->alias);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Add channel PSL data.
 */
HANDEL_STATIC int _addData(Module *module, void *val, const char *name)
{
    GenBuffer *buf = (GenBuffer*)val;

    int status;
    int j;

    xiaLog(XIA_LOG_DEBUG, "_addData", "name = %s", name);

    /* Determine if the name string is "data_all" or "data_chan{n}" */
    if (STREQ(name, "data_all")) {
        for (j = 0; j < (int)module->number_of_channels; j++) {
            module->ch[j].data.data = handel_md_alloc(buf->length);
            if (module->ch[j].data.data == NULL) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addData",
                       "Unable to allocate memory for chosen->data[%d]", j);
                return status;
            }

            memcpy(module->ch[j].data.data, buf->data, buf->length);
            module->ch[j].data.length = buf->length;
        }

        return XIA_SUCCESS;
    }

    /* At this point we have a "chan{n}" name or a bad name. */
    if (sscanf(name, "data_chan%d", &j) == 1) {
        xiaLog(XIA_LOG_DEBUG, "_addData", "idx = %d", j);

        if ((j >= (int)module->number_of_channels) ||
            (j < 0)) {
            status = XIA_BAD_CHANNEL;
            xiaLog(XIA_LOG_ERROR, status, "_addData", "Specified channel is invalid");
            return status;
        }

        xiaLog(XIA_LOG_DEBUG, "_addData",
               "name = %s, new value = %.32s",
               name, (char*)buf->data);

        module->ch[j].data.data = handel_md_alloc(buf->length);
        if (module->ch[j].data.data == NULL) {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "_addData",
                   "Unable to allocate memory for chosen->data[%d]", j);
            return status;
        }

        memcpy(module->ch[j].data.data, buf->data, buf->length);
        module->ch[j].data.length = buf->length;
    }
    else {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "_addData", "Invalid name: %s", name);
        return status;
    }

    return XIA_SUCCESS;
}


/**********
 * Currently, this routine is a wrapper around xiaProcessInterface().
 * In the future, xiaProcessInterface() should be refactored into
 * _addInterface().
 **********/
HANDEL_STATIC int  _addInterface(Module *module, void *val, const char *name)
{
    int status;


    ASSERT(module != NULL);
    ASSERT(val != NULL);
    ASSERT(name != NULL);


    status = xiaProcessInterface(module, name, val);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "_addInterface",
               "Error adding interface component '%s' to module '%s'",
               name, module->alias);
        return status;
    }

    return XIA_SUCCESS;
}


/** @brief Returns the module alias for the specified detChan.
 *
 * Assumes that the user has allocated enough memory to hold the entire
 * module alias. The maximum alias size is MAXALIAS_LEN.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaModuleFromDetChan(int detChan, char *alias)
{
    Module *m = xiaGetModuleHead();

    unsigned int i;


    if (!alias) {
        xiaLog(XIA_LOG_ERROR, XIA_NULL_ALIAS, "xiaModuleFromDetChan",
               "'alias' may not be NULL.");
        return XIA_NULL_ALIAS;
    }

    while (m != NULL) {

        for (i = 0; i < m->number_of_channels; i++) {
            if (detChan == m->channels[i]) {
                strncpy(alias, m->alias, MAXALIAS_LEN);
                return XIA_SUCCESS;
            }
        }

        m = m->next;
    }

    xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaModuleFromDetChan",
           "detChan %d is not defined in any of the known modules", detChan);
    return XIA_INVALID_DETCHAN;
}


/** @brief Converts the specified detChan into a detector alias.
 *
 * Assumes that the user has allocated enough memory to hole the entire
 * module alias. The maximum alias size if MAXALIAS_LEN.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaDetectorFromDetChan(int detChan, char *alias)
{
    Module *m = xiaGetModuleHead();

    unsigned int i;

    char *t = NULL;


    if (!alias) {
        xiaLog(XIA_LOG_ERROR, XIA_NULL_ALIAS, "xiaDetectorFromDetChan",
               "'alias' may not be NULL.");
        return XIA_NULL_ALIAS;
    }

    while (m != NULL) {

        for (i = 0; i < m->number_of_channels; i++) {
            if (detChan == m->channels[i]) {
                /* The detector aliases are stored in the Module structure as
                 * "alias:{n}", where "n" represents the physical detector
                 * preamplifier that the detChan is bound to. We need to strip
                 * that extra information from the alias.
                 */
                t = strtok(m->detector[i], ":");
                strcpy(alias, t);
                return XIA_SUCCESS;
            }
        }

        m = m->next;
    }

    xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaDetectorFromDetChan",
           "detChan %d is not defined in any of the known modules", detChan);
    return XIA_INVALID_DETCHAN;
}


/*****************************************************************************
 *
 * Return the module list's head.
 *
 *****************************************************************************/
HANDEL_SHARED Module * HANDEL_API xiaGetModuleHead(void)
{
    return xiaModuleHead;
}

/*****************************************************************************
 *
 * This routine clears the Module LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInitModuleDS(void)
{
    xiaModuleHead = NULL;
    return XIA_SUCCESS;
}
