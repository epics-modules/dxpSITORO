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


#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <errno.h>

#include "xia_module.h"
#include "xia_common.h"
#include "xia_assert.h"
#include "xia_file.h"
#include "xia_handel.h"
#include "xia_handel_structures.h"

#include "handel_generic.h"
#include "handel_errors.h"
#include "handel_file.h"
#include "handel_log.h"

#include "base64.h"

PRAGMA_PUSH
PRAGMA_IGNORE_STRICT_PROTOTYPES

#include "miniz.h"

PRAGMA_POP

/** Structures **/

/* This structure exists so that we can re-use the
 * section of the code that parses in the sections
 * of the ini files
 */
typedef struct
{
    /* Pointer to the proper xiaLoadRoutine */
    int (*function_ptr)(FILE *, fpos_t *, fpos_t *);

    /* Section heading name: the part in brackets */
    const char *section;

    boolean_t multiSection;

} SectionInfo;


/** Prototypes **/
HANDEL_STATIC int HANDEL_API xiaWriteIniFile(const char *filename);

HANDEL_STATIC int HANDEL_API xiaFindEntryStart(FILE *fp, const char *section, fpos_t *start);
HANDEL_STATIC int HANDEL_API xiaFindEntryLimits(FILE *fp, const char *section,
                        fpos_t *start, fpos_t *end);
HANDEL_STATIC int HANDEL_API xiaGetLine_N(FILE *fp, char *lline, int len);
HANDEL_STATIC int HANDEL_API xiaGetLine(FILE *fp, char *line);
HANDEL_STATIC int HANDEL_API xiaGetLineData(const char *line,
                                            char *name, char *value);
HANDEL_STATIC int HANDEL_API xiaSetPosOnNext(FILE *fp, fpos_t *start, fpos_t *end,
                                             const char *name, fpos_t *newPos,
                                             boolean_t after);
HANDEL_SHARED int HANDEL_API xiaCopyFile(const char *src, const char *dest);

HANDEL_STATIC int xiaLoadDetector(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadModule(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadModChanData(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadFirmware(FILE *fp, fpos_t *start, fpos_t *end);
HANDEL_STATIC int xiaLoadDefaults(FILE *fp, fpos_t *start, fpos_t *end);

HANDEL_STATIC int HANDEL_API xiaReadPTRRs(FILE *fp, fpos_t *start, fpos_t *end, char *alias);
HANDEL_STATIC int HANDEL_API xiaReadChanData(FILE *fp);

static int writeInterface(FILE *fp, Module *m);

typedef int (*interfaceWrite_FP)(FILE *, Module *);

typedef struct _InterfaceWriter {
  unsigned int type;
  interfaceWrite_FP fn;
} InterfaceWriters_t;

#ifndef EXCLUDE_INET
static int writeINET(FILE *fp, Module *module);
#endif /* EXCLUDE_INET */

static InterfaceWriters_t INTERFACE_WRITERS[] = {
  /* Sentinel */
  {0, NULL},
#ifndef EXCLUDE_INET
  {INET,          writeINET},
#endif /* EXCLUDE_INET */
};

static SectionInfo sectionInfo[] =
{
 {xiaLoadDetector,    "detector definitions", TRUE_},
 {xiaLoadFirmware,    "firmware definitions", TRUE_},
 {xiaLoadDefaults,    "default definitions", TRUE_},
 {xiaLoadModule,      "module definitions", TRUE_},
 {xiaLoadModChanData, "module channel data", FALSE_}
};


/** @brief Loads in a save file of type @a type.
 *
 * When Handel loads a system it first must clear out the existing configuration
 * in order to allow the other configuration calls to succeed. If you load a
 * file that is malformed, you will also lose the existing configuration.
 */
HANDEL_EXPORT int HANDEL_API xiaLoadSystem(const char *type, const char *filename)
{
    int status;


    if (type == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_NULL_TYPE, "xiaLoadSystem",
               ".INI file 'type' string is NULL");
        return XIA_NULL_TYPE;
    }

    if (filename == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_NO_FILENAME, "xiaLoadSystem",
               ".INI file 'name' string is NULL");
        return XIA_NO_FILENAME;
    }

    /* If we support different output types in the future, we need to change
     * this logic around.
     */
    if (!STREQ(type, "handel_ini")) {
      xiaLog(XIA_LOG_ERROR, XIA_FILE_TYPE, "xiaLoadSystem",
             "Unknown file type '%s' for target save file '%s'", type, filename);
        return XIA_FILE_TYPE;
    }

    /* We need to clear and re-initialize Handel */
    status = xiaInitHandel();

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadSystem",
               "Error reinitializing Handel");
        return status;
    }

    status = xiaReadIniFile(filename);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadSystem",
               "Error reading in .INI file '%s'", filename);
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine saves the configuration to the file filename and of type type.
 * Currently, the only supported types are "handel_ini".
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSaveSystem(const char *type, const char *filename)
{
    int status;


    if (STREQ(type, "handel_ini")) {

        status = xiaWriteIniFile(filename);

    } else {

        status = XIA_FILE_TYPE;
    }

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaSaveSystem",
               "Error writing %s", filename);
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine writes out a "handel_ini" file based on the current
 * information in the data structures.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaWriteIniFile(const char *filename)
{
    int status;
    int i;

    unsigned int j;

    FILE *iniFile = NULL;
    char tmpFilename[MAX_PATH_LEN];
    const char tmpExt[] = ".tmp";

    char* path = NULL;
    const char* lastSlash = NULL;

    char typeStr[MAXITEM_LEN];

    Detector    *detector    = NULL;
    FirmwareSet *firmwareSet = NULL;
    Firmware    *firmware    = NULL;
    XiaDefaults *defaults    = NULL;
    XiaDaqEntry *entry       = NULL;
    Module      *module      = NULL;


    if ((filename == NULL) || (STREQ(filename, "")))
    {
        status = XIA_NO_FILENAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
               "Filename is either NULL or empty, illegal value");
        return status;
    }

    /*
     * Write a temp file first so we don't wreck the user's file if anything
     * goes wrong.
     */
    strncpy(tmpFilename, filename, sizeof(tmpFilename) - 1);
    strncpy(tmpFilename + strlen(filename), tmpExt,
            sizeof(tmpFilename) - 1 - strlen(filename));

    iniFile = xia_file_open(tmpFilename, "wb");
    if (iniFile == NULL)
    {
        status = XIA_OPEN_FILE;
        xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
               "Could not open %s", filename);
        return status;
    }

    /*
     * Get the path, ie dirname(filename).
     */

    lastSlash = filename + strlen(filename) - 1;

    while (lastSlash != filename)
    {
        if ((*lastSlash == HANDLE_PATHNAME_SEP) || (*lastSlash == '/'))
            break;
        --lastSlash;
    }

    if (lastSlash != filename)
    {
        path = handel_md_alloc((size_t) (lastSlash - filename + 1));
        if (path == NULL)
        {
            xia_file_close(iniFile);
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                   "No memory for path: %s", filename);
            return status;
        }

        memcpy(path, filename, (size_t) (lastSlash - filename));
        path[lastSlash - filename] = '\0';
    }

    /* Write the sections in the same order that they are read in */
    fprintf(iniFile, "[detector definitions]\n\n");

    for (detector = xiaGetDetectorHead(), i = 0;
         detector != NULL;
         detector = detector->next, i++)
    {
        fprintf(iniFile, "START #%d\n", i);
        fprintf(iniFile, "alias = %s\n", detector->alias);
        fprintf(iniFile, "number_of_channels = %hu\n", detector->nchan);

        module = xiaFindModuleFromDetAlias(detector->alias);

        if (module && module->psl->iniWrite)
        {
            status = module->psl->iniWrite(iniFile, "detector", path,
                                           detector, i, module);

            if (status != XIA_SUCCESS)
            {
                if (path)
                    handel_md_free(path);
                xia_file_close(iniFile);
                status = XIA_UNKNOWN;
                xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                       "PSL Detector Write failure");
                return status;
            }
        }

        switch (detector->type)
        {
            case XIA_DET_RESET:
                strcpy(typeStr, "reset");
                break;

            case XIA_DET_RCFEED:
                strcpy(typeStr, "rc_feedback");
                break;

            default:
            case XIA_DET_UNKNOWN:
                if (path)
                    handel_md_free(path);
                xia_file_close(iniFile);
                status = XIA_MISSING_TYPE;
                xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                       "Unknown detector type for alias %s", detector->alias);
                return status;
        }

        fprintf(iniFile, "type = %s\n", typeStr);
        fprintf(iniFile, "type_value = %3.3f\n", detector->typeValue[0]);

        for (j = 0; j < detector->nchan; j++)
        {
            fprintf(iniFile, "channel%u_gain = %3.6f\n", j, detector->gain[j]);

            switch (detector->polarity[j])
            {
                case 0: /* Negative */
                    fprintf(iniFile, "channel%u_polarity = %s\n", j, "-");
                    break;

                case 1: /* Positive */
                    fprintf(iniFile, "channel%u_polarity = %s\n", j, "+");
                    break;

                default:
                    if (path)
                        handel_md_free(path);
                    xia_file_close(iniFile);
                    status = XIA_UNKNOWN;
                    xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                           "Impossible polarity error");
                    return status;
            }

        }

        fprintf(iniFile, "END #%d\n\n", i);
    }

    fprintf(iniFile, "[firmware definitions]\n\n");

    for (firmwareSet = xiaGetFirmwareSetHead(), i = 0;
         firmwareSet != NULL;
         firmwareSet = firmwareSet->next, i++)
    {
        fprintf(iniFile, "START #%d\n", i);
        fprintf(iniFile, "alias = %s\n", firmwareSet->alias);

        if (module && module->psl->iniWrite)
        {
            status = module->psl->iniWrite(iniFile, "firmware", path,
                                           firmware, i, module);

            if (status != XIA_SUCCESS)
            {
                if (path)
                    handel_md_free(path);
                xia_file_close(iniFile);
                status = XIA_UNKNOWN;
                xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                       "PSL Firmare Write failure");
                return status;
            }
        }

        if (firmwareSet->mmu != NULL)
        {
            fprintf(iniFile, "mmu = %s\n", firmwareSet->mmu);
        }

        if (firmwareSet->filename != NULL)
        {
            fprintf(iniFile, "filename = %s\n", firmwareSet->filename);

            if (firmwareSet->tmpPath != NULL) {
                fprintf(iniFile, "fdd_tmp_path = %s\n", firmwareSet->tmpPath);
            }

            fprintf(iniFile, "num_keywords = %u\n", (int) firmwareSet->numKeywords);

            for (j = 0; j < firmwareSet->numKeywords; j++)
            {
                fprintf(iniFile, "keyword%u = %s\n", j, firmwareSet->keywords[j]);
            }

        } else {

            firmware = firmwareSet->firmware;
            while (firmware != NULL)
            {
                fprintf(iniFile, "ptrr = %hu\n", firmware->ptrr);
                fprintf(iniFile, "min_peaking_time = %3.3f\n", firmware->min_ptime);
                fprintf(iniFile, "max_peaking_time = %3.3f\n", firmware->max_ptime);

                if (firmware->fippi != NULL)
                {
                    fprintf(iniFile, "fippi = %s\n", firmware->fippi);
                }

                if (firmware->user_fippi != NULL)
                {
                    fprintf(iniFile, "user_fippi = %s\n", firmware->user_fippi);
                }

                if (firmware->dsp != NULL)
                {
                    fprintf(iniFile, "dsp = %s\n", firmware->dsp);
                }

                fprintf(iniFile, "num_filter = %hu\n", firmware->numFilter);

                for (j = 0; j < firmware->numFilter; j++)
                {
                    fprintf(iniFile,
                            "filter_info%u = %hu\n", j, firmware->filterInfo[j]);
                }

                firmware = firmware->next;
            }
        }

        fprintf(iniFile, "END #%d\n\n", i);
    }

    fprintf(iniFile, "***** Generated by Handel -- DO NOT MODIFY *****\n");

    fprintf(iniFile, "[default definitions]\n\n");

    for (defaults = xiaGetDefaultsHead(), i = 0;
         defaults != NULL;
         defaults = defaults->next, i++)
    {
        fprintf(iniFile, "START #%d\n", i);
        fprintf(iniFile, "alias = %s\n", defaults->alias);

        if (module && module->psl->iniWrite)
        {
            status = module->psl->iniWrite(iniFile, "defaults", path,
                                           defaults, i, module);

            if (status != XIA_SUCCESS)
            {
                if (path)
                    handel_md_free(path);
                xia_file_close(iniFile);
                status = XIA_UNKNOWN;
                xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                       "PSL Defaults Write failure");
                return status;
            }
        }

        entry = defaults->entry;
        while ((entry != NULL) && (strlen(entry->name) > 0))
        {
            fprintf(iniFile, "%s = %3.6f\n", entry->name, entry->data);
            entry = entry->next;
        }

        fprintf(iniFile, "END #%d\n\n", i);
    }

    fprintf(iniFile, "***** End of Generated Information *****\n\n");

    fprintf(iniFile, "[module definitions]\n\n");

    for (module = xiaGetModuleHead(), i = 0;
         module != NULL;
         module = module->next, i++)
    {
        fprintf(iniFile, "START #%d\n", i);
        fprintf(iniFile, "alias = %s\n", module->alias);
        fprintf(iniFile, "module_type = %s\n", module->type);

        status = writeInterface(iniFile, module);

        if (status != XIA_SUCCESS) {
            if (path)
                handel_md_free(path);
            xia_file_close(iniFile);
            xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                   "Error writing interface information for module '%s'",
                   module->alias);
            return status;
        }

        if (module && module->psl->iniWrite)
        {
            status = module->psl->iniWrite(iniFile, "module", path,
                                           NULL, i, module);

            if (status != XIA_SUCCESS)
            {
                if (path)
                    handel_md_free(path);
                xia_file_close(iniFile);
                status = XIA_UNKNOWN;
                xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
                       "PSL Module Write failure");
                return status;
            }
        }

        fprintf(iniFile, "number_of_channels = %u\n", module->number_of_channels);

        for (j = 0; j < module->number_of_channels; j++)
        {
            fprintf(iniFile, "channel%u_alias = %d\n", j, module->channels[j]);
            fprintf(iniFile, "channel%u_detector = %s:%u\n", j,
                    module->detector[j], (unsigned int)module->detector_chan[j]);
            fprintf(iniFile, "firmware_set_chan%u = %s\n", j, module->firmware[j]);
            fprintf(iniFile, "default_chan%u = %s\n", j, module->defaults[j]);
        }

        fprintf(iniFile, "END #%d\n\n", i);
    }

    fprintf(iniFile, "[module channel data]\n\n");

    for (module = xiaGetModuleHead(), i = 0;
         module != NULL;
         module = module->next, i++)
    {
        fprintf(iniFile, "START %s\n", module->alias);

        for (j = 0; j < module->number_of_channels; j++)
        {
            /*
             * Base64-encode a blob of data from each channel that has it.
             * The PSL manages the format of the blob.
             */
            if (module->ch[j].data.length > 0) {
                uLong dataCmpSize;
                byte_t *dataCmp;
                int cmpStatus;

                dataCmpSize = compressBound((uLong)module->ch[j].data.length);

                dataCmp = handel_md_alloc(dataCmpSize);
                if (!dataCmp) {
                    if (path)
                        handel_md_free(path);
                    xia_file_close(iniFile);
                    return XIA_NOMEM;
                }

                cmpStatus = compress(dataCmp, &dataCmpSize,
                                     module->ch[j].data.data,
                                     (uLong)module->ch[j].data.length);
                if (cmpStatus != Z_OK) {
                    if (path)
                        handel_md_free(path);
                    xia_file_close(iniFile);
                    handel_md_free(dataCmp);
                    xiaLog(XIA_LOG_ERROR, XIA_ENCODE, "xiaWriteIniFile",
                           "Compressing %s data_chan%u: %d",
                           module->alias, j, cmpStatus);
                    return XIA_ENCODE;
                }

                char *dataEnc;
                size_t dataEncSize;
                int encStatus;

                dataEncSize = dataCmpSize * 4 / 3;                /* Inflate by 4/3 */
                dataEncSize = ((dataEncSize - 1) | (4-1)) + 1;    /* Get a multiple of 4 */
                dataEncSize++;                                    /* Add the null terminator */

                dataEnc = handel_md_alloc(dataEncSize);
                if (!dataEnc) {
                    handel_md_free(dataCmp);
                    if (path)
                        handel_md_free(path);
                    xia_file_close(iniFile);
                    return XIA_NOMEM;
                }

                encStatus = Base64Encode(dataCmp, dataCmpSize,
                                         dataEnc, dataEncSize);
                if (encStatus) {
                    handel_md_free(dataCmp);
                    if (path)
                        handel_md_free(path);
                    xia_file_close(iniFile);
                    handel_md_free(dataEnc);
                    xiaLog(XIA_LOG_ERROR, XIA_ENCODE, "xiaWriteIniFile",
                           "Encoding %s data_chan%u: %d",
                           module->alias, j, encStatus);
                    return XIA_ENCODE;
                }

                handel_md_free(dataCmp);

                fprintf(iniFile, "data_chan%u_len = %zu\n", j, strlen(dataEnc));
				fprintf(iniFile, "data_chan%u = ", j);
				fputs(dataEnc, iniFile);
				fprintf(iniFile, "\n");

                handel_md_free(dataEnc);
            }
        }

        fprintf(iniFile, "END %s\n", module->alias);
    }

    if (path)
        handel_md_free(path);

    xia_file_close(iniFile);

    status = xiaCopyFile(tmpFilename, filename);
    if (status == XIA_SUCCESS) {
        remove(tmpFilename);
    }
    else {
        status = XIA_BAD_FILE_WRITE;
        xiaLog(XIA_LOG_ERROR, status, "xiaWriteIniFile",
               "Failed to copy temp file %s to destination %s.", tmpFilename, filename);
        return status;
    }

    return XIA_SUCCESS;
}

HANDEL_SHARED int HANDEL_API xiaCopyFile(const char *src, const char *dest)
{
    int status;
    FILE *srcfp = NULL, *destfp = NULL;
    struct stat sb;
    char *data;

    size_t objsRead;

	size_t toWrite;
    size_t haveWritten = 0;
    size_t written =  0;


    ASSERT(src);
    ASSERT(dest);


    xiaLog(XIA_LOG_DEBUG, "xiaCopyFile", "Copying file %s to %s", src, dest);

    srcfp = xia_file_open(src, "rb");

    if (srcfp == NULL) {
        status = XIA_OPEN_FILE;
        xiaLog(XIA_LOG_ERROR, status, "xiaCopyFile", "Could not open %s", src);
        return status;
    }

    destfp = xia_file_open(dest, "w");

    if (destfp == NULL) {
        xia_file_close(srcfp);
        status = XIA_OPEN_FILE;
        xiaLog(XIA_LOG_ERROR, status, "xiaCopyFile", "Could not open %s", dest);
        return status;
    }

    if (stat(src, &sb) < 0) {
        xia_file_close(srcfp);
        xia_file_close(destfp);
        xiaLog(XIA_LOG_ERROR, XIA_NOT_FOUND, "xiaCopyFile",
               "Could not stat: %s", src);
        return XIA_NOT_FOUND;
    }

	data = handel_md_alloc((size_t) (sb.st_size + 1));

    if (data == NULL) {
        xia_file_close(srcfp);
        xia_file_close(destfp);
        xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "xiaCopyFile",
               "No memory when loading data to copy: %d (%s)",
               (int) sb.st_size, src);
        return XIA_NOMEM;
    }

    memset(data, 0, (size_t) (sb.st_size + 1));

    objsRead = fread(data, (size_t) sb.st_size, 1, srcfp);
    xia_file_close(srcfp);
    if (objsRead != 1) {
        handel_md_free(data);
        xia_file_close(destfp);
        xiaLog(XIA_LOG_ERROR, XIA_BAD_FILE_READ, "xiaCopyFile",
               "Could not read: %s", src);
        return XIA_BAD_FILE_READ;
    }

	toWrite = (size_t) sb.st_size;

    while (toWrite) {
        written = fwrite(data + haveWritten, 1, toWrite, destfp);

        if (((ssize_t) written) < 0) {
            handel_md_free(data);
            xia_file_close(destfp);
            status = XIA_BAD_FILE_WRITE;
            xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                   "Copying file failed: %s: (%d) %s",
                   dest, errno, strerror(errno));
            return status;
        }

        haveWritten += written;
        toWrite -= written;
    }

    handel_md_free(data);
    xia_file_close(destfp);

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * Routine to read in "handel_ini" type ini files
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaReadIniFile(const char *inifile)
{
    int status = XIA_SUCCESS;
    int numSections;
    int i;
    /*
     * Pointers to keep track of the xia.ini file
     */
    FILE *fp = NULL;

    char newFile[MAXFILENAME_LEN];
    char tmpLine[XIA_LINE_LEN];
    char line[XIA_LINE_LEN];

    fpos_t start;
    fpos_t end;
    fpos_t local;
    fpos_t local_end;

    char xiaini[8] = "xia.ini";

    /* Check if an .INI file was specified */
    if (inifile == NULL)
    {
        inifile = xiaini;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaReadIniFile", "INI file = %s", inifile);

    /* Open the .ini file */
    fp = xiaFindFile(inifile, "rb", newFile);

    if (fp == NULL) {
        status = XIA_OPEN_FILE;
        xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
               "Could not open %s", inifile);
        return status;
    }

    /* Loop over all the sections as defined in sectionInfo */
    /* XXX BUG: Should be sectionInfo / sectionInfo[0]?
     */
    numSections = (int) (sizeof(sectionInfo) / sizeof(SectionInfo));

    for (i = 0; i < numSections; i++)
    {
        if (!sectionInfo[i].multiSection) {
            fpos_t unknownEnd;

            status = xiaFindEntryStart(fp, sectionInfo[i].section, &start);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_WARNING, "xiaReadIniFile",
                       "Section missing from ini file: %s", sectionInfo[i].section);
                continue;
            }

            status = sectionInfo[i].function_ptr(fp, &local, &unknownEnd);

            if (status != XIA_SUCCESS) {
                xia_file_close(fp);
                xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                       "Error loading \"%s\" section from ini file", sectionInfo[i].section);
                return status;
            }

            continue;
        }

        status = xiaFindEntryLimits(fp, sectionInfo[i].section, &start, &end);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_WARNING, "xiaReadIniFile",
                   "Section missing from ini file: %s", sectionInfo[i].section);
            continue;
        }

        /* Here is the psuedocode for parsing in a section w/ multiple headings
         *
         * 1) Set local to line with START on it (this is a one shot thing)
         * 2) Cache line pointed to by "end" (this is because we can't do direct
         *    arithmetic comparisons with fpos_t structs). Also, actually do a
         *    comparison between local's "line" and the end's "line". If they
         *    match then we've reached the end of the section and are finished.
         *    N.b.: I pray to god that a situation never somes up that would
         *    break this comparison. In principle it shouldn't happen since
         *    end's "line" is either EOF or a section heading.
         * 3) Increment local until we run into END
         * 4) Set local_end
         * 5) xiaLoadxxxxx(local, local_end);
         * 6) Set current to local_end and jump to step (2)
         */

        status = fsetpos(fp, &end);

        if (status != 0) {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                   "Error setting file position to the end of the current section."
                   " errno = %d, '%s'.", (int)errno, strerror(errno));
            return XIA_SET_POS;
        }

        status = xiaGetLine(fp, tmpLine);

        if (status != XIA_SUCCESS && status != XIA_EOF) {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                   "Error getting end of section line after setting the file position.");
            return status;
        }

        ASSERT(tmpLine[0] == '[');

        xiaLog(XIA_LOG_DEBUG, "xiaReadIniFile",
               "Cached end string = %s", tmpLine);

        status = fsetpos(fp, &start);

        if (status != 0) {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                   "Error setting file position to the end of the current section."
                   " errno = %d, '%s'.", (int)errno, strerror(errno));
            return XIA_SET_POS;
        }

        status = xiaGetLine(fp, line);

        if (status != XIA_SUCCESS && status != XIA_EOF) {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                   "Error getting start of section line after setting the file position.");
            return status;
        }

        while(!STREQ(line, tmpLine))
        {
            status = fgetpos(fp, &local);

            if (STRNEQ(line, "START"))
            {
                int count = 0;

                do
                {
                    status = fgetpos(fp, &local_end);

                    status = xiaGetLine(fp, line);

                    xiaLog(XIA_LOG_DEBUG, "xiaReadIniFile",
                           "Inside START/END bracket: %s", line);

                    if (++count > 500) {
                        xia_file_close(fp);
                        status = XIA_FILE_RA;
                        xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                               "Error loading information from ini file, no END found");
                        return status;
                    }

                } while (!STRNEQ(line, "END"));

                status = sectionInfo[i].function_ptr(fp, &local, &local_end);

                if (status != XIA_SUCCESS) {
                    xia_file_close(fp);
                    xiaLog(XIA_LOG_ERROR, status, "xiaReadIniFile",
                           "Error loading information from ini file");
                    return status;
                }
            }

            status = xiaGetLine(fp, line);

            if (status == XIA_EOF)
            {
                break;
            }

            xiaLog(XIA_LOG_DEBUG, "xiaReadIniFile",
                   "Looking for START: %s", line);
        }
    }

    xia_file_close(fp);

    return XIA_SUCCESS;
}



/******************************************************************************
 *
 * Routine to open a new file.
 * Try to open the file directly first.
 * Then try to open the file in the directory pointed to
 *     by XIAHOME.
 * Finally try to open the file as an environment variable.
 *
 ******************************************************************************/
FILE* xiaFindFile(const char* filename, const char* mode,
                  char newFile[MAXFILENAME_LEN])
{
    FILE *fp = NULL;
    char *name = NULL, *name2 = NULL;
    char *home = NULL;

    size_t len = 0;


    ASSERT(filename != NULL);

    /* Try to open file directly */
    if ((fp = xia_file_open(filename, mode)) != NULL) {
        len = MAXFILENAME_LEN > (strlen(filename) + 1) ?
            strlen(filename) : MAXFILENAME_LEN;
        strncpy(newFile, filename, len);
        newFile[len] = '\0';
        return fp;
    }
    /* Try to open the file with the path XIAHOME */
    if ((home = getenv("XIAHOME")) != NULL) {
        name =
            (char *) handel_md_alloc(sizeof(char) * (strlen(home) + strlen(filename) + 2));
        sprintf(name, "%s/%s", home, filename);
        if ((fp = xia_file_open(name, mode)) != NULL) {
            len = MAXFILENAME_LEN > (strlen(name) + 1) ? strlen(name) : MAXFILENAME_LEN;
            strncpy(newFile, name, len);
            newFile[len] = '\0';
            handel_md_free(name);
            return fp;
        }
        handel_md_free(name);
        name = NULL;
    }
    /* Try to open the file with the path DXPHOME */
    if ((home = getenv("DXPHOME")) != NULL) {
        name =
            (char *) handel_md_alloc(sizeof(char) * (strlen(home) + strlen(filename) + 2));
        sprintf(name, "%s/%s", home, filename);
        if ((fp = xia_file_open(name, mode)) != NULL){
            len = MAXFILENAME_LEN > (strlen(name) + 1) ? strlen(name) : MAXFILENAME_LEN;
            strncpy(newFile, name, len);
            newFile[len] = '\0';
            handel_md_free(name);
            return fp;
        }
        handel_md_free(name);
        name = NULL;
    }
    /* Try to open the file as an environment variable */
    if ((name = getenv(filename)) != NULL) {
        if((fp = xia_file_open(name, mode)) != NULL){
            len = MAXFILENAME_LEN > (strlen(name) + 1) ? strlen(name) : MAXFILENAME_LEN;
            strncpy(newFile, name, len);
            newFile[len] = '\0';
            return fp;
        }
        name = NULL;
    }
    /* Try to open the file with the path XIAHOME and pointing
     * to a file as an environment variable */
    if ((home = getenv("XIAHOME")) != NULL) {
        if ((name2 = getenv(filename)) != NULL) {
            name =
                (char *) handel_md_alloc(sizeof(char) * (strlen(home) + strlen(name2) + 2));
            sprintf(name, "%s/%s", home, name2);
            if((fp = xia_file_open(name, mode)) != NULL) {
                len = MAXFILENAME_LEN > (strlen(name) + 1) ? strlen(name) : MAXFILENAME_LEN;
                strncpy(newFile, name, len);
                newFile[len] = '\0';
                handel_md_free(name);
                return fp;
            }
            handel_md_free(name);
            name = NULL;
        }
    }
    /* Try to open the file with the path DXPHOME and pointing
     * to a file as an environment variable */
    if ((home = getenv("DXPHOME")) != NULL) {
        if ((name2 = getenv(filename)) != NULL) {
            name =
                (char *) handel_md_alloc(sizeof(char) * (strlen(home) + strlen(name2) + 2));
            sprintf(name, "%s/%s", home, name2);
            if((fp = xia_file_open(name, mode)) != NULL) {
                len = MAXFILENAME_LEN > (strlen(name) + 1) ? strlen(name) : MAXFILENAME_LEN;
                strncpy(newFile, name, len);
                newFile[len] = '\0';
                handel_md_free(name);
                return fp;
            }
            handel_md_free(name);
            name = NULL;
        }
    }

    return NULL;
}



/******************************************************************************
 *
 * Routine that gets the first line with text after the current file
 * position.
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaGetLineData(const char *line,
                                            char *name, char *value)
{
    int status = XIA_SUCCESS;

    size_t loc;
    size_t begin;
    size_t end;
    size_t len;


    /* If this line is a comment then skip it.
     * See BUG ID #64.
     */
    if (line[0] == '*') {
        strcpy(name, "COMMENT");
        strcpy(value, line);
        return XIA_SUCCESS;
    }

    /* Start by finding the '=' within the line */
    loc = strcspn(line, "=");
    if (loc == 0 || loc == strlen(line))
    {
        status = XIA_FORMAT_ERROR;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetLineData",
               "No = present in xia.ini line: \n %s", line);
        return status;
    }

    /* Strip all the leading blanks */
    begin = 0;
    while (isspace(CTYPE_CHAR(line[begin])))
    {
        begin++;
    }

    /* Strip all the trailing blanks */
    end = loc - 1;
    while (isspace(CTYPE_CHAR(line[end])))
    {
        end--;
    }

    /* Bug #76, prevents a bad core dump */
    if (begin > end)
    {
        status = XIA_FORMAT_ERROR;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetLineData",
               "Invalid name found in line:  %s", line);
        return status;
    }

    /* copy the name */
    len = end - begin + 1;
    strncpy(name, line + begin, len);
    name[len] = '\0';

    /* Strip all the leading blanks */
    begin = loc + 1;

    while (isspace(CTYPE_CHAR(line[begin]))) {
        begin++;
    }

    /* Strip all the trailing blanks */
    end = strlen(line) - 1;

    while (isspace(CTYPE_CHAR(line[end]))) {
        end--;
    }

    /* Bug #76, prevents a bad core dump */
    if (begin > end)
    {
        status = XIA_FORMAT_ERROR;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetLineData",
               "Invalid value found in line:  %s", line);
        return status;
    }

    /* copy the value */
    len = end - begin + 1;
    strncpy(value, line + begin, len);
    value[len] = '\0';

    /* Convert name to lower case */
    /*
      for (j = 0; j < (unsigned int)strlen(name); j++)
      {
      name[j] = (char) tolower(name[j]);
      }

    */

    return status;
}

HANDEL_STATIC int HANDEL_API xiaGetLine(FILE *fp, char *lline)
{
    return xiaGetLine_N(fp, lline, XIA_LINE_LEN);
}

/*
 * Gets the first line with text after the current file position.
 *
 * If the line is longer than llen, the file position is scanned to
 * the start of the next line.
 */
HANDEL_STATIC int HANDEL_API xiaGetLine_N(FILE *fp, char *lline, int llen)
{
    unsigned int j;

    char *cstatus;

    do {
        size_t len;
        char extra[8193];

        cstatus = handel_md_fgets(lline, llen, fp);

        /* lline won't be overwritten in this case */
        if (cstatus == NULL) {
            return XIA_EOF;
        }

        /* If a partial line was read, flush the rest of that line so the next read
         * gets a new line.
         */
        while ((len = strlen(cstatus)) > 0 &&
               cstatus[len - 1] != '\r' && cstatus[len - 1] != '\n') {
            cstatus = handel_md_fgets(extra, sizeof(extra) - 1, fp);

            if (!cstatus) {
                break;
            }
        }

        len = strlen(lline);

        /*
         * Remove the new line character to keep the log file output from
         * containing the extra white space.
         */
        if (len > 1 && lline[len - 2] == '\n')
            lline[len - 2] = '\0';
        if (len > 0 && lline[len - 1] == '\n')
            lline[len - 1] = '\0';

        /* Check for any alphanumeric character in the line */
        for (j = 0; j < (unsigned int)len; j++) {
            if (isgraph(CTYPE_CHAR(lline[j]))) {
                return XIA_SUCCESS;
            }
        }
    } while (cstatus != NULL);

    return XIA_EOF;
}

/*
 * Searches thru the .ini file and finds the start of a specific
 * section starting at [section]. The fp is left on the first line
 * after the section tag.
 */
HANDEL_STATIC int HANDEL_API xiaFindEntryStart(FILE *fp, const char *section, fpos_t *start)
{
    int status = XIA_SUCCESS;
    unsigned int j;

    boolean_t isStartFound = FALSE_;

    char *cstatus;

    char line[XIA_LINE_LEN];

    /* First rewind the file to the beginning */
    rewind(fp);

    /* Now find the match to the section entry */
    do {
        do {
            cstatus = handel_md_fgets(line, XIA_LINE_LEN, fp);

        } while ((line[0]!='[') &&
                 (cstatus!=NULL));

        if (cstatus == NULL)
        {
            status = XIA_NOSECTION;
            /* This isn't an error since the user has the option of specifying
             * the missing information using the dynamic configuration
             * routines.
             */
            xiaLog(XIA_LOG_WARNING, "xiaFindEntryLimits",
                   "Unable to find section %s", section);
            return status;
        }

        /* Find the terminating ] to this section */
        for (j = 1; j < (strlen(line) + 1); j++)
        {
            if (line[j]==']')
            {
                break;
            }
        }
        /* Did we not find the terminating ]? */
        if (j == (unsigned int)strlen(line))
        {
            status = XIA_FORMAT_ERROR;
            xiaLog(XIA_LOG_ERROR, status, "xiaFindEntryLimits",
                   "Syntax error in Init file, no terminating ] found");
            return status;
        }

        if (strncmp(line + 1, section, j - 1) == 0)
        {
            /* Recorcd the starting position) */
            fgetpos(fp, start);

            isStartFound = TRUE_;
        }
        /* Else look for the next section entry */
    } while (!isStartFound);

    return XIA_SUCCESS;
}

/*
 * Searches thru the .ini file and finds the start and end of a
 * specific section starting at [section] and ending at the
 * next [].
 */
HANDEL_STATIC int HANDEL_API xiaFindEntryLimits(FILE *fp, const char *section,
                                                fpos_t *start, fpos_t *end)
{
    int status = XIA_SUCCESS;

    char *cstatus;

    char line[XIA_LINE_LEN];

    status = xiaFindEntryStart(fp, section, start);
    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaFindEntryLimits",
               "Finding \"%s\" section start.", section);
        return status;
    }

    do
    {
        /* Get the current file position before the next read.  If the file
         * ends or we find a '[' then we are done and want to sent the ending
         * position to the location of the previous read
         */
        status = fgetpos(fp, end);

        cstatus = handel_md_fgets(line, XIA_LINE_LEN, fp);

    } while ((line[0] != '[') &&
             (cstatus != NULL));

    if (!cstatus) {
        ASSERT(feof(fp));
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as
 * detector information. If it fails, then it fails hard and the user needs
 * to fix their inifile.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadDetector(FILE *fp, fpos_t *start, fpos_t *end)
{
    int status;

    unsigned short i;
    unsigned short numChans;

    double gain;
    double typeValue;

    char value[MAXITEM_LEN];
    char alias[MAXALIAS_LEN];
    char name[MAXITEM_LEN];

    /* We need to load things in a certain order since some information must be
     * specified to HanDeL before others. The following order should work well:
     * 1) alias
     * 2) number of channels
     * 3) rest of the detector information
     */

    status = xiaFileRA(fp, start, end, "alias", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Unable to load alias information");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaLoadDetector",
           "alias = %s", value);

    strcpy(alias, value);

    status = xiaNewDetector(alias);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Error creating new detector");
        return status;
    }

    status = xiaFileRA(fp, start, end, "number_of_channels", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Unable to find number_of_channels");
        return status;
    }

    sscanf(value, "%hu", &numChans);

    xiaLog(XIA_LOG_DEBUG, "xiaLoadDetector",
           "number_of_channels = %hu", numChans);

    status = xiaAddDetectorItem(alias, "number_of_channels", (void *)&numChans);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Error adding number_of_channels to detector %s", alias);
        return status;
    }

    status = xiaFileRA(fp, start, end, "type", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Unable to find type for detector %s", alias);
        return status;
    }

    status = xiaAddDetectorItem(alias, "type", (void *)value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Error adding type to detector %s", alias);
        return status;
    }

    status = xiaFileRA(fp, start, end, "type_value", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Unable to find type_value for detector %s", alias);
        return status;
    }

    sscanf(value, "%lf", &typeValue);

    status = xiaAddDetectorItem(alias, "type_value", (void *)&typeValue);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
               "Error adding type_value to detector %s", alias);
        return status;
    }

    for (i = 0; i < numChans; i++)
    {
        sprintf(name, "channel%hu_gain", i);
        status = xiaFileRA(fp, start, end, name, value);

        if (status == XIA_FILE_RA)
        {
            xiaLog(XIA_LOG_WARNING, "xiaLoadDetector",
                   "Current configuration file missing %s", name);

        } else {

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
                       "Unable to load channel gain");
                return status;
            }

            sscanf(value, "%6lf", &gain);

            xiaLog(XIA_LOG_DEBUG, "xiaLoadDetector",
                   "%s = %f", name, gain);

            status = xiaAddDetectorItem(alias, name, (void *)&gain);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
                       "Error adding %s to detector %s", name, alias);
                return status;
            }
        }

        sprintf(name, "channel%hu_polarity", i);
        status = xiaFileRA(fp, start, end, name, value);

        if (status == XIA_FILE_RA)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
                   "Current configuration file missing %s", name);

        } else {

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
                       "Unable to load channel polarity");
                return status;
            }

            /*sscanf(value, "%s", polarity);*/

            xiaLog(XIA_LOG_DEBUG, "xiaLoadDetector",
                   "%s = %s", name, value);

            status = xiaAddDetectorItem(alias, name, (void *)value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadDetector",
                       "Error adding %s to detector %s", name, alias);
                return status;
            }
        }
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as
 * module information. If it fails, then it fails hard and the user needs
 * to fix their inifile.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadModule(FILE *fp, fpos_t *start, fpos_t *end)
{
    int status;
    int chanAlias;

    unsigned int numChans;
    unsigned int i;

    char value[MAXITEM_LEN];
    char alias[MAXALIAS_LEN];
    char iface[MAXITEM_LEN];
    char moduleType[MAXITEM_LEN];
    char name[MAXITEM_LEN];
    char detAlias[MAXALIAS_LEN];
    char firmAlias[MAXALIAS_LEN];
    char defAlias[MAXALIAS_LEN];

    ASSERT(fp);
    ASSERT(start);
    ASSERT(end);


    status = xiaFileRA(fp, start, end, "alias", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Unable to load alias information");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
           "alias = %s", value);

    strcpy(alias, value);

    status = xiaNewModule(alias);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Error creating new module");
        return status;
    }

    status = xiaFileRA(fp, start, end, "module_type", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Unable to load module type");
        return status;
    }

    sscanf(value, "%s", moduleType);

    xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
           "moduleType = %s", moduleType);

    status = xiaAddModuleItem(alias, "module_type", (void *)moduleType);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Error adding module type to module %s", alias);
    }

    status = xiaFileRA(fp, start, end, "number_of_channels", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Unable to load number of channels");
        return status;
    }

    sscanf(value, "%u", &numChans);

    xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
           "number_of_channels = %u", numChans);

    status = xiaAddModuleItem(alias, "number_of_channels", (void *)&numChans);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Error adding number_of_channels to module %s", alias);
        return status;
    }

    /* Deal with interface here */
    status = xiaFileRA(fp, start, end, "interface", value);

    sscanf(value, "%s", iface);

    xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
           "interface = %s", iface);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Unable to load interface");
        return status;
    }


    if (STREQ(iface, "inet")) {
        char address[MAXITEM_LEN];
        unsigned int port;
        unsigned int timeout;

        status = xiaAddModuleItem(alias, "interface", iface);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding '%s' interface to module '%s'.", iface, alias);
            return status;
        }

        status = xiaFileRA(fp, start, end, "inet_address", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Unable to load INET address");
            return status;
        }

        sscanf(value, "%s", address);

        xiaLog(XIA_LOG_DEBUG, "xiaLoadModule", "INET address = %s", address);

        status = xiaAddModuleItem(alias, "inet_address", address);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding INET address to module %s", alias);
            return status;
        }

        status = xiaFileRA(fp, start, end, "inet_port", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Unable to load INET port");
            return status;
        }

        sscanf(value, "%u", &port);

        xiaLog(XIA_LOG_DEBUG, "xiaLoadModule", "INET port = %d", port);

        status = xiaAddModuleItem(alias, "inet_port", &port);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding INET port to module %s", alias);
            return status;
        }

        status = xiaFileRA(fp, start, end, "inet_timeout", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Unable to load INET timeout");
            return status;
        }

        sscanf(value, "%u", &timeout);

        xiaLog(XIA_LOG_DEBUG, "xiaLoadModule", "INET timeout = %d", timeout);

        status = xiaAddModuleItem(alias, "inet_timeout", &timeout);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding INET timeout to module %s", alias);
            return status;
        }
    }
    else {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
               "Unknown interface '%s' for module '%s'.", iface, alias);
        return XIA_BAD_INTERFACE;
    }

    for (i = 0; i < numChans; i++) {
        sprintf(name, "channel%u_alias", i);
        status = xiaFileRA(fp, start, end, name, value);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Unable to load %s from %s", name, alias);
            return status;
        }

        sscanf(value, "%d", &chanAlias);

        xiaLog(XIA_LOG_DEBUG, "xiaLoadDetector",
               "%s = %d", name, chanAlias);

        status = xiaAddModuleItem(alias, name, (void *)&chanAlias);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding %s to module %s", name, alias);
            return status;
        }

        sprintf(name, "channel%u_detector", i);
        status = xiaFileRA(fp, start, end, name, value);

        if (status == XIA_FILE_RA) {
            xiaLog(XIA_LOG_WARNING, "xiaLoadModule",
                   "Current configuration file missing %s", name);

        } else {

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                       "Unable to load channel detector alias");
                return status;
            }

            sscanf(value, "%s", detAlias);

            xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
                   "%s = %s", name, detAlias);

            status = xiaAddModuleItem(alias, name, (void *)detAlias);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                       "Error adding %s to module %s", name, alias);
                return status;
            }
        }
    }

    /* Need a little extra logic to determine how to load the firmware
     * and defaults. Check for *_all first and if that isn't found then
     * try and find ones for individual channels.
     */
    status = xiaFileRA(fp, start, end, "firmware_set_all", value);

    if (status != XIA_SUCCESS)
    {
        for (i = 0; i < numChans; i++)
        {
            sprintf(name, "firmware_set_chan%u", i);
            status = xiaFileRA(fp, start, end, name, value);

            if (status == XIA_FILE_RA)
            {
                xiaLog(XIA_LOG_WARNING, "xiaLoadModule",
                       "Current configuration file missing %s", name);

            } else {
                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                           "Unable to load channel firmware information");
                    return status;
                }

                strcpy(firmAlias, value);

                xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
                       "%s = %s", name, firmAlias);

                status = xiaAddModuleItem(alias, name, (void *)firmAlias);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                           "Error adding %s to module %s", name, alias);
                    return status;
                }
            }
        }

    } else {

        strcpy(firmAlias, value);

        status = xiaAddModuleItem(alias, "firmware_set_all", (void *)firmAlias);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding firmware_set_all to module %s", alias);
            return status;
        }
    }

    status = xiaFileRA(fp, start, end, "default_all", value);

    if (status != XIA_SUCCESS)
    {
        for (i = 0; i < numChans; i++)
        {
            sprintf(name, "default_chan%u", i);
            status = xiaFileRA(fp, start, end, name, value);

            if (status == XIA_FILE_RA)
            {
                xiaLog(XIA_LOG_INFO, "xiaLoadModule",
                       "Current configuration file missing %s", name);

            } else {

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                           "Unable to load channel default information");
                    return status;
                }

                strcpy(defAlias, value);

                xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
                       "%s = %s", name, defAlias);

                status = xiaAddModuleItem(alias, name, (void *)defAlias);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                           "Error adding %s to module %s", name, alias);
                    return status;
                }
            }
        }

    } else {

        strcpy(defAlias, value);

        status = xiaAddModuleItem(alias, "default_all", (void *)defAlias);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadModule",
                   "Error adding default_all to module %s", alias);
            return status;
        }
    }

    return XIA_SUCCESS;
}


HANDEL_STATIC int xiaLoadModChanData(FILE *fp, fpos_t *start, fpos_t *end)
{
    int status;

    UNUSED(start);
    UNUSED(end);

    do {
        status = xiaReadChanData(fp);
    } while (status == XIA_SUCCESS);

    if (status == XIA_EOF) {
        return XIA_SUCCESS;
    }

    return status;
}


/*
 * Search the section for module channel data starting with prefix,
 * e.g. "data_all" or "data_chan0". If both the length and data are
 * found, the data is parsed and added to the module. Otherwise
 * XIA_FILE_RA is returned.
 */
HANDEL_STATIC int xiaReadChanData(FILE *fp)
{
    char line[XIA_LINE_LEN];

    char name[MAXITEM_LEN];
    char value[MAXITEM_LEN];
    char prefix[MAXITEM_LEN];
    char dataKey[MAXITEM_LEN];

    int match;
    char alias[MAXALIAS_LEN];
    unsigned int ch;
    int status;

    size_t dataEncLen;
    char *dataEnc;

    int decStatus;
    size_t dataDecLen;
    byte_t *dataDec;

    int uncmpStatus;
    uLong uncmpLen;
    GenBuffer buf;

    /* Loop over the module sections: START module1. */
    do {
        /* Get the next module alias. */
        status = xiaGetLine(fp, line);
        if (status != XIA_SUCCESS) {
            if (status != XIA_EOF) {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "Finding channel data.");
            }
            return status;
        }

        /*
         * If we hit another section, treat it like EOF for the
         * purposes of this section parser.
         */
        if (line[0] == '[') {
            return XIA_EOF;
        }

        if (!STRNEQ(line, "START ")) {
            status = XIA_FILE_RA;
            xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                   "Expected module name: %.40s", line);
            return status;
        }

        strncpy(alias, line + strlen("START "), MAXALIAS_LEN);

        xiaLog(XIA_LOG_DEBUG, "xiaReadChanData",
               "Channel data for %s.", alias);

        do {
            /* Get the next channel's data length. */
            status = xiaGetLine(fp, line);
            if (status != XIA_SUCCESS) {
                if (status != XIA_EOF) {
                    xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                           "Finding channel data.");
                }
                return status;
            }

            /*
             * If we hit the end of the section, we're done with one module.
             */
            if (STRNEQ(line, "END ")) {
                break;
            }

            /* Read the channel and length. */
            status = xiaGetLineData(line, name, value);
            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "Finding channel data length: %.40s", line);
                return status;
            }

            /* Handle data_all or data_chanN. */
            if (STRNEQ(line, "data_all_len")) {
                sprintf(prefix, "data_all");
            }
            else {
                /* Parse the channel from the name. */
                match = sscanf(name, "data_chan%u_len", &ch);

                if (match != 1) {
                    status = XIA_FILE_RA;
                    xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                           "Finding channel number in %s, %d matches", name, match);
                    return status;
                }

                sprintf(prefix, "data_chan%u", ch);
            }

            xiaLog(XIA_LOG_DEBUG, "xiaLoadModule",
                   "%s = %s", name, value);

            /* Parse the length from the value. */
            sscanf(value, "%zu", &dataEncLen);

            /*
             * The next line should be the data for the same key. Scan the key, leaving
             * the fp at the data.
             */
            match = fscanf(fp, "%200[^= ]%*10[= ]", name);
            if (match != 1 || !STREQ(prefix, name)) {
                status = XIA_FILE_RA;
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "Expected %s, got %s, %d matches", dataKey, name, match);
                return status;
            }

            /* Now use the length to allocate and slurp the data. */
            dataEnc = handel_md_alloc((dataEncLen + 1) * sizeof(char));
            if (dataEnc == NULL) {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "No memory for %s %s", alias, prefix);
                return status;
            }

            status = xiaGetLine_N(fp, dataEnc, (int)dataEncLen + 1);
            if (status != XIA_SUCCESS) {
                handel_md_free(dataEnc);
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "Unable to load %s %s", alias, prefix);
                return status;
            }

            /* Decode. Base64 deflates by 3/4. Add one for the null terminator. */
            dataDecLen = dataEncLen * 3 / 4 + 1;
            dataDec = handel_md_alloc(dataDecLen);
            if (dataDec == NULL) {
                handel_md_free(dataEnc);
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addData",
                       "Unable to allocate memory to decode chosen->data[i]");
                return status;
            }

            decStatus = Base64Decode(dataEnc, dataEncLen, dataDec, &dataDecLen);
            if (decStatus) {
                handel_md_free(dataEnc);
                handel_md_free(dataDec);
                status = XIA_DECODE;
                xiaLog(XIA_LOG_ERROR, status, "_addData",
                       "Unable to decode %s %s. Decode status=%d.", alias, name, decStatus);
                return status;
            }

            handel_md_free(dataEnc);

            uncmpLen = (uLong)dataDecLen * 32; /* Conservative estimate 32x deflate */
            buf.data = handel_md_alloc(uncmpLen);
            if (buf.data == NULL) {
                handel_md_free(dataDec);
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "_addData",
                       "Unable to allocate memory for chosen->data[i]");
                return status;
            }

            memset(buf.data, 0, uncmpLen);

            uncmpStatus = uncompress(buf.data, &uncmpLen, dataDec, (uLong)dataDecLen);
            if (uncmpStatus != Z_OK) {
                handel_md_free(dataDec);
                handel_md_free(buf.data);
                status = XIA_DECODE;
                xiaLog(XIA_LOG_ERROR, status, "_addData",
                       "Unable to uncompress %s %s. Uncompress status=%d.", alias, name, uncmpStatus);
                return status;
            }

            handel_md_free(dataDec);

            buf.length = uncmpLen + 1;

            status = xiaAddModuleItem(alias, prefix, &buf);

            handel_md_free(buf.data);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadChanData",
                       "Error adding module %s %s", alias, prefix);
                return status;
            }
        } while (status == XIA_SUCCESS); /* Channels in a module */
    } while (status == XIA_SUCCESS); /* Module sections */

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine parses data in from fp (and bounded by start & end) as
 * firmware information. If it fails, then it fails hard and the user needs
 * to fix their inifile.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadFirmware(FILE *fp, fpos_t *start, fpos_t *end)
{
    int status;

    unsigned short i;
    unsigned short numKeywords;

    char value[MAXITEM_LEN];
    char alias[MAXALIAS_LEN];
    char file[MAXFILENAME_LEN];
    char mmu[MAXFILENAME_LEN];
    char path[MAXITEM_LEN];
    /* k-e-y-w-o-r-d + (possibly) 2 numeric digits + \0
     * N.b. Better not be more then 100 keywords
     */
    char keyword[10];

    status = xiaFileRA(fp, start, end, "alias", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
               "Unable to load alias information");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaLoadFirmware",
           "alias = %s", value);

    strcpy(alias, value);

    status = xiaNewFirmware(alias);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
               "Error creating new firmware");
        return status;
    }

    /* Check for an MMU first since we'll be exiting if we find a filename */
    status = xiaFileRA(fp, start, end, "mmu", value);

    if (status == XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_DEBUG, "xiaLoadFirmware",
               "mmu = %s", value);

        strcpy(mmu, value);
        status = xiaAddFirmwareItem(alias, "mmu", (void *)mmu);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
                   "Error adding MMU to alias %s", alias);
            return status;
        }
    }

    /* If we find a filename, then we are done and can return */
    status = xiaFileRA(fp, start, end, "filename", value);

    if (status == XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_DEBUG, "xiaLoadFirmware",
               "filename = %s", value);

        strcpy(file, value);
        status = xiaAddFirmwareItem(alias, "filename", (void *)file);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
                   "Error adding filename to alias %s", alias);
            return status;
        }

        status = xiaFileRA(fp, start, end, "fdd_tmp_path", value);

        if (status == XIA_SUCCESS) {
            strcpy(path, value);

            status = xiaAddFirmwareItem(alias, "fdd_tmp_path", (void *)path);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
                       "Error adding FDD temporary path to '%s'", alias);
                return status;
            }
        }

        /* Check for keywords, if any...no need to really warn since the most
         * important "keywords" are generated by Handel.
         */
        status = xiaFileRA(fp, start, end, "num_keywords", value);

        if (status == XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_DEBUG, "xiaLoadFirmware",
                   "num_keywords = %s", value);

            sscanf(value, "%hu", &numKeywords);
            for (i = 0; i < numKeywords; i++)
            {
                sprintf(keyword, "keyword%hu", i);
                status = xiaFileRA(fp, start, end, keyword, value);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
                           "Unable to load keyword");
                    return status;
                }

                xiaLog(XIA_LOG_DEBUG, "xiaLoadFirmware",
                       "%s = %s", keyword, value);

                status = xiaAddFirmwareItem(alias, "keyword", (void *)keyword);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
                           "Error adding keyword, %s, to alias %s", keyword, alias);
                    return status;
                }
            }
        }
        /* Don't even bother trying to parse in more information */
        return XIA_SUCCESS;
    }

    /* Need to be a little careful here about how we parse in the PTRR chunks.
     * Start slowly by getting the number of PTRRs first.
     */
    status = xiaReadPTRRs(fp, start, end, alias);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadFirmware",
               "Error loading PTRR information for alias %s", alias);
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine parses in the information specified in the defaults
 * definitions.
 *
 *****************************************************************************/
HANDEL_STATIC int xiaLoadDefaults(FILE *fp, fpos_t *start, fpos_t *end)
{
    int status;

    char value[MAXITEM_LEN];
    char alias[MAXALIAS_LEN];
    char tmpName[MAXITEM_LEN];
    char tmpValue[MAXITEM_LEN];
    char endLine[XIA_LINE_LEN];
    char line[XIA_LINE_LEN];

    double defValue;

    fpos_t dataStart;

    status = xiaFileRA(fp, start, end, "alias", value);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDefaults",
               "Unable to load alias information");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaLoadDefaults",
           "alias = %s", value);

    strcpy(alias, value);

    status = xiaNewDefault(alias);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaLoadDefaults",
               "Error creating new default");
        return status;
    }

    /* Want a position after the alias line so that we can just read in line-
     * by-line until we reach endLine
     */
    xiaSetPosOnNext(fp, start, end, "alias", &dataStart, TRUE_);

    fsetpos(fp, end);
    status = xiaGetLine(fp, endLine);

    fsetpos(fp, &dataStart);
    status = xiaGetLine(fp, line);

    while (!STREQ(line, endLine))
    {
        status = xiaGetLineData(line, tmpName, tmpValue);
        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadDefaults",
                   "Error getting data for entry %s", tmpName);
            return status;
        }

        sscanf(tmpValue, "%lf", &defValue);

        status = xiaAddDefaultItem(alias, tmpName, (void *)&defValue);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaLoadDefaults",
                   "Error adding %s (value = %.3f) to alias %s",
                   tmpName, defValue, alias);
            return status;
        }


        xiaLog(XIA_LOG_DEBUG, "xiaLoadDefaults",
               "Added %s (value = %.3f) to alias %s",
               tmpName, defValue, alias);

        status = xiaGetLine(fp, line);
    }

    return XIA_SUCCESS;
}




/*****************************************************************************
 *
 * This routine will read in several PTRRs(*) and add them to the Firmware
 * indicated by alias.
 *
 * (*) -- Actually, it will read in the number specified by number_of_ptrrs.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaReadPTRRs(FILE *fp, fpos_t *start,
                                          fpos_t *end, char *alias)
{
    int status;

    unsigned short i;
    unsigned short ptrr;
    unsigned short numFilter;
    unsigned short filterInfo;

    double min_peaking_time;
    double max_peaking_time;

    /* f+i+l+t+e+r+_+i+n+f+o+ 2 digits + \0 */
    char filterName[14];
    char value[MAXITEM_LEN];

    fpos_t newStart;
    fpos_t newEnd;
    fpos_t lookAheadStart;

    boolean_t isLast = FALSE_;


    xiaLog(XIA_LOG_DEBUG, "xiaReadPTRRs",
           "Starting parse of PTRRs");

    /* This assumes that there is at least one PTRR for a specified alias */
    newEnd = *start;
    while (!isLast)
    {
        xiaSetPosOnNext(fp, &newEnd, end, "ptrr", &lookAheadStart, TRUE_);
        xiaSetPosOnNext(fp, &newEnd, end, "ptrr", &newStart, FALSE_);

        /* Find the end here: either the END or another ptrr */
        status = xiaSetPosOnNext(fp, &lookAheadStart, end, "ptrr", &newEnd, FALSE_);

        if (status == XIA_END)
        {
            isLast = TRUE_;
        }

        /* Do the actual actions here */
        status = xiaFileRA(fp, &newStart, &newEnd, "ptrr", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read ptrr from file");
            return status;
        }

        sscanf(value, "%hu", &ptrr);

        xiaLog(XIA_LOG_DEBUG, "xiaReadPTRRs",
               "ptrr = %hu", ptrr);

        status = xiaAddFirmwareItem(alias, "ptrr", (void *)&ptrr);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Error adding ptrr to alias %s", alias);
            return status;
        }

        status = xiaFileRA(fp, &newStart, &newEnd, "min_peaking_time", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read min_peaking_time from ptrr = %hu", ptrr);
            return status;
        }

        sscanf(value, "%lf", &min_peaking_time);

        status = xiaAddFirmwareItem(alias, "min_peaking_time", (void *)&min_peaking_time);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Error adding min_peaking_time to alias %s", alias);
            return status;
        }

        status = xiaFileRA(fp, &newStart, &newEnd, "max_peaking_time", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read max_peaking_time from ptrr = %hu", ptrr);
            return status;
        }

        sscanf(value, "%lf", &max_peaking_time);

        status = xiaAddFirmwareItem(alias, "max_peaking_time", (void *)&max_peaking_time);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Error adding max_peaking_time to alias %s", alias);
            return status;
        }

        status = xiaFileRA(fp, &newStart, &newEnd, "fippi", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read fippi from ptrr = %hu", ptrr);
            return status;
        }

        status = xiaAddFirmwareItem(alias, "fippi", (void *)value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Error adding fippi to alias %s", alias);
            return status;
        }

        status = xiaFileRA(fp, &newStart, &newEnd, "dsp", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read dsp from ptrr = %hu", ptrr);
            return status;
        }

        status = xiaAddFirmwareItem(alias, "dsp", (void *)value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Error adding dsp to alias %s", alias);
            return status;
        }

        /* Check for the quite optional "user_fippi"... */
        status = xiaFileRA(fp, &newStart, &newEnd, "user_fippi", value);

        if (status == XIA_SUCCESS)
        {
            status = xiaAddFirmwareItem(alias, "user_fippi", (void *)value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                       "Error adding user_fippi to alias %s", alias);
                return status;
            }

        } else if (status == XIA_FILE_RA) {

            xiaLog(XIA_LOG_INFO, "xiaReadPTRRs",
                   "No user_fippi present in .ini file");

        } else {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read user_fippi from ptrr = %hu", ptrr);
            return status;
        }

        status = xiaFileRA(fp, &newStart, &newEnd, "num_filter", value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                   "Unable to read num_filter from ptrr = %hu", ptrr);
            return status;
        }

        sscanf(value, "%hu", &numFilter);

        xiaLog(XIA_LOG_DEBUG, "xiaReadPTRRs",
               "numFilter = %hu", numFilter);

        for (i = 0; i < numFilter; i++)
        {
            sprintf(filterName, "filter_info%hu", i);
            status = xiaFileRA(fp, &newStart, &newEnd, filterName, value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                       "Unable to read %s from ptrr = %hu", filterName, ptrr);
                return status;
            }

            sscanf(value, "%hu", &filterInfo);

            xiaLog(XIA_LOG_DEBUG, "xiaReadPTRRs",
                   "filterInfo = %hu", filterInfo);

            status = xiaAddFirmwareItem(alias, "filter_info", (void *)&filterInfo);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaReadPTRRs",
                       "Error adding filter_info to alias %s", alias);
                return status;
            }
        }

    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine searches between start and end for name. If it finds a name,
 * it sets newPos to that location. If not, it returns a value of XIA_END
 * and sets newPos to end. There are a couple of important caveats here:
 * You can't use fpos_t pointers in direct arithmetic comparisons. They just
 * don't work that way. Instead, I set the file to the position and then read
 * a line from the file. This line serves as the "unique" identifier from
 * which all future comparisons are done. There is a finite probability that
 * the same string may appear elsewhere in the file. Hopefully the same string
 * won't appear twice between start and end.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetPosOnNext(FILE *fp, fpos_t *start, fpos_t *end,
                                             const char *name, fpos_t *newPos,
                                             boolean_t after)
{
    int status;

    char endLine[XIA_LINE_LEN];
    char line[XIA_LINE_LEN];
    char tmpValue[XIA_LINE_LEN];
    char tmpName[XIA_LINE_LEN];

    fsetpos(fp, end);
    status = xiaGetLine(fp, endLine);

    fsetpos(fp, start);
    fgetpos(fp, newPos);
    status = xiaGetLine(fp, line);

    xiaLog(XIA_LOG_DEBUG, "xiaSetPosOnNext",
           "endLine: %s", endLine);
    xiaLog(XIA_LOG_DEBUG, "xiaSetPosOnNext",
           "startLine: %s", line);

    while (!STREQ(line, endLine))
    {
        status = xiaGetLineData(line, tmpName, tmpValue);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaSetPosOnNext",
                   "Error trying to find %s", name);
            return status;
        }


        if (STREQ(name, tmpName))
        {
            if (after)
            {
                fgetpos(fp, newPos);
            }

            fsetpos(fp, newPos);
            status = xiaGetLine(fp, line);

            xiaLog(XIA_LOG_DEBUG, "xiaSetPosOnNext",
                   "newPos set to line: %s", line);

            return XIA_SUCCESS;
        }

        fgetpos(fp, newPos);

        status = xiaGetLine(fp, line);
    }

    /* Okay, we must have made it to the end of the file */
    return XIA_END;
}


/*****************************************************************************
 *
 * This routine will attempt to find the value from the specified name-value
 * pair. Returns XIA_FILE_RA if it couldn't find anything. It calls
 * xiaGetLine() until the name is matched or the end condition is satisfied.
 *
 *****************************************************************************/
int HANDEL_API xiaFileRA(FILE *fp, fpos_t *start, fpos_t *end,
                         const char *name, char *value)
{
    int status;

    /* Use this to hold the "value" of value until we're sure that this is the
     * right name and we can return.
     */
    char tmpValue[XIA_LINE_LEN];
    char tmpName[XIA_LINE_LEN];
    char line[XIA_LINE_LEN];
    char endLine[XIA_LINE_LEN];


    fsetpos(fp, end);
    status = xiaGetLine(fp, endLine);

    fsetpos(fp, start);
    status = xiaGetLine(fp, line);

    while (!STREQ(line, endLine))
    {
        status = xiaGetLineData(line, tmpName, tmpValue);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaFileRA",
                   "Error trying to find value for %s", name);
            return status;
        }

        if (STREQ(name, tmpName))
        {
            strcpy(value, tmpValue);

            return XIA_SUCCESS;
        }

        status = xiaGetLine(fp, line);
    }

    return XIA_FILE_RA;
}


/** @brief Writes the interface portion of the module configuration to the
 * .ini file.
 */
static int writeInterface(FILE *fp, Module *module)
{
  int i;
  int status = XIA_SUCCESS;


  ASSERT(fp != NULL);
  ASSERT(module != NULL);


  for (i = 0; i < (int) N_ELEMS(INTERFACE_WRITERS); i++) {
    if (module->interface_->type == INTERFACE_WRITERS[i].type) {

      status = INTERFACE_WRITERS[i].fn(fp, module);

      if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "writeInterface",
               "Error writing interface data for type '%u'",
               module->interface_->type);
        return status;
      }

      return XIA_SUCCESS;
    }
  }

  xiaLog(XIA_LOG_ERROR, status, "writeInterface",
         "Unknown interface type: '%u'", module->interface_->type);
  return XIA_BAD_INTERFACE;
}

#ifndef EXCLUDE_INET
/** @brief Writes the Inet interface info to the passed in file pointer.
 *
 * Assumes that the file has been advanced to the proper location. Also assumes
 * that the module is using the INET communication interface.
 *
 */
static int writeINET(FILE *fp, Module *module)
{
  ASSERT(fp != NULL);
  ASSERT(module != NULL);

  fprintf(fp, "interface = inet\n");
  fprintf(fp, "inet_address = %s\n",
          module->interface_->info.inet->address);
  fprintf(fp, "inet_port = %u\n",
          module->interface_->info.inet->port);
  fprintf(fp, "inet_timeout = %u\n",
          module->interface_->info.inet->timeout);

  return XIA_SUCCESS;
}
#endif /* EXCLUDE_INET */
