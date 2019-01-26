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

#include "cpu/cpu.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCANLINE_COUNT 262
#define CYCLES_PER_SCANLINE 341
#define RESOLUTION_H 256
#define RESOLUTION_V 240

#define NAME_TABLE_TILE_LENGTH 8
#define NAME_TABLE_WIDTH (RESOLUTION_H / NAME_TABLE_TILE_LENGTH)
#define NAME_TABLE_HEIGHT (RESOLUTION_V / NAME_TABLE_TILE_LENGTH)

#define NAME_TABLE_BASE_ADDR 0x2000
#define NAME_TABLE_INTERVAL 0x400
#define NAME_TABLE_SIZE 0x3C0

#define ATTR_TABLE_TILE_LENGTH 32
#define ATTR_TABLE_WIDTH (RESOLUTION_H / ATTR_TABLE_TILE_LENGTH)
#define ATTR_TABLE_HEIGHT (RESOLUTION_V / ATTR_TABLE_TILE_LENGTH)

typedef union {
    struct sections {
        unsigned char top_left:2 PACKED;
        unsigned char top_right:2 PACKED;
        unsigned char bottom_left:2 PACKED;
        unsigned char bottom_right:2 PACKED;
    } sections;
    uint8_t serial;
} AttributeTableEntry;

static MirroringMode g_mirror_mode;

PpuControl g_ppu_control;
PpuMask g_ppu_mask;
PpuStatus g_ppu_status;
PpuInternalRegisters g_ppu_internal_regs;

unsigned char g_pattern_table_left[0x1000];
unsigned char g_pattern_table_right[0x1000];
unsigned char g_name_table_mem[0x800];

static uint64_t g_frame;
static uint16_t g_scanline;
static uint16_t g_scanline_tick;

void initialize_ppu(MirroringMode mirror_mode) {
    g_mirror_mode = mirror_mode;
    
    memset(&g_ppu_control, '\0', 1);
    memset(&g_ppu_mask, '\0', 1);
    
    g_frame = 0;
    g_scanline = 0;
    g_scanline_tick = 0;
}

uint8_t read_ppu_mmio(uint8_t index) {
    assert(index <= 7);

    switch (index) {
        case 2: {
            uint8_t res;
            memcpy(&res, &g_ppu_status, 1);
            return res;
        }
        case 7: {
            //TODO: VRAM access
            return 0;
        }
        default: {
            //TODO: I think it returns a latch value here
            return 0;
        }
    }
}

void write_ppu_mmio(uint8_t index, uint8_t val) {
    assert(index <= 7);

    switch (index) {
        case 0:
            memcpy(&g_ppu_control, &val, 1);
            return;
        case 1:
            memcpy(&g_ppu_mask, &val, 1);
            return;
        case 2:
            //TODO: not sure what to do here
            return;
        case 3:
            //TODO: sprite RAM address
            return;
        case 4:
            //TODO: sprite RAM data
            return;
        case 5:
            //TODO: VRAM address 1
            return;
        case 6:
            //TODO: VRAM address 2
            return;
        case 7: {
            //TODO: VRAM access
            return;
        }
    }
}

uint16_t _translate_name_table_address(uint16_t addr) {
    assert(addr <= 0xFFF);

    switch (addr) {
        // name table 0
        case 0x000 ... 0x3FF: {
            // it's always in the first half
            return addr;
        }
        // name table 1
        case 0x400 ... 0x7FF: {
            if (g_mirror_mode == MIRROR_VERTICAL) {
                // no need for translation
                return addr;
            } else {
                // look up the data in name table 0 since name table 1 is a mirror
                return addr - 0x2400;
            }
        }
        // name table 2
        case 0x800 ... 0xBFF: {
            if (g_mirror_mode == MIRROR_HORIZONTAL) {
                // use the second half of the memory
                return addr - 400;
            } else {
                // use name table 0 since name table 2 is a mirror
                return addr - 800;
            }
        }
        // name table 3
        case 0xC00 ... 0xFFF: {
            // it's always in the second half
            return addr - 0x800;
        }
        default: {
            exit(-1); // shouldn't happen
        }
    }
}

uint8_t ppu_memory_read(uint16_t addr) {
    switch (addr) {
        // pattern table left
        case 0x0000 ... 0x0FFF: {
            return g_pattern_table_left[addr];
        }
        // pattern table right
        case 0x1000 ... 0x1FFF: {
            return g_pattern_table_right[addr - 0x1000];
        }
        // name table 0
        case 0x2000 ... 0x2FFF: {
            return g_name_table_mem[_translate_name_table_address(addr - 0x2000)];
        }
        // unmapped
        default: {
            return 0;
        }
    }
}

void ppu_memory_write(uint16_t addr, uint8_t val) {
    switch (addr) {
        // pattern table left
        case 0x0000 ... 0x0FFF: {
            g_pattern_table_left[addr] = val;
        }
        // pattern table right
        case 0x1000 ... 0x1FFF: {
            g_pattern_table_right[addr - 0x1000] = val;
        }
        // name table 0
        case 0x2000 ... 0x2FFF: {
            g_name_table_mem[_translate_name_table_address(addr - 0x2000)] = val;
        }
        // unmapped
        default: {
            return;
        }
    }
}

void cycle_ppu(void) {
    switch (g_scanline) {
        // first tick is "skipped" on odd frames
        case 0:
            if (g_frame % 2 == 1 && g_scanline_tick == 0) {
                g_scanline_tick = 1;
            }
            // intentional fall-through to visible screen ticking
        // visible screen
        case 1 ... 239: {
            if (g_scanline_tick >= 257 && g_scanline_tick <= 320) {
                //TODO: perform sprite tile fetching
            } else {
                unsigned int fetch_pixel_x;
                unsigned int fetch_pixel_y;

                if ((g_scanline_tick >= 1 && g_scanline_tick <= 256)) {
                    fetch_pixel_x = g_scanline_tick + 15; // we fetch two tiles ahead
                    fetch_pixel_y = g_scanline;
                } else if (g_scanline_tick <= 336) { // start fetching for the next scanline
                    fetch_pixel_x = g_scanline_tick - 321; // fetching starts at cycle 321
                    fetch_pixel_y = g_scanline + 1; // we're on the next line
                } else {
                    // these cycles are for garbage NT fetches
                    break;
                }
                
                switch ((g_scanline_tick - 1) % 8) {
                    // fetch name table entry
                    case 1: {
                        uint8_t nt_h_off = fetch_pixel_x / 8;
                        uint8_t nt_v_off = fetch_pixel_y / 8;

                        //TODO
                        uint8_t scroll_x = 0;
                        uint8_t scroll_y = 0;

                        uint8_t nt_index = g_ppu_control.name_table;

                        if (nt_h_off + scroll_x > NAME_TABLE_WIDTH) {
                            nt_index ^= 0b01; // flip the LSB
                        }

                        if (nt_v_off + scroll_y > NAME_TABLE_HEIGHT) {
                            nt_index ^= 0b10; // flip the MSB
                        }

                        uint8_t real_nt_h_off = (nt_h_off + scroll_x) % NAME_TABLE_WIDTH;
                        uint8_t real_nt_v_off = (nt_v_off + scroll_y) % NAME_TABLE_HEIGHT;

                        uint16_t name_table_addr = NAME_TABLE_BASE_ADDR + (nt_index * NAME_TABLE_INTERVAL)
                                + (real_nt_v_off * 32) + real_nt_h_off;

                        assert(name_table_addr < 0x3000);

                        g_ppu_internal_regs.name_table_entry_latch = ppu_memory_read(name_table_addr);

                        break;
                    }
                    case 3: {
                        //TODO: fetch attribute byte

                        break;
                    }
                    default: {
                        //TODO

                        break;
                    }
                }
            }

            break;
        }
        // set vblank flag
        case 241: {
            if (g_scanline_tick == 1) {
                g_ppu_status.vblank = 1;
                issue_interrupt(INT_NMI);
            }

            break;
        }
        // pre-render line
        case 261: {
            // clear status
            if (g_scanline_tick == 1) {
                g_ppu_status.vblank = 0;
                g_ppu_status.sprite_0_hit = 0;
                g_ppu_status.sprite_overflow = 0;
            }
        }
    }

    if (++g_scanline_tick >= CYCLES_PER_SCANLINE) {
        g_scanline_tick = 0;

        if (++g_scanline >= SCANLINE_COUNT) {
            g_scanline = 0;

            g_frame++;

            printf("frame %ld\n", g_frame);
        }
    }
}
