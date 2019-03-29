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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef WINDOWS
#include <windows.h>
#else
#ifndef __USE_POSIX199309
#define __USE_POSIX199309
#endif
#include <time.h>
#endif

#define PACKED __attribute__((packed))

#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))

typedef struct {
    unsigned char *data;
    size_t size;
} DataBlob;

typedef struct linked_list_t {
    void *value;
    struct linked_list_t *next;
} LinkedList;

static inline void add_to_linked_list(LinkedList *list, void *value) {
    while (list->next != NULL) {
        list = list->next;
    }
    list->next = (LinkedList*) malloc(sizeof(LinkedList));
    list->next->value = value;
}

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
    struct timespec spec[] = {{0, ms * 1000000L}};
    nanosleep(spec, NULL);
    #endif
}

static inline unsigned char reverse_bits(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
