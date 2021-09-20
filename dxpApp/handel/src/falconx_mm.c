/*
 * Copyright (c) 2016 XIA LLC
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

/*
 * FalconX Platform Specific Layer
 */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>

#include "handel_log.h"

#include "psldef.h"
#include "psl_common.h"

#include "xia_handel.h"
#include "xia_system.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "handel_errors.h"

#include "md_threads.h"

#include "handel_mapping_modes.h"

#include "falconx_mm.h"
#include "falconxn_psl.h"

/*
 * Data Formatter Helpers
 */

int psl__MappingModeBuffer_A(void)
{
    return 0;
}

int psl__MappingModeBuffer_B(void)
{
    return 1;
}

size_t psl__MappingModeBuffers_Size(MM_Buffers* buffers)
{
    return buffers->buffer[0].size;
}

static boolean_t psl__MappingModeBuffers_Full(MM_Buffers* buffers, int buffer)
{
    return
        (buffers->buffer[buffer].level > 0) &&
        ((buffers->buffer[buffer].level >= buffers->buffer[buffer].size) ||
         psl__MappingModeBuffers_PixelsReceived(buffers));
}

boolean_t psl__MappingModeBuffers_A_Full(MM_Buffers* buffers)
{
    return psl__MappingModeBuffers_Full(buffers, 0);
}

boolean_t psl__MappingModeBuffers_A_Active(MM_Buffers* buffers)
{
    return buffers->active == 0;
}

boolean_t psl__MappingModeBuffers_B_Full(MM_Buffers* buffers)
{
    return psl__MappingModeBuffers_Full(buffers, 1);
}

boolean_t psl__MappingModeBuffers_B_Active(MM_Buffers* buffers)
{
    return buffers->active == 1;
}

int psl__MappingModeBuffers_Next(MM_Buffers* buffers)
{
    return buffers->active ? 0 : 1;
}

int psl__MappingModeBuffers_Active(MM_Buffers* buffers)
{
    return buffers->active;
}

char psl__MappingModeBuffers_Active_Label(MM_Buffers* buffers)
{
    return buffers->active == 0 ? 'A' : 'B';
}

char psl__MappingModeBuffers_Next_Label(MM_Buffers* buffers)
{
    return buffers->active == 1 ? 'A' : 'B';
}

boolean_t psl__MappingModeBuffers_Next_Full(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Full(buffers, buffer);
}

boolean_t psl__MappingModeBuffers_Active_Done(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return buffers->buffer[buffer].done;
}

void psl__MappingModeBuffers_Active_SetDone(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    buffers->buffer[buffer].done = TRUE_;
}

PSL_STATIC void psl__MappingModeBuffers_Active_Set(MM_Buffers* buffers, int buffer)
{
    buffers->active = buffer;
    buffers->buffer[buffer].done = FALSE_;
}

void psl__MappingModeBuffers_Toggle(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    psl__MappingModeBuffers_Active_Set(buffers, buffer);
    psl__MappingModeBuffers_Active_Reset(buffers);
}

void psl__MappingModeBuffers_Overrun(MM_Buffers* buffers)
{
    ++buffers->bufferOverruns;
}

uint32_t psl__MappingModeBuffers_Overruns(MM_Buffers* buffers)
{
    uint32_t overruns = buffers->bufferOverruns;
    buffers->bufferOverruns = 0;
    return overruns;
}

boolean_t psl__MappingModeBuffers_PixelsReceived(MM_Buffers* buffers)
{
    return (buffers->numPixels > 0) && (buffers->pixel >= buffers->numPixels);
}

PSL_STATIC uint32_t* psl__MappingModeBuffers_Data(MM_Buffers* buffers, int buffer)
{
    MM_Buffer* mmb = &buffers->buffer[buffer];
    return &mmb->buffer[0];
}

uint32_t* psl__MappingModeBuffers_Next_Data(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Data(buffers, buffer);
}

uint32_t* psl__MappingModeBuffers_Active_Data(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_Data(buffers, buffer);
}

PSL_STATIC int psl__MappingModeBuffers_Clear(MM_Buffers* buffers, int buffer)
{
    MM_Buffer* mmb = &buffers->buffer[buffer];
    mmb->level = 0;
    mmb->next = 0;
    mmb->bufferPixel = 0;
    mmb->marker = 0;
    mmb->full = FALSE_;
    mmb->done = TRUE_;
    return XIA_SUCCESS;
}

int psl__MappingModeBuffers_Next_Clear(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Clear(buffers, buffer);
}

int psl__MappingModeBuffers_Active_Clear(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_Clear(buffers, buffer);
}

PSL_STATIC int psl__MappingModeBuffers_Reset(MM_Buffers* buffers, int buffer)
{
    MM_Buffer* mmb = &buffers->buffer[buffer];
    mmb->next = 0;
    return XIA_SUCCESS;
}

int psl__MappingModeBuffers_Next_Reset(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Reset(buffers, buffer);
}

int psl__MappingModeBuffers_Active_Reset(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_Reset(buffers, buffer);
}


PSL_STATIC size_t psl__MappingModeBuffers_Level(MM_Buffers* buffers, int buffer)
{
    MM_Buffer* mmb = &buffers->buffer[buffer];
    return mmb->level;
}

static void psl__MappingModeBuffers_SetLevel(MM_Buffers* buffers,
                                             int buffer, size_t level)
{
    MM_Buffer* mmb = &buffers->buffer[buffer];
    mmb->level = level;
    mmb->full = psl__MappingModeBuffers_Full(buffers, buffer);
}

size_t psl__MappingModeBuffers_Next_Level(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Level(buffers, buffer);
}

void psl__MappingModeBuffers_Next_SetLevel(MM_Buffers* buffers, size_t level)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    psl__MappingModeBuffers_SetLevel(buffers, buffer, level);
}

void psl__MappingModeBuffers_Next_MoveLevel(MM_Buffers* buffers, size_t level)
{
    size_t current_level = psl__MappingModeBuffers_Next_Level(buffers);
    psl__MappingModeBuffers_Next_SetLevel(buffers, current_level + level);
}

size_t psl__MappingModeBuffers_Active_Level(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_Level(buffers, buffer);
}

void psl__MappingModeBuffers_Active_SetLevel(MM_Buffers* buffers, size_t level)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    psl__MappingModeBuffers_SetLevel(buffers, buffer, level);
}

void psl__MappingModeBuffers_Active_MoveLevel(MM_Buffers* buffers, size_t level)
{
    size_t current_level = psl__MappingModeBuffers_Active_Level(buffers);
    psl__MappingModeBuffers_Active_SetLevel(buffers, current_level + level);
}

size_t psl__MappingModeBuffers_Next_Remaining(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    MM_Buffer* mmb = &buffers->buffer[buffer];
    return mmb->size - mmb->level;
}

size_t psl__MappingModeBuffers_Active_Remaining(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    MM_Buffer* mmb = &buffers->buffer[buffer];
    return mmb->level - mmb->next;
}

PSL_STATIC uint32_t psl__MappingModeBuffers_Pixels(MM_Buffers* buffers, int buffer)
{
    return buffers->buffer[buffer].bufferPixel;
}

uint32_t psl__MappingModeBuffers_Next_Pixels(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_Pixels(buffers, buffer);
}

uint32_t psl__MappingModeBuffers_Active_Pixels(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_Pixels(buffers, buffer);
}

PSL_STATIC uint32_t psl__MappingModeBuffers_PixelTotal(MM_Buffers* buffers,
                                                       int buffer)
{
    UNUSED(buffer);

    return buffers->pixel;
}

uint32_t psl__MappingModeBuffers_Next_PixelTotal(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    return psl__MappingModeBuffers_PixelTotal(buffers, buffer);
}

uint32_t psl__MappingModeBuffers_Active_PixelTotal(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Active(buffers);
    return psl__MappingModeBuffers_PixelTotal(buffers, buffer);
}

void psl__MappingModeBuffers_Pixel_Inc(MM_Buffers* buffers)
{
    int buffer = psl__MappingModeBuffers_Next(buffers);
    MM_Buffer* mmb = &buffers->buffer[buffer];
    ++buffers->pixel;
    ++mmb->bufferPixel;
}

int psl__MappingModeBuffers_CopyIn(MM_Buffers* buffers, void* value, size_t size)
{
    int status = XIA_SUCCESS;

    int buffer = psl__MappingModeBuffers_Next(buffers);

    MM_Buffer* mmb = &buffers->buffer[buffer];

    pslLog(PSL_LOG_DEBUG,
           "COPY-IN buffer:%c length:%d level:%d size:%d",
           buffer == 0 ? 'A' : 'B', (int) size, (int) mmb->level, (int) mmb->size);

    if ((mmb->level + size) > mmb->size) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "MMBuffer: Buffer %c overflow",
               psl__MappingModeBuffers_Next_Label(buffers));
        return status;
    }

    memcpy(&mmb->buffer[mmb->level], value, size * sizeof(uint32_t));

    mmb->level += size;

    mmb->full = psl__MappingModeBuffers_Full(buffers, buffer);

    return status;
}

int psl__MappingModeBuffers_CopyOut(MM_Buffers* buffers, void* value, size_t* size)
{
    int status = XIA_SUCCESS;

    int buffer = psl__MappingModeBuffers_Active(buffers);

    MM_Buffer* mmb = &buffers->buffer[buffer];

    pslLog(PSL_LOG_DEBUG,
           "COPY-OUT buffer:%c level:%d size:%d",
           buffer == 0 ? 'A' : 'B', (int) mmb->level, (int) *size);

    /*
     * If size is 0 copy the remaining data.
     */
    if (*size == 0)
        *size = mmb->level - mmb->next;

    if ((mmb->next + *size) > mmb->level)
        *size = mmb->level - mmb->next;

    memcpy(value, &mmb->buffer[mmb->next], *size * sizeof(uint32_t));

    mmb->next += *size;
    mmb->full = psl__MappingModeBuffers_Full(buffers, buffer);

    return status;
}

boolean_t psl__MappingModeBuffers_Update(MM_Buffers* buffers)
{
    /*
     * If the Next buffer is full check if the Active is empty. If empty we can
     * toggle the buffers. If the Active still has data there is nothing we can
     * do. The user has to read all the data or we overrun the buffers.
     */

    pslLog(PSL_LOG_DEBUG,
           "UPDATE: NextFull:%c ActiveDone:%c",
           psl__MappingModeBuffers_Next_Full(buffers) ? 'Y' : 'N',
           psl__MappingModeBuffers_Active_Done(buffers) ? 'Y' : 'N');

    if (psl__MappingModeBuffers_Next_Full(buffers) &&
        psl__MappingModeBuffers_Active_Done(buffers)) {
        psl__MappingModeBuffers_Toggle(buffers);
        return TRUE_;
    }
    return FALSE_;
}

int psl__MappingModeBuffer_Open(MM_Buffer* buffer, size_t size)
{
    int status = XIA_SUCCESS;

    if (!buffer->buffer) {
        memset(buffer, 0, sizeof(*buffer));

        buffer->buffer = handel_md_alloc(size * sizeof(uint32_t));
        if (!buffer->buffer) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for MM buffer");
            return status;
        }

        memset(buffer->buffer, 0, size * sizeof(uint32_t));
        buffer->full = FALSE_;
        buffer->done = TRUE_;
        buffer->next = 0;
        buffer->bufferPixel = 0;
        buffer->marker = 0;
        buffer->size = size;
    }

    return status;
}

int psl__MappingModeBuffer_Close(MM_Buffer* buffer)
{
    int status = XIA_SUCCESS;

    if (buffer->buffer) {
        handel_md_free(buffer->buffer);
        memset(buffer, 0, sizeof(*buffer));
    }

    return status;
}

int psl__MappingModeBuffers_Open(MM_Buffers* buffers, size_t size, int64_t numPixels)
{
    int status = XIA_SUCCESS;
    int buffer = 0;

    pslLog(PSL_LOG_DEBUG,
           "size:%u (%u)", (uint32_t) size, (uint32_t) (size * sizeof(uint32_t)));

    buffers->active = 1;
    buffers->bufferNumber = 0;
    buffers->numPixels = (uint32_t) numPixels;
    buffers->pixel = 0;

    while (buffer < MMC_BUFFERS) {
        status = psl__MappingModeBuffer_Open(&buffers->buffer[buffer], size);
        if (status != XIA_SUCCESS) {
            while (buffer > 0) {
                --buffer;
                psl__MappingModeBuffer_Close(&buffers->buffer[buffer]);
            }
            return status;
        }
        ++buffer;
    }

    return status;
}

int psl__MappingModeBuffers_Close(MM_Buffers* buffers)
{
    int status = XIA_SUCCESS;
    int buffer = MMC_BUFFERS;

    while (buffer > 0) {
        int this_status;
        --buffer;
        this_status = psl__MappingModeBuffer_Close(&buffers->buffer[buffer]);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS)) {
            status = this_status;
        }
    }

    return status;
}

int psl__MappingModeBinner_Open(MM_Binner* binner,
                                size_t     bins)
{
    int status = XIA_SUCCESS;

    if (!binner->bins) {
        binner->bins = handel_md_alloc(bins * sizeof(uint64_t));
        if (!binner->bins) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for MM bins");
            return status;
        }
        binner->buffer = NULL; /* point to next buffer's pixel */
        if (!binner->buffer) {
            handel_md_free(binner->bins);
            binner->bins = NULL;
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for MM Bin's buffer");
            return status;
        }
        memset(binner->bins, 0, bins * sizeof(uint64_t));
        binner->flags = MM_BINNER_GATE_HIGH;
        binner->numberOfBins = bins;
        binner->outOfRange = 0;
        binner->errorBits = 0;
        memset(&binner->stats, 0, sizeof(binner->stats));
        binner->bufferSize = 0;
    }

    return status;
}

int psl__MappingModeBinner_Close(MM_Binner* binner)
{
    int status = XIA_SUCCESS;

    if (binner->bins) {
        handel_md_free(binner->buffer);
        handel_md_free(binner->bins);
        memset(binner, 0, sizeof(*binner));
    }

    return status;
}

int psl__MappingModeBinner_BinAdd(MM_Binner* binner,
                                  uint32_t   bin,
                                  uint32_t   amount)
{
    if (bin >= binner->numberOfBins) {
        ++binner->outOfRange;
    }
    else {
        binner->bins[bin] += amount;
    }
    return XIA_SUCCESS;
}

int psl__MappingModeBinner_DataCopy(MM_Binner*      binner,
                                    MM_Buffers*     buffers)
{
    int status = XIA_SUCCESS;

    MM_Buffer* buffer = &buffers->buffer[buffers->active];

    if (buffer->full)  {
        status = XIA_INTERNAL_BUFFER_OVERRUN;
    }
    else {
        const size_t srcSize = binner->bufferLevel;
        const size_t dstSize = buffer->size - buffer->level;
        const size_t copy = dstSize < srcSize ? dstSize : srcSize;

        if (copy) {
            pslLog(PSL_LOG_DEBUG,
                   "buffer:%c dst:%u src:%u copy:%u full:%s",
                   buffers->active == 0 ? 'A' : 'B',
                   (int) dstSize, (int) srcSize, (int) copy,
                   (buffer->level + copy) >= buffer->size ? "YES" : "NO");

            /*
             * Copy the data to the output buffer.
             */
            memcpy(buffer->buffer + buffer->level,
                   binner->buffer,
                   copy * sizeof(uint32_t));

            buffer->level += copy;
            binner->bufferLevel -= copy;

            /*
             * Compact the input buffer.
             */
            if (copy < srcSize)
                memmove(binner->buffer,
                        binner->buffer + copy,
                        (srcSize - copy) * sizeof(uint32_t));

            /*
             * Update the buffer full flag.
             */
            buffer->full = psl__MappingModeBuffers_Full(buffers,
                                                        buffers->active);
        }
    }

    return status;
}

boolean_t psl__MappingModeControl_IsMode(MM_Control* mmc, MM_Mode mode)
{
    return (mmc->mode == mode) && (mmc->dataFormatter != NULL);
}

int psl__MappingModeControl_CloseAny(MM_Control* control)
{
    int status = XIA_SUCCESS;
    if (psl__MappingModeControl_IsMode(control, MAPPING_MODE_MCA))
        status = psl__MappingModeControl_CloseMM0(control);
    else if (psl__MappingModeControl_IsMode(control, MAPPING_MODE_MCA_FSM))
        status = psl__MappingModeControl_CloseMM1(control);
    return status;
}

int psl__MappingModeControl_OpenMM0(MM_Control* control,
                                    uint16_t    number_mca_channels,
                                    uint32_t    number_stats)
{
    int status = XIA_SUCCESS;

    MMC0_Data* mm0;

    if (control->dataFormatter != NULL) {
        status = XIA_ALREADY_OPEN;
        pslLog(PSL_LOG_ERROR, status,
               "Mapping mode control already open");
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "MM0 Open: number_mca_channels=%d number_stats=%d",
           (int) number_mca_channels, (int) number_stats);

    control->mode = MAPPING_MODE_NIL;

    mm0 = handel_md_alloc(sizeof(MMC0_Data));

    if (!mm0) {
        status = XIA_NOMEM;
        pslLog(PSL_LOG_ERROR, status,
               "Error allocating memory for MMC0 data");
        return status;
    }

    memset(mm0, 0, sizeof(MMC0_Data));

    status = psl__MappingModeBuffers_Open(&mm0->buffers,
                                          (size_t) ((number_mca_channels * 2) +
                                                    number_stats),
                                          0);
    if (status != XIA_SUCCESS) {
        handel_md_free(mm0);
        return status;
    }

    mm0->numMCAChannels = number_mca_channels;
    mm0->numStats = number_stats;

    control->dataFormatter = mm0;
    control->mode = MAPPING_MODE_MCA;

    return status;
}

int psl__MappingModeControl_CloseMM0(MM_Control* control)
{
    int status = XIA_SUCCESS;

    pslLog(PSL_LOG_DEBUG, "MM0 Close");

    control->mode = MAPPING_MODE_NIL;

    if (control->dataFormatter) {
        MMC0_Data* mm0 = control->dataFormatter;
        int        this_status;
        this_status = psl__MappingModeBuffers_Close(&mm0->buffers);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS))
            status = this_status;
        handel_md_free(control->dataFormatter);
        control->dataFormatter = NULL;
    }

    return status;
}

MMC0_Data* psl__MappingModeControl_MM0Data(MM_Control* control)
{
    return control->dataFormatter;
}

int psl__MappingModeControl_OpenMM1(MM_Control* control,
                                    int         detChan,
                                    boolean_t   listmode,
                                    uint32_t    run_number,
                                    int64_t     num_pixels,
                                    uint16_t    number_mca_channels,
                                    int64_t     num_pixels_per_buffer)
{
    int status = XIA_SUCCESS;

    MMC1_Data* mm1;

    size_t buffer_size;

    if (control->dataFormatter != NULL) {
        status = XIA_ALREADY_OPEN;
        pslLog(PSL_LOG_ERROR, status,
               "Mapping Mode control already open");
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "MM1 Open: listmode=%d run_number=%d num_pixels=%d "\
           "number_mca_channels=%d num_pixels_per_buffer=%d",
           (int) listmode, (int) run_number, (int) num_pixels,
           (int) number_mca_channels, (int) num_pixels_per_buffer);

    control->mode = MAPPING_MODE_NIL;

    mm1 = handel_md_alloc(sizeof(MMC1_Data));

    if (!mm1) {
        status = XIA_NOMEM;
        pslLog(PSL_LOG_ERROR, status,
               "Error allocating memory for MMC1 data");
        return status;
    }

    memset(mm1, 0, sizeof(MMC1_Data));

    if (listmode) {
        status = psl__MappingModeBinner_Open(&mm1->bins, (size_t) number_mca_channels);
        if (status != XIA_SUCCESS) {
            handel_md_free(mm1);
            return status;
        }
    }

    buffer_size = psl__MappingModeControl_MM1BufferSize(number_mca_channels,
                                                        num_pixels_per_buffer);

    status = psl__MappingModeBuffers_Open(&mm1->buffers, buffer_size, num_pixels);
    if (status != XIA_SUCCESS) {
        psl__MappingModeBinner_Close(&mm1->bins);
        handel_md_free(mm1);
        return status;
    }

    /*
     * Set the buffer overheads for the mode.
     */
    mm1->detChan = detChan;
    mm1->listMode = listmode;
    mm1->numMCAChannels = (uint16_t) number_mca_channels;
    mm1->runNumber = run_number;

    control->dataFormatter = mm1;
    control->mode = MAPPING_MODE_MCA_FSM;

    return status;
}

int psl__MappingModeControl_CloseMM1(MM_Control* control)
{
    int status = XIA_SUCCESS;

    control->mode = MAPPING_MODE_NIL;

    pslLog(PSL_LOG_DEBUG, "MM1 Close");

    if (control->dataFormatter) {
        MMC1_Data* data = control->dataFormatter;
        int        this_status;
        this_status = psl__MappingModeBuffers_Close(&data->buffers);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS))
            status = this_status;
        this_status = psl__MappingModeBinner_Close(&data->bins);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS))
            status = this_status;
        handel_md_free(control->dataFormatter);
        control->dataFormatter = NULL;
    }

    return status;
}

MMC1_Data* psl__MappingModeControl_MM1Data(MM_Control* control)
{
    return control->dataFormatter;
}

size_t psl__MappingModeControl_MM1BufferSize(uint16_t number_mca_channels,
                                             int64_t num_pixels_per_buffer)
{
    if (num_pixels_per_buffer == 0)
        num_pixels_per_buffer = XMAP_MAX_PIXELS_PER_BUFFER;

    return XMAP_BUFFER_HEADER_SIZE_U32 +
        (size_t) (num_pixels_per_buffer *
                  (number_mca_channels + XMAP_PIXEL_HEADER_SIZE_U32));
}

MM_Mode psl__MappingModeControl_Mode(MM_Control* control)
{
    return control->mode;
}

uint16_t psl__Lower16(uint32_t value)
{
    return value & 0xFFFF;
}

uint16_t psl__Upper16(uint32_t value)
{
    return value >> 16;
}

void psl__Write32(uint16_t* buffer, uint32_t value)
{
    buffer[0] = psl__Lower16(value);
    buffer[1] = psl__Upper16(value);
}

int psl__XMAP_WriteBufferHeader_MM1(MMC1_Data* mm1)
{
    int status = XIA_SUCCESS;

    MM_Buffers* mmb = &mm1->buffers;

    uint16_t* in = (uint16_t*) psl__MappingModeBuffers_Next_Data(mmb);

    int i;

    /*
     * Taken from the XMAP_User_Manual.pdf with the current release.
     * Section 5.3.3.2.
     */

    /* 0,1: tag0, tag1, 16bits each */
    in[0] = 0x55aa;
    in[1] = 0xaa55;

    /* 2: header size, 16bits */
    in[2] = XMAP_BUFFER_HEADER_SIZE;

    /* 3: mapping mode, 16bits */
    in[3] = MAPPING_MODE_MCA_FSM;

    /* 4: run number, 16bits */
    in[4] = (uint16_t) mm1->runNumber;

    /* 5,6: buffer number, 32bits */
    psl__Write32(&in[5], mmb->bufferNumber);

    /* 7: buffer id, 16bits */
    in[7] = (uint16_t) psl__MappingModeBuffers_Next(mmb);

    /* 8: number of pixels in the buffer, 16bits */
    in[8] = 0;

    /* 9,10: starting pixel, 32bits */
    psl__Write32(&in[9], psl__MappingModeBuffers_Next_PixelTotal(mmb));

    /* 11: module ID, 16bits */
    in[11] = 0;

    /* 12: detector channel, 16bits */
    in[12] = (uint16_t) mm1->detChan;

    /* Remainder of XMAP_BUFFER_HEADER_SIZE: set to 0 */
    for (i = 13; i < XMAP_BUFFER_HEADER_SIZE; ++i)
        in[i] = 0;

    psl__MappingModeBuffers_Next_MoveLevel(mmb, XMAP_BUFFER_HEADER_SIZE_U32);

    mmb->bufferNumber++;

    return status;
}

int psl__XMAP_UpdateBufferHeader_MM1(MMC1_Data* mm1)
{
    int status = XIA_SUCCESS;

    MM_Buffers* mmb = &mm1->buffers;

    uint16_t* in = (uint16_t*) psl__MappingModeBuffers_Next_Data(mmb);

    /* 8: number of pixels in the buffer, 16bits */
    in[8] = (uint16_t) psl__MappingModeBuffers_Next_Pixels(mmb);

    return status;
}

int psl__XMAP_WritePixelHeader_MM1(MMC1_Data* mm1, MM_Pixel_Stats* stats)
{
    int status = XIA_SUCCESS;

    MM_Buffers* mmb = &mm1->buffers;

    uint32_t* buf = psl__MappingModeBuffers_Next_Data(mmb);
    size_t    level = psl__MappingModeBuffers_Next_Level(mmb);
    uint16_t* in = (uint16_t*) &buf[level];

    int i;

    const uint16_t ch_block_size = mm1->numMCAChannels * 2;
    const uint32_t pixel_block_size =
        XMAP_PIXEL_HEADER_SIZE_U32 * 2 + ch_block_size;

    /*
     * Taken from the XMAP_User_Manual.pdf with the current release.
     * Section 5.3.3.3.
     */

    /* 0,1: tag0, tag1, 16bits each */
    in[0] = 0x33cc;
    in[1] = 0xcc33;

    /* 2: header size, 16bits */
    in[2] = XMAP_PIXEL_HEADER_SIZE;

    /* 3: mapping mode, 16bits */
    in[3] = MAPPING_MODE_MCA_FSM;

    /* 4,5: pixel number, 32bits */
    psl__Write32(&in[4], psl__MappingModeBuffers_Next_PixelTotal(mmb));

    /* 6,7: block size, 32bits */
    psl__Write32(&in[6], pixel_block_size);

    /* 8: this channel block size, 16bits */
    in[8] = ch_block_size;

    /* 10->31: set to 0 */
    for (i = 10; i < 32; ++i)
        in[i] = 0;

    /* 32,33: ch0 realtime */
    psl__Write32(&in[32], stats->realtime);
    /* 34,35: ch0 livetime */
    psl__Write32(&in[34], stats->livetime);
    /* 36,37: ch0 triggers */
    psl__Write32(&in[36], stats->triggers);
    /* 38,39: ch0 output events */
    psl__Write32(&in[38], stats->output_events);

    /* 8->XMAP_PIXEL_HEADER_SIZE: set to 0 */
    for (i = 40; i < XMAP_PIXEL_HEADER_SIZE; ++i)
        in[i] = 0;

    psl__MappingModeBuffers_Next_MoveLevel(mmb, XMAP_PIXEL_HEADER_SIZE_U32);

    return status;
}
