/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2013 XIA LLC
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

#ifndef HANDEL_MAPPING_MODES_H
#define HANDEL_MAPPING_MODES_H

#include <stdint.h>

#include "xia_common.h"

#include "handeldef.h"

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mapping Mode block headers.
 */

#define HANDEL_MM_WORDS (256)

#define HANDEL_MM_WORD(_l)      uint16_t _l; uint16_t _spare_ ## _l
#define HANDEL_MM_ARRAY(_l, _s, _e) uint16_t _l[((_e) - (_s) + 1) * 2]

typedef struct _MappingMode0 {
    HANDEL_MM_WORD(tag0);                  /*  0 */
    HANDEL_MM_WORD(tag1);                  /*  1 */
    HANDEL_MM_WORD(headerSize);            /*  2 */
    HANDEL_MM_WORD(mappingMode);           /*  3 */
    HANDEL_MM_WORD(runNumber);             /*  4 */
    HANDEL_MM_WORD(seqBufferNum_LSW);      /*  5 */
    HANDEL_MM_WORD(seqBufferNum_MSW);      /*  6 */
    HANDEL_MM_WORD(bufferId);              /*  7 */
    HANDEL_MM_WORD(numPixels);             /*  8 */
    HANDEL_MM_WORD(startingPixel_LSW);     /*  9 */
    HANDEL_MM_WORD(startingPixel_MSW);     /* 10 */
    HANDEL_MM_WORD(modSerialNum);          /* 11 */
    HANDEL_MM_WORD(detChan0);              /* 12 */
    HANDEL_MM_WORD(detElement0);           /* 13 */
    HANDEL_MM_WORD(detChan1);              /* 14 */
    HANDEL_MM_WORD(detElement1);           /* 15 */
    HANDEL_MM_WORD(detChan2);              /* 16 */
    HANDEL_MM_WORD(detElement2);           /* 17 */
    HANDEL_MM_WORD(detChan3);              /* 18 */
    HANDEL_MM_WORD(detElement3);           /* 19 */
    HANDEL_MM_WORD(chanSize0);             /* 20 */
    HANDEL_MM_WORD(chanSize1);             /* 21 */
    HANDEL_MM_WORD(chanSize2);             /* 22 */
    HANDEL_MM_WORD(chanSize3);             /* 23 */
    HANDEL_MM_WORD(bufferErrors);          /* 24 */
    HANDEL_MM_ARRAY(res25_31, 25, 31);     /* 25-31 */
    HANDEL_MM_ARRAY(user, 32, 63);         /* 32-63 */
    HANDEL_MM_ARRAY(res64_255, 64, 255);   /* 64-255 */
} MappingMode0;

typedef struct _MappingMode1 {
    HANDEL_MM_WORD(tag0);                  /*  0 */
    HANDEL_MM_WORD(tag1);                  /*  1 */
    HANDEL_MM_WORD(headerSize);            /*  2 */
    HANDEL_MM_WORD(mappingMode);           /*  3 */
    HANDEL_MM_WORD(pixelNum_LSW);          /*  4 */
    HANDEL_MM_WORD(pixelNum_MSW);          /*  5 */
    HANDEL_MM_WORD(totalPixelBlkSize_LSW); /*  6 */
    HANDEL_MM_WORD(totalPixelBlkSize_MSW); /*  7 */
    HANDEL_MM_WORD(chan0Size);             /*  8 */
    HANDEL_MM_WORD(chan1Size);             /*  9 */
    HANDEL_MM_WORD(chan2Size);             /* 10 */
    HANDEL_MM_WORD(chan3Size);             /* 11 */
    HANDEL_MM_ARRAY(res12_31, 12, 31);     /* 12-31 */
    HANDEL_MM_WORD(chan0Realtime_LSW);     /* 32 */
    HANDEL_MM_WORD(chan0Realtime_MSW);     /* 33 */
    HANDEL_MM_WORD(chan0Livetime_LSW);     /* 34 */
    HANDEL_MM_WORD(chan0Livetime_MSW);     /* 35 */
    HANDEL_MM_WORD(chan0Triggers_LSW);     /* 36 */
    HANDEL_MM_WORD(chan0Triggers_MSW);     /* 37 */
    HANDEL_MM_WORD(chan0OutEvents_LSW);    /* 38 */
    HANDEL_MM_WORD(chan0OutEvents_MSW);    /* 39 */
    HANDEL_MM_WORD(chan1Realtime_LSW);     /* 40 */
    HANDEL_MM_WORD(chan1Realtime_MSW);     /* 41 */
    HANDEL_MM_WORD(chan1Livetime_LSW);     /* 42 */
    HANDEL_MM_WORD(chan1Livetime_MSW);     /* 43 */
    HANDEL_MM_WORD(chan1Triggers_LSW);     /* 44 */
    HANDEL_MM_WORD(chan1Triggers_MSW);     /* 45 */
    HANDEL_MM_WORD(chan1OutEvents_LSW);    /* 46 */
    HANDEL_MM_WORD(chan1OutEvents_MSW);    /* 47 */
    HANDEL_MM_WORD(chan2Realtime_LSW);     /* 48 */
    HANDEL_MM_WORD(chan2Realtime_MSW);     /* 49 */
    HANDEL_MM_WORD(chan2Livetime_LSW);     /* 50 */
    HANDEL_MM_WORD(chan2Livetime_MSW);     /* 51 */
    HANDEL_MM_WORD(chan2Triggers_LSW);     /* 52 */
    HANDEL_MM_WORD(chan2Triggers_MSW);     /* 53 */
    HANDEL_MM_WORD(chan2OutEvents_LSW);    /* 54 */
    HANDEL_MM_WORD(chan2OutEvents_MSW);    /* 55 */
    HANDEL_MM_WORD(chan3Realtime_LSW);     /* 56 */
    HANDEL_MM_WORD(chan3Realtime_MSW);     /* 57 */
    HANDEL_MM_WORD(chan3Livetime_LSW);     /* 58 */
    HANDEL_MM_WORD(chan3Livetime_MSW);     /* 59 */
    HANDEL_MM_WORD(chan3Triggers_LSW);     /* 60 */
    HANDEL_MM_WORD(chan3Triggers_MSW);     /* 61 */
    HANDEL_MM_WORD(chan3OutEvents_LSW);    /* 62 */
    HANDEL_MM_WORD(chan3OutEvents_MSW);    /* 63 */
    HANDEL_MM_ARRAY(res64_255, 64, 255);   /* 64-255 */
} MappingMode1;

    /* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

#endif /* Endif for HANDEL_MAPPING_MODES_H */
