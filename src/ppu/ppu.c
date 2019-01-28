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

#include "renderer.h"
#include "cpu/cpu.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCANLINE_COUNT 262
#define CYCLES_PER_SCANLINE 341

#define NAME_TABLE_GRANULARITY 8
#define NAME_TABLE_WIDTH (RESOLUTION_H / NAME_TABLE_GRANULARITY)
#define NAME_TABLE_HEIGHT (RESOLUTION_V / NAME_TABLE_GRANULARITY)

#define NAME_TABLE_BASE_ADDR 0x2000
#define NAME_TABLE_INTERVAL 0x400
#define NAME_TABLE_SIZE 0x3C0

#define ATTR_TABLE_GRANULARITY 32
#define ATTR_TABLE_WIDTH (RESOLUTION_H / ATTR_TABLE_GRANULARITY)
#define ATTR_TABLE_HEIGHT (RESOLUTION_V / ATTR_TABLE_GRANULARITY)

#define ATTR_TABLE_BASE_ADDR 0x23C0

#define PT_LEFT_ADDR 0x0000
#define PT_RIGHT_ADDR 0x1000

#define PALETTE_DATA_BASE_ADDR 0x3F00

typedef union {
    struct sections {
        unsigned char top_left:2 PACKED;
        unsigned char top_right:2 PACKED;
        unsigned char bottom_left:2 PACKED;
        unsigned char bottom_right:2 PACKED;
    } sections;
    uint8_t serial;
} AttributeTableEntry;

static Cartridge *g_cartridge;

static MirroringMode g_mirror_mode;

PpuControl g_ppu_control;
PpuMask g_ppu_mask;
PpuStatus g_ppu_status;
PpuInternalRegisters g_ppu_internal_regs;

unsigned char g_pattern_table_left[0x1000];
unsigned char g_pattern_table_right[0x1000];
unsigned char g_name_table_mem[0x800];
unsigned char g_palette_ram[0x20];

static uint64_t g_frame;
static uint16_t g_scanline;
static uint16_t g_scanline_tick;

void initialize_ppu(Cartridge *cartridge, MirroringMode mirror_mode) {
    g_cartridge = cartridge;

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
            // return the status
            uint8_t res;
            memcpy(&res, &g_ppu_status, 1);
            return res;
        }
        case 7: {
            // read from stored address
            return ppu_memory_read(g_ppu_internal_regs.v);
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
            break;
        case 1:
            memcpy(&g_ppu_mask, &val, 1);
            break;
        case 2:
            //TODO: not sure what to do here
            break;
        case 3:
            //TODO: sprite RAM address
            break;
        case 4:
            //TODO: sprite RAM data
            break;
        case 5:
            // set either x- or y-scroll, depending on whether this is the first or second write
            if (g_ppu_internal_regs.w) {
                g_ppu_internal_regs.scroll_y = val;
            } else {
                g_ppu_internal_regs.scroll_x = val;
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;
            break;
        case 6:
            // set either the upper or lower address bits, depending on which write this is
            if (g_ppu_internal_regs.w) {
                // clear lower bits
                g_ppu_internal_regs.v &= 0xFF00;
                // set upper bits
                g_ppu_internal_regs.v |= val;
            } else {
                // clear upper bits
                g_ppu_internal_regs.v &= 0xFF;
                // set upper bits
                g_ppu_internal_regs.v |= val << 8;
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;

            break;
        case 7: {
            // write to the stored address
            ppu_memory_write(g_ppu_internal_regs.v, val);
            g_ppu_internal_regs.v++;

            break;
        }
        default:
            assert(false);
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
        case 0x0000 ... 0x1FFF: {
            return g_cartridge->chr_rom[addr];
        }
        // name table 0
        case 0x2000 ... 0x3EFF: {
            return g_name_table_mem[_translate_name_table_address((addr - 0x2000) % 0x1000)];
        }
        case 0x3F00 ... 0x3FFF: {
            unsigned int index = (addr - 0x3F00) % 0x20;

            // certain indices are just mirrors
            switch (index) {
                case 0x10:
                case 0x14:
                case 0x18:
                case 0x1C:
                    index -= 0x10;
                    break;
            }

            return g_palette_ram[index];
        }
        // unmapped
        default: {
            return 0;
        }
    }
}

void ppu_memory_write(uint16_t addr, uint8_t val) {
    //printf("writing %02x to addr %04x\n", val, addr);
    switch (addr) {
        // pattern table left
        case 0x0000 ... 0x0FFF: {
            //TODO: unsupported altogether for now
            break;
        }
        // name table 0
        case 0x2000 ... 0x3EFF: {
                        //TODO: fetch attribute byte
            g_name_table_mem[_translate_name_table_address((addr - 0x2000) % 0x1000)] = val;
            break;
        }
        case 0x3F00 ... 0x3FFF: {
            unsigned int index = (addr - 0x3F00) % 0x20;

            // certain indices are just mirrors
            switch (index) {
                case 0x10:
                case 0x14:
                case 0x18:
                case 0x1C:
                    index -= 0x10;
                    break;
            }

            g_palette_ram[index] = val;
            break;
        }
        // unmapped
        default: {
            return;
        }
    }
}

static uint16_t _compute_table_addr(unsigned int pix_x, unsigned int pix_y, unsigned int width, unsigned int height,
        unsigned int granularity, uint16_t base_addr) {
    uint8_t h_off = pix_x / granularity;
    uint8_t v_off = pix_y / granularity;

    uint8_t scroll_x = g_ppu_internal_regs.scroll_x;
    uint8_t scroll_y = g_ppu_internal_regs.scroll_y;

    uint8_t table_index = g_ppu_control.name_table;

    // figure out if we overflowed on the x-axis
    if (h_off + scroll_x > width) {
        table_index ^= 0b01; // flip the LSB
    }

    // figure out if we overflowed on the y-axis
    if (v_off + scroll_y > height) {
        table_index ^= 0b10; // flip the MSB
    }

    uint8_t real_h_off = (h_off + scroll_x) % width;
    uint8_t real_v_off = (v_off + scroll_y) % height;

    uint16_t table_addr = base_addr + (table_index * NAME_TABLE_INTERVAL)
            + (real_v_off * width) + real_h_off;

    assert(table_addr < 0x3000);

    return table_addr;
}

void cycle_ppu(void) {
    if (g_scanline == 0 && g_scanline_tick == 0 && g_frame % 2 == 1) {
        g_scanline_tick = 1;
    }

    switch (g_scanline) {
        // visible screen
        case 0 ... 239: {
            if (g_scanline_tick == 0) {
                // idle tick
                break;
            } else if (g_scanline_tick >= 257 && g_scanline_tick <= 320) {
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
                    case 0: {
                        // flush the palette select data into the primary shift registers
                        g_ppu_internal_regs.palette_shift_l = g_ppu_internal_regs.attr_table_entry_latch & 1;
                        g_ppu_internal_regs.palette_shift_h = g_ppu_internal_regs.attr_table_entry_latch >> 1;

                        // flush the pattern bitmap latches into the upper halves of the primary shift registers

                        // clear upper bits
                        g_ppu_internal_regs.pattern_shift_l &= 0xFF;
                        // set upper bits
                        g_ppu_internal_regs.pattern_shift_l |= g_ppu_internal_regs.pattern_bitmap_l_latch << 8;
                        
                        // clear upper bits
                        g_ppu_internal_regs.pattern_shift_h &= 0xFF;
                        // set upper bits
                        g_ppu_internal_regs.pattern_shift_h |= g_ppu_internal_regs.pattern_bitmap_h_latch << 8;

                        break;
                    }
                    // fetch name table entry
                    case 1: {
                        uint16_t name_table_addr = _compute_table_addr(fetch_pixel_x, fetch_pixel_y, NAME_TABLE_WIDTH,
                                NAME_TABLE_HEIGHT, NAME_TABLE_GRANULARITY, NAME_TABLE_BASE_ADDR);

                        g_ppu_internal_regs.name_table_entry_latch = ppu_memory_read(name_table_addr);

                        break;
                    }
                    case 3: {
                        uint16_t attr_table_addr = _compute_table_addr(fetch_pixel_x, fetch_pixel_y, ATTR_TABLE_WIDTH,
                                ATTR_TABLE_HEIGHT, ATTR_TABLE_GRANULARITY, ATTR_TABLE_BASE_ADDR);

                        uint8_t attr_table_byte = ppu_memory_read(attr_table_addr);

                        // check if it's in the bottom half of the table cell
                        if ((fetch_pixel_y % 32) >= 16) {
                            attr_table_byte >>= 4;
                        }

                        // check if it's in the right half of the table cell
                        if ((fetch_pixel_x % 32) >= 16) {
                            attr_table_byte >>= 2;
                        }

                        g_ppu_internal_regs.attr_table_entry_latch = attr_table_byte & 0b11;

                        break;
                    }
                    case 5: {
                        // multiply by 8 since each plane is 8 bytes, and 2 since there are two planes per tile
                        // for a total of 16 consecutive bytes per tile
                        // then we just add the mod of the current line to get the sub-tile offset
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + (fetch_pixel_y % 8);

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_l_latch = ppu_memory_read(pattern_addr);

                        break;
                    }
                    case 7: {
                        // basically the same as above, but we add 8 to get the second plane
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + (fetch_pixel_y % 8) + 8;

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_h_latch = ppu_memory_read(pattern_addr);

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
                if (g_ppu_control.gen_nmis) {
                    issue_interrupt(INT_NMI);
                }
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

    if (g_scanline < RESOLUTION_V && g_scanline_tick < RESOLUTION_H) {
            unsigned int palette_low = ((g_ppu_internal_regs.pattern_shift_h << 1) & 1)
                    | (g_ppu_internal_regs.pattern_shift_l & 1);
            
            unsigned int palette_offset;

            if (palette_low) {
                // if the palette low bits are not zero, we select the color normally
                unsigned int palette_high = ((g_ppu_internal_regs.palette_shift_h & 1) << 1)
                        | (g_ppu_internal_regs.palette_shift_l & 1);
                palette_offset = (palette_high << 2) + palette_low;
            } else {
                // otherwise, we use the default background color
                palette_offset = 0;
            }

            uint16_t palette_entry_addr = PALETTE_DATA_BASE_ADDR + palette_offset;
            set_pixel(g_scanline_tick, g_scanline, ppu_memory_read(palette_entry_addr));

            // shift the internal registers
            g_ppu_internal_regs.pattern_shift_h >>= 1;
            g_ppu_internal_regs.pattern_shift_l >>= 1;
            g_ppu_internal_regs.palette_shift_h >>= 1;
            g_ppu_internal_regs.palette_shift_l >>= 1;
    }

    if (++g_scanline_tick >= CYCLES_PER_SCANLINE) {
        g_scanline_tick = 0;

        if (++g_scanline >= SCANLINE_COUNT) {
            g_scanline = 0;

            g_frame++;

            flush_frame();
        }
    }
}
