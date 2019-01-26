/*
 * This file is a part of cNES.
 * Copyright (c) 2019, Max Roncace <mproncace@gmail.com>
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef WINDOWS
#include <windows.h>
#else
#define __USE_POSIX199309
#include <time.h>
#endif

#define PACKED __attribute__((packed))

typedef struct {
    unsigned char *data;
    size_t size;
} DataBlob;

static inline uint32_t endian_swap(uint32_t x) {
    return (x >> 24)               | 
          ((x << 8 ) & 0x00FF0000) |
          ((x >> 8 ) & 0x0000FF00) |
           (x << 24);
}

static inline void sleep_cp(int ms) {
    #ifdef WINDOWS
    Sleep(ms);
    #else
    nanosleep((struct timespec[]) {{0, ms * 1000000L}}, NULL);
    #endif
}
