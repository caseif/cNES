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

#include "util.h"

#include <stdbool.h>
#include <stddef.h>

struct cartridge;

#include "mappers/mappers.h"

typedef enum MirroringMode {MIRROR_HORIZONTAL, MIRROR_VERTICAL,
                            MIRROR_SINGLE_LOWER, MIRROR_SINGLE_UPPER} MirroringMode;

typedef struct cartridge {
    char *title;
    unsigned char *prg_rom;
    unsigned char *chr_rom;
    size_t prg_size; // in bytes
    size_t chr_size; // in bytes
    size_t prg_ram_size; // in bytes
    MirroringMode mirror_mode;
    bool has_nv_ram;
    bool ignore_mirror_ctrl;
    Mapper *mapper;
} Cartridge;
