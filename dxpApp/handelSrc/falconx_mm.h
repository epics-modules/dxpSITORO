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

#ifndef FALCON_MM_H
#define FALCON_MM_H

/*
 * FalconX Mapping Mode Buffering Support.
 */

/*
 * XMAP Header Size.
 */
#define XMAP_BUFFER_HEADER_SIZE     256 /* 16bit words */
#define XMAP_BUFFER_HEADER_SIZE_U32 (XMAP_BUFFER_HEADER_SIZE / 2)

/*
 * XMAP Pixel Header Size.
 */
#define XMAP_PIXEL_HEADER_SIZE     256 /* 16bit words */
#define XMAP_PIXEL_HEADER_SIZE_U32 (XMAP_PIXEL_HEADER_SIZE / 2)

/*
 * Maximum number of pixels per buffer.
 */
#define XMAP_MAX_PIXELS_PER_BUFFER 1024

/*
 * XMAP mapping stats clock tick in seconds. It's effectively 16x the
 * XMAP clock. We reuse this unit because it's a fair balance of
 * precision and range for mapping pixel stats.
 */
#define XMAP_MAPPING_TICKS 0.00000032

/*
 * Data formatter structures.
 *
 * All levels are counts of uint32_t and not byte offsets.
 */
#define MMC_BUFFERS (2)

/*
 * Modes.
 */
typedef enum {
    MAPPING_MODE_MCA = 0,
    MAPPING_MODE_MCA_FSM = 1,
    MAPPING_MODE_SCA = 2,
    MAPPINGMODE_LIST = 3,
    MAPPING_MODE_COUNT,
    MAPPING_MODE_NIL,
} MM_Mode;

typedef struct
{
    uint32_t realtime;
    uint32_t livetime;
    uint32_t triggers;
    uint32_t output_events;
} MM_Pixel_Stats;

typedef struct
{
    uint32_t low;
    uint32_t high;
} MM_Region;

typedef struct
{
    uint32_t   numOfRegions;
    MM_Region* regions;
} MM_Rois;

/*
 * A buffer is one of 2 output buffers accessed by the Handel user. The buffer
 * is large enough to hold the required number of pixels and any pixel header.
 */
typedef struct
{
    boolean_t full;           /* The buffer is full. */
    boolean_t done;           /* The buffer is done and can be used again. */
    uint32_t  bufferPixel;    /* The pixel count in buffer. */
    size_t    next;           /* The next value to read. */
    size_t    level;          /* The amount of data in the buffer. */
    size_t    marker;         /* Buffer marker. */
    uint32_t* buffer;         /* The buffer. */
    size_t    size;           /* uint32_t units, not bytes. */
} MM_Buffer;

typedef struct
{
    int       active;         /* The active buffer index. */
    uint32_t  bufferNumber;   /* The count of buffers processed */
    uint32_t  pixel;          /* The pixel number. */
    uint32_t  numPixels;      /* The number of pixels in a run. */
    uint32_t  bufferOverruns; /* Count of buffer overruns */
    MM_Buffer buffer[MMC_BUFFERS];
} MM_Buffers;

/*
 * Binner flags.
 */
#define MM_BINNER_GATE_HIGH      (1 << 0)   /* Gate 1 for trigger. */
#define MM_BINNER_GATE_TRIGGER   (1 << 16)  /* Gate has been triggered. */
#define MM_BINNER_STATS_VALID    (1 << 17)  /* The stats are valid. */

#define MM_BINNER_PIXEL_VALID(_b) \
    (((_b)->flags ^ (MM_BINNER_GATE_TRIGGER | MM_BINNER_STATS_VALID)) == 0)

/*
 * The binner takes the list mode data stream from the SiToro API and converts
 * it to bins. The binner has an input buffer used to get the list mode data.
 */
typedef struct
{
    /* Binning output */
    uint32_t  flags;         /* State flags. */
    size_t    numberOfBins;  /* The number of bins. */
    uint64_t* bins;          /* The bins. */
    uint64_t  outOfRange;    /* Count of energy levels out of range. */
    uint32_t  errorBits;     /* Error bits returned from the List API. */
    uint64_t  timestamp;     /* Current timestamp. */
    void*     stats;         /* Extracted stats. */
    /* Input buffering of data from SiToro */
    uint32_t* buffer;        /* Output buffer. */
    size_t    bufferSize;    /* The size of the buffer */
    size_t    bufferLevel;   /* The level of data in the buffer. */
} MM_Binner;

typedef struct
{
    uint32_t   numMCAChannels;
    uint32_t   numStats;
    MM_Buffers buffers;
} MMC0_Data;

typedef struct
{
    uint16_t   numMCAChannels; /* 16 bits constrained by buffer format. */
    int        detChan;
    boolean_t  listMode;
    uint32_t   runNumber;
    uint32_t   pixelHeaderSize;
    uint32_t   bufferHeaderSize;
    int32_t    pixelAdvanceCounter; /* User advance. -1 to disable rewind. */
    MM_Buffers buffers;
    MM_Binner  bins;
} MMC1_Data;

/* Mapping mode control. */
typedef struct {
    /* The mode. */
    MM_Mode mode;

    /* Data formatter, an opaque handle. */
    void* dataFormatter;
} MM_Control;

/*
 * Mapping Mode Buffer.
 */
int psl__MappingModeBuffer_Open(MM_Buffer* buffer, size_t size);
int psl__MappingModeBuffer_Close(MM_Buffer* buffer);
int psl__MappingModeBuffer_A(void);
int psl__MappingModeBuffer_B(void);

/*
 * Mapping Mode Buffers
 */
int       psl__MappingModeBuffers_Open(MM_Buffers* buffers,
                                       size_t      size,
                                       int64_t     numPixels);
int       psl__MappingModeBuffers_Close(MM_Buffers* buffers);
size_t    psl__MappingModeBuffers_Size(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Toggle(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_Update(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Overrun(MM_Buffers* buffers);
uint32_t  psl__MappingModeBuffers_Overruns(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Pixel_Inc(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_PixelsReceived(MM_Buffers* buffers);

boolean_t psl__MappingModeBuffers_A_Full(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_A_Active(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_B_Full(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_B_Active(MM_Buffers* buffers);

int       psl__MappingModeBuffers_Next(MM_Buffers* buffers);
char      psl__MappingModeBuffers_Next_Label(MM_Buffers* buffers);
uint32_t* psl__MappingModeBuffers_Next_Data(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_Next_Full(MM_Buffers* buffers);
int       psl__MappingModeBuffers_Next_Clear(MM_Buffers* buffers);
int       psl__MappingModeBuffers_Next_Reset(MM_Buffers* buffers);
size_t    psl__MappingModeBuffers_Next_Level(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Next_SetLevel(MM_Buffers* buffers, size_t level);
void      psl__MappingModeBuffers_Next_MoveLevel(MM_Buffers* buffers, size_t level);
size_t    psl__MappingModeBuffers_Next_Remaining(MM_Buffers* buffers);
uint32_t  psl__MappingModeBuffers_Next_Pixels(MM_Buffers* buffers);
uint32_t  psl__MappingModeBuffers_Next_PixelTotal(MM_Buffers* buffers);

int       psl__MappingModeBuffers_Active(MM_Buffers* buffers);
char      psl__MappingModeBuffers_Active_Label(MM_Buffers* buffers);
uint32_t* psl__MappingModeBuffers_Active_Data(MM_Buffers* buffers);
boolean_t psl__MappingModeBuffers_Active_Done(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Active_SetDone(MM_Buffers* buffers);
int       psl__MappingModeBuffers_Active_Clear(MM_Buffers* buffers);
int       psl__MappingModeBuffers_Active_Reset(MM_Buffers* buffers);
size_t    psl__MappingModeBuffers_Active_Level(MM_Buffers* buffers);
void      psl__MappingModeBuffers_Active_SetLevel(MM_Buffers* buffers, size_t level);
void      psl__MappingModeBuffers_Active_MoveLevel(MM_Buffers* buffers, size_t level);
size_t    psl__MappingModeBuffers_Active_Remaining(MM_Buffers* buffers);
uint32_t  psl__MappingModeBuffers_Active_Pixels(MM_Buffers* buffers);
uint32_t  psl__MappingModeBuffers_Active_PixelTotal(MM_Buffers* buffers);

int psl__MappingModeBuffers_CopyIn(MM_Buffers* buffers, void* value, size_t size);
int psl__MappingModeBuffers_CopyOut(MM_Buffers* buffers, void* value, size_t* size);

/*
 * Mapping Mode Binner.
 */
int psl__MappingModeBinner_Open(MM_Binner* binner,
                                size_t     bins);
int psl__MappingModeBinner_Close(MM_Binner* binner);
int psl__MappingModeBinner_BinAdd(MM_Binner* binner,
                                  uint32_t   bin,
                                  uint32_t   amount);
int psl__MappingModeBinner_DataCopy(MM_Binner*      binner,
                                    MM_Buffers*     buffers);

/*
 * Mapping Mode Control.
 */
boolean_t psl__MappingModeControl_IsMode(MM_Control* control, MM_Mode mode);

int psl__MappingModeControl_CloseAny(MM_Control* control);

int psl__MappingModeControl_OpenMM0(MM_Control* control,
                                    uint16_t    number_mca_channels,
                                    uint32_t    number_stats);
int psl__MappingModeControl_CloseMM0(MM_Control* control);
MMC0_Data* psl__MappingModeControl_MM0Data(MM_Control* control);

int psl__MappingModeControl_OpenMM1(MM_Control* control,
                                    int         detChan,
                                    boolean_t   listmode,
                                    uint32_t    run_number,
                                    int64_t     num_pixels,
                                    uint16_t    number_mca_channels,
                                    int64_t     num_pixels_buffer);
int psl__MappingModeControl_CloseMM1(MM_Control* control);
MMC1_Data* psl__MappingModeControl_MM1Data(MM_Control* control);
size_t psl__MappingModeControl_MM1BufferSize(uint16_t number_mca_channels,
                                             int64_t num_pixels_per_buffer);

MM_Mode psl__MappingModeControl_Mode(MM_Control* control);

/*
 * XMAP Helpers.
 */
uint16_t psl__Lower16(uint32_t value);
uint16_t psl__Upper16(uint32_t value);
void psl__Write32(uint16_t* buffer, uint32_t value);

int psl__XMAP_WriteBufferHeader_MM1(MMC1_Data* mm1);
int psl__XMAP_UpdateBufferHeader_MM1(MMC1_Data* mm1);
int psl__XMAP_WritePixelHeader_MM1(MMC1_Data* mm1, MM_Pixel_Stats* stats);

#endif
