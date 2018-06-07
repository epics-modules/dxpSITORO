/*
 * Copyright (c) 2017 XIA LLC
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of XIA LLC nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XIA_SIO_H
#define XIA_SIO_H

#include "xia_common.h"

typedef struct {
    boolean_t ro;    /* Read-only mode incurs no allocation. */
    size_t    next;  /* Next value to read. */
    size_t    level; /* Amount of data in the buffer. */
    size_t    size;  /* Amount of memory allocated. */
    union {
        char *s;
        const char *ro;
    } s;
} xia_sio;

int xia_sio_opens(xia_sio *io, const char *init);
int xia_sio_openro(xia_sio *io, const char *init);
int xia_sio_open(xia_sio *io, size_t size);
void xia_sio_close(xia_sio *io);

char *xia_sio_gets(xia_sio *io, char *dest, size_t max);
int xia_sio_printf(xia_sio *io, const char *fmt, ...) HANDEL_PRINTF(2,3);

size_t xia_sio_level(xia_sio *io);
size_t xia_sio_copy_out(xia_sio *io, char* dest, size_t size);

#endif /* XIA_SIO_H */
