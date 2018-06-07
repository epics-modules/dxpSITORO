/* printf/gets operating on in-memory strings
 *
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

#include <stdarg.h>

#include "handel_errors.h"
#include "xia_handel.h" /* alloc/free */

#include "xia_sio.h"

/* Initialize a r/w buffer pre-allocated to size. */
int xia_sio_open(xia_sio *io, size_t size)
{
    io->ro = FALSE_;

    io->s.s = handel_md_alloc(size);
    if (!io->s.s) {
        return XIA_NOMEM;
    }

    io->size = size;
    io->level = io->next = 0;

    return XIA_SUCCESS;
}

/* Initialize a r/w buffer with a copy of the string given in init. */
int xia_sio_opens(xia_sio *io, const char *init)
{
    size_t size = strlen(init) + 1;

    int status = xia_sio_open(io, size);
    if (status != XIA_SUCCESS)
        return status;

    strcpy(io->s.s, init);

    io->level = size;

    return XIA_SUCCESS;
}

/* Initialize a read-only buffer with the given contents. This is a
 * zero-allocation version intended for quick line-by-line scanning
 * with xia_sio_gets. */
int xia_sio_openro(xia_sio *io, const char *init)
{
    size_t size = strlen(init) + 1;

    io->ro = TRUE_;
    io->s.ro = init;
    io->size = io->level = size;
    io->next = 0;

    return XIA_SUCCESS;
}

void xia_sio_close(xia_sio *io)
{
    if (io->s.s) {
        handel_md_free(io->s.s);
        io->s.s = NULL;
    }

    io->s.ro = NULL;
}

/* Like gets or fgets, reads from the buffer until max-1
 * characters are consumed or a \n or \r\n is found, copying into
 * dest. Newline characters are consumed from io but not copied into
 * dest, for compatibility with md_fgets. */
char *xia_sio_gets(xia_sio *io, char *dest, size_t max)
{
    const char *s = io->ro ? io->s.ro : io->s.s;

    if (s[io->next] == '\0')
        return NULL;

    size_t end = strcspn(s + io->next, "\n\r");

    if (end >= max)
        end = max - 1;

    strncpy(dest, s + io->next, end);
    dest[end] = '\0';

    io->next += end;
    if (s[io->next] != '\0')
        io->next += strspn(s + io->next, "\n\r");

    return dest;
}

/* Printf to the buffer. This will reallocate memory as needed and
 * return -XIA_NOMEM if allocation fails.
 *
 * Returns the number of characters written on success and the
 * negative of an XIA error code otherwise. */
int xia_sio_printf(xia_sio *io, const char *fmt, ...)
{
    va_list args;

    size_t remain;
    int written;

    if (io->ro) {
        return -XIA_READ_ONLY;
    }

    va_start(args, fmt);

    do {
        remain = io->size - io->level;
        written = vsnprintf(io->s.s + io->level, remain, fmt, args);

        if (written < 0) {
            va_end(args);
            return -XIA_ENCODE;
        }

        if ((size_t) written + 1 > remain) {
            size_t newLen = io->size * 2;
            char *re = realloc(io->s.s, newLen);
            if (!re) {
                va_end(args);
                return -XIA_NOMEM;
            }

            memset(re + io->size, 0, newLen - io->size);

            io->s.s = re;
            io->size = newLen;
        }
    } while ((size_t) written + 1 > remain);

    io->level += (size_t) written;

    va_end(args);

    return written;
}

/* Returns the amount of data in the buffer. */
size_t xia_sio_level(xia_sio *io)
{
    return io->level;
}

/* Copies the buffer from the current read pointer (next) into dest. */
size_t xia_sio_copy_out(xia_sio *io, char* dest, size_t size)
{
    const char *s = io->ro ? io->s.ro : io->s.s;

    if (size == 0)
        return 0;

    if (io->next + size > io->level)
        size = io->level - io->next;

    strncpy(dest, s, size);

    io->next += size;

    return size;
}
