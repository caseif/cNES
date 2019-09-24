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

#include "cartridge.h"

#include <stdint.h>

#define MAPPER_ID_NROM 0
#define MAPPER_ID_MMC1 1
#define MAPPER_ID_UNROM 2
#define MAPPER_ID_CNROM 3
#define MAPPER_ID_MMC3 4
#define MAPPER_ID_AXROM 7
#define MAPPER_ID_COLOR_DREAMS 11

typedef void (*MapperInitFunction)(struct cartridge *cart);
typedef uint8_t (*MemoryReadFunction)(struct cartridge *cart, uint16_t);
typedef void (*MemoryWriteFunction)(struct cartridge *cart, uint16_t, uint8_t);
typedef void (*MapperTickFunction)(void);

typedef struct {
    unsigned int id;
    char name[12];
    MapperInitFunction init_func;
    MemoryReadFunction ram_read_func;
    MemoryWriteFunction ram_write_func;
    MemoryReadFunction vram_read_func;
    MemoryWriteFunction vram_write_func;
    MapperTickFunction tick_func;
} Mapper;

void mapper_init_nrom(Mapper *mapper, unsigned int submapper_id);

void mapper_init_mmc1(Mapper *mapper, unsigned int submapper_id);

void mapper_init_unrom(Mapper *mapper, unsigned int submapper_id);

void mapper_init_cnrom(Mapper *mapper, unsigned int submapper_id);

void mapper_init_mmc3(Mapper *mapper, unsigned int submapper_id);

void mapper_init_axrom(Mapper *mapper, unsigned int submapper_id);

void mapper_init_color_dreams(Mapper *mapper, unsigned int submapper_id);
