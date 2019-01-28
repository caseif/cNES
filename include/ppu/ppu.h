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

typedef enum {MIRROR_HORIZONTAL, MIRROR_VERTICAL} MirroringMode;

typedef struct {
    unsigned char name_table:2 PACKED;
    unsigned char vertical_increment:1 PACKED;
    unsigned char sprite_table:1 PACKED;
    unsigned char background_table:1 PACKED;
    unsigned char tall_sprites:1 PACKED;
    unsigned char ext_master:1 PACKED;
    unsigned char gen_nmis:1 PACKED;
} PpuControl;

typedef struct {
    unsigned char grayscale:1 PACKED;
    unsigned char clip_background:1 PACKED;
    unsigned char clip_sprites:1 PACKED;
    unsigned char show_background:1 PACKED;
    unsigned char show_sprites:1 PACKED;
    unsigned char em_red:1 PACKED;
    unsigned char em_green:1 PACKED;
    unsigned char em_blue:1 PACKED;
} PpuMask;

typedef struct {
    unsigned char :5 PACKED;
    unsigned char sprite_overflow:1 PACKED;
    unsigned char sprite_0_hit:1 PACKED;
    unsigned char vblank:1 PACKED;
} PpuStatus;

typedef struct {
    uint16_t v;  // current VRAM address
    uint16_t t;  // temporary VRAM address
    uint8_t x;   // fine x-scroll
    uint8_t w:1; // write flag (for twice-writable registers)

    uint8_t name_table_entry_latch;
    unsigned int attr_table_entry_latch:2;

    uint8_t pattern_bitmap_l_latch;
    uint8_t pattern_bitmap_h_latch;

    uint16_t pattern_shift_l;
    uint16_t pattern_shift_h;
    uint16_t palette_shift_l;
    uint16_t palette_shift_h;
} PpuInternalRegisters;

void initialize_ppu(MirroringMode mirror_mode);

uint8_t read_ppu_mmio(uint8_t index);

void write_ppu_mmio(uint8_t addr, uint8_t val);

uint8_t ppu_memory_read(uint16_t addr);

void ppu_memory_write(uint16_t addr, uint8_t val);

void cycle_ppu(void);
