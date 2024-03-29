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

#pragma warning(disable: 4201)
#pragma warning(disable: 4214)

#include <stdbool.h>
#include <stdint.h>

#define RESOLUTION_H 256
#define RESOLUTION_V 240

// ~600 ms
#define PPU_BUS_DECAY_CYCLES 3220000

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBValue;
   
#pragma pack(push,1)

typedef union {
    struct {
        unsigned char name_table:2;
        unsigned char vertical_increment:1;
        unsigned char sprite_table:1;
        unsigned char background_table:1;
        unsigned char tall_sprites:1;
        unsigned char ext_master:1;
        unsigned char gen_nmis:1;
    };
    uint8_t serial;
} PpuControl;

typedef union {
    struct {
        unsigned char monochrome:1;
        unsigned char show_background_left:1;
        unsigned char show_sprites_left:1;
        unsigned char show_background:1;
        unsigned char show_sprites:1;
        unsigned char em_red:1;
        unsigned char em_green:1;
        unsigned char em_blue:1;
    };
    uint8_t serial;
} PpuMask;

typedef union {
    struct {
        unsigned char :5; // unused
        unsigned char sprite_overflow:1;
        unsigned char sprite_0_hit:1;
        unsigned char vblank:1;
    };
    uint8_t serial;
} PpuStatus;

typedef struct {
    unsigned int palette_index:2;
    unsigned int :3; // unused
    unsigned int low_priority:1;
    unsigned int flip_hor:1;
    unsigned int flip_ver:1;
} SpriteAttributes;

typedef struct {
    uint8_t y;
    union {
        uint8_t tile_num;
    };
    union {
        SpriteAttributes attrs;
        uint8_t attrs_serial;
    };
    uint8_t x;
} Sprite;

typedef union {
    struct {
        unsigned int x_coarse:5;
        unsigned int y_coarse:5;
        unsigned int nt:2;
        unsigned int y_fine:3;
    };
    unsigned int addr:15;
} VramAddr;

#pragma pack(pop)

typedef struct {
    VramAddr v;  // current VRAM address
    VramAddr t;  // temporary VRAM address
    unsigned int x:8;   // fine x-scroll
    unsigned int w:1;   // write flag (for twice-writable registers)

    uint8_t s;         // sprite address

    unsigned int m:3;   // sprite data index, used for sprite evaluation
    unsigned int n:7;   // primary OAM index, used for sprite evaluation
    unsigned int o:4;   // secondary OAM index, used for sprite evaluation/tile fetching
    unsigned int p:8;   // (imaginary) current offset into OAM RAM
    uint8_t sprite_attr_latch; // latch for sprite during evaluation
    bool has_latched_sprite;   // whether a byte is latched
    uint8_t loaded_sprites; // number of currently loaded sprites
    uint8_t sprite_0_next_scanline; // whether sprite 0 is on the next scanline
    uint8_t sprite_0_scanline; // whether sprite 0 is on the current scanline

    uint8_t sprite_tile_index_latch; // stores the tile index during fetching
    uint8_t sprite_y_latch; // stores the sprite y-position during fetching
    SpriteAttributes sprite_attr_latches[8]; // latches for sprite attribute data
    uint8_t sprite_x_counters[8]; // counters for sprite x-positions
    // there's no analog to this in hardware (all sprite fetches after the first 8 pixels return 0, or transparent),
    // but we introduce it here to improve efficiency slightly and aid with debugging (so that sprites only "exist" to
    // the render routine while they're actually being rendered)
    uint8_t sprite_death_counters[8];
    uint8_t sprite_tile_shift_l[8]; // shift registers for sprite tile data
    uint8_t sprite_tile_shift_h[8]; // shift registers for sprite tile data

    uint8_t read_buf;

    uint16_t addr_bus;
    uint8_t name_table_entry_latch;
    unsigned int attr_table_entry_latch:2;
    unsigned int attr_table_entry_latch_secondary:2;

    uint8_t pattern_bitmap_l_latch;
    uint8_t pattern_bitmap_h_latch;

    uint16_t pattern_shift_l;
    uint16_t pattern_shift_h;
    uint8_t palette_shift_l;
    uint8_t palette_shift_h;

    uint8_t ppu_bus;
    uint32_t ppu_bus_decay_timers[8];
} PpuInternalRegisters;

typedef enum {
    RM_NORMAL, RM_NT0, RM_NT1, RM_NT2, RM_NT3, RM_PT
} RenderMode;

typedef enum MirroringMode { MIRROR_HORIZONTAL, MIRROR_VERTICAL,
                             MIRROR_SINGLE_LOWER, MIRROR_SINGLE_UPPER,
                             MIRROR_FOUR_SCREEN } MirroringMode;

void initialize_ppu(void);

void ppu_set_mirroring_mode(MirroringMode mirror_mode);

bool ppu_is_rendering_enabled(void);

uint16_t ppu_get_scanline(void);

uint16_t ppu_get_scanline_tick(void);

bool ppu_get_swap_pattern_tables(void);

PpuInternalRegisters *ppu_get_internal_regs(void);

uint8_t ppu_read_mmio(uint8_t index);

void ppu_write_mmio(uint8_t addr, uint8_t val);

uint8_t ppu_name_table_read(uint16_t addr);

void ppu_name_table_write(uint16_t addr, uint8_t val);

uint8_t ppu_palette_table_read(uint8_t index);

void ppu_palette_table_write(uint8_t index, uint8_t val);

void ppu_push_dma_byte(uint8_t val);

void cycle_ppu(void);

RenderMode get_render_mode(void);

void set_render_mode(RenderMode mode);

void dump_vram(void);

void dump_oam(void);
