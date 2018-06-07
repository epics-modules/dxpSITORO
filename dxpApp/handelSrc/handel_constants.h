/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2016 XIA LLC
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


#ifndef HANDEL_CONSTANTS_H
#define HANDEL_CONSTANTS_H

/* Preset Run Types */
#define XIA_PRESET_NONE           0.0 /**< Run until stopped by user. */
#define XIA_PRESET_FIXED_REAL     1.0 /**< Run for fixed real time. */
#define XIA_PRESET_FIXED_LIVE     2.0 /**< Run for fixed live time.
                                       * Unimplemented for
                                       * FalconXn. */
#define XIA_PRESET_FIXED_EVENTS   3.0 /**< Run until a given number of
                                       * output pulses are counted. */
#define XIA_PRESET_FIXED_TRIGGERS 4.0 /**< Run until a given number of
                                       * input triggers are counted. */

/* Module statistics */
#define XIA_NUM_MODULE_STATISTICS 9 /**< Number of statistics per
                                     * channel in the module
                                     * statistics block.
                                     */

/* Preamplifier type */
#define XIA_PREAMP_RESET 0.0
#define XIA_PREAMP_RC    1.0

/* Peak mode **/
#define XIA_PEAK_SENSING_MODE     0
#define XIA_PEAK_SAMPLING_MODE    1

/* Mapping Mode Point Control Types */
#define XIA_MAPPING_CTL_USER 0.0
#define XIA_MAPPING_CTL_GATE 1.0
#define XIA_MAPPING_CTL_SYNC 2.0

/* GATE polarity */
#define XIA_GATE_COLLECT_HI 0
#define XIA_GATE_COLLECT_LO 1

/* Trigger and livetime signal output constants */
#define XIA_OUTPUT_DISABLED        0
#define XIA_OUTPUT_FASTFILTER      1
#define XIA_OUTPUT_BASELINEFILTER  2
#define XIA_OUTPUT_ENERGYFILTER    3
#define XIA_OUTPUT_ENERGYACTIVE    4

/* Old Test Constants */
#define XIA_HANDEL_TEST_MASK             0x1
#define XIA_HANDEL_DYN_MODULE_TEST_MASK  0x2
#define XIA_HANDEL_FILE_TEST_MASK        0x4
#define XIA_HANDEL_RUN_PARAMS_TEST_MASK  0x8
#define XIA_HANDEL_RUN_CONTROL_TEST_MASK 0x10
#define XIA_XERXES_TEST_MASK             0x20
#define XIA_HANDEL_SYSTEM_TEST_MASK      0x40

/* List mode variant */
#define XIA_LIST_MODE_SLOW_PIXEL 0.0
#define XIA_LIST_MODE_FAST_PIXEL 1.0
#define XIA_LIST_MODE_CLOCK      2.0

/* FalconXn detection filter */
#define XIA_FILTER_LOW_ENERGY     0 /**< Optimize for low energy pulses. */
#define XIA_FILTER_LOW_RATE       1 /**< Optimize for low rate pulses. */
#define XIA_FILTER_MID_RATE       2 /**< Optimize for mid rate pulses. */
#define XIA_FILTER_HIGH_RATE      3 /**< Optimize for high rate pulses. */
#define XIA_FILTER_MAX_THROUGHPUT 4 /**< Optimize for maximum throughput. */

/* FalconXn SCA trigger mode */
#define SCA_TRIGGER_OFF 	0 /**< Do not generate SCA pulses. */
#define SCA_TRIGGER_HIGH 	1 /**< Generate SCA pulses when the gate source is high. */
#define SCA_TRIGGER_LOW		2 /**< Generate SCA pulses when the gate source is low. */
#define SCA_TRIGGER_ALWAYS	3 /**< Generate SCA pulses always. */

/* FalconXn decay times */
#define XIA_DECAY_LONG       0.0
#define XIA_DECAY_MEDIUM     1.0
#define XIA_DECAY_SHORT      2.0
#define XIA_DECAY_VERY_SHORT 3.0

#define SCA_MAX_PULSER_DURATION 262140 /**< SCA generated pulse duration in nanoseconds. */

/* FalconXn SINC param readout string length */
#define XIA_DEBUG_PARAM_LEN 32

/* Supported board features returned by get_board_features board operations */
#define BOARD_SUPPORTS_NO_EXTRA_FEATURES 0x0;
/* FalconX features 0x11 - 0x18 */
#define BOARD_SUPPORTS_TERMINATAION_50OHM       0x11  /* Support for "50ohm" for termination */
#define BOARD_SUPPORTS_ATTENUATION_GROUND       0x12  /* Support for "Ground" for attenuation */


#endif /* HANDEL_CONSTANTS_H */
