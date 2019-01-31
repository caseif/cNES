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
#include "util.h"
#include "cpu/cpu.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCANLINE_COUNT 262
#define CYCLES_PER_SCANLINE 341

#define NAME_TABLE_GRANULARITY 8
#define NAME_TABLE_WIDTH (RESOLUTION_H / (double) NAME_TABLE_GRANULARITY)
#define NAME_TABLE_HEIGHT (RESOLUTION_V / (double) NAME_TABLE_GRANULARITY)

#define NAME_TABLE_BASE_ADDR 0x2000
#define NAME_TABLE_INTERVAL 0x400
#define NAME_TABLE_SIZE 0x3C0

#define ATTR_TABLE_GRANULARITY 32
#define ATTR_TABLE_WIDTH (RESOLUTION_H / (double) ATTR_TABLE_GRANULARITY)
#define ATTR_TABLE_HEIGHT (RESOLUTION_V / (double) ATTR_TABLE_GRANULARITY)

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

const RGBValue g_palette[] = {
    {0x7C, 0x7C, 0x7C}, {0x00, 0x00, 0xFC}, {0x00, 0x00, 0xBC}, {0x44, 0x28, 0xBC},
    {0x94, 0x00, 0x84}, {0xA8, 0x00, 0x20}, {0xA8, 0x10, 0x00}, {0x88, 0x14, 0x00},
    {0x50, 0x30, 0x00}, {0x00, 0x78, 0x00}, {0x00, 0x68, 0x00}, {0x00, 0x58, 0x00},
    {0x00, 0x40, 0x58}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xBC, 0xBC, 0xBC}, {0x00, 0x78, 0xF8}, {0x00, 0x58, 0xF8}, {0x68, 0x44, 0xFC},
    {0xD8, 0x00, 0xCC}, {0xE4, 0x00, 0x58}, {0xF8, 0x38, 0x00}, {0xE4, 0x5C, 0x10},
    {0xAC, 0x7C, 0x00}, {0x00, 0xB8, 0x00}, {0x00, 0xA8, 0x00}, {0x00, 0xA8, 0x44},
    {0x00, 0x88, 0x88}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xF8, 0xF8, 0xF8}, {0x3C, 0xBC, 0xFC}, {0x68, 0x88, 0xFC}, {0x98, 0x78, 0xF8},
    {0xF8, 0x78, 0xF8}, {0xF8, 0x58, 0x98}, {0xF8, 0x78, 0x58}, {0xFC, 0xA0, 0x44},
    {0xF8, 0xB8, 0x00}, {0xB8, 0xF8, 0x18}, {0x58, 0xD8, 0x54}, {0x58, 0xF8, 0x98},
    {0x00, 0xE8, 0xD8}, {0x78, 0x78, 0x78}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFC, 0xFC, 0xFC}, {0xA4, 0xE4, 0xFC}, {0xB8, 0xB8, 0xF8}, {0xD8, 0xB8, 0xF8},
    {0xF8, 0xB8, 0xF8}, {0xF8, 0xA4, 0xC0}, {0xF0, 0xD0, 0xB0}, {0xFC, 0xE0, 0xA8},
    {0xF8, 0xD8, 0x78}, {0xD8, 0xF8, 0x78}, {0xB8, 0xF8, 0xB8}, {0xB8, 0xF8, 0xD8},
    {0x00, 0xFC, 0xFC}, {0xF8, 0xD8, 0xF8}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

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
Sprite g_oam_ram[0x40];

static bool g_odd_frame;
static uint16_t g_scanline;
static uint16_t g_scanline_tick;

static inline bool _is_rendering_enabled(void) {
    return g_ppu_mask.show_background || g_ppu_mask.show_sprites;
}

void initialize_ppu(Cartridge *cartridge, MirroringMode mirror_mode) {
    g_cartridge = cartridge;

    g_mirror_mode = mirror_mode;
    
    g_ppu_control = (PpuControl) {0};
    g_ppu_mask = (PpuMask) {0};

    memset(g_name_table_mem, 0xFF, 0x1000);
    memset(g_palette_ram, 0xFF, 0x20);
    memset(g_oam_ram, 0xFF, sizeof(Sprite) * 0x40);
    
    g_odd_frame = false;
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

            // reading this register resets this latch
            g_ppu_internal_regs.w = 0;
            // and the vblank flag
            g_ppu_status.vblank = 0;

            return res;
        }
        case 7: {
            if (g_ppu_internal_regs.v < 0x3F00) {
                // most VRAM goes through a read buffer
                uint8_t res = g_ppu_internal_regs.read_buf;

                g_ppu_internal_regs.read_buf = ppu_memory_read(g_ppu_internal_regs.v);

                g_ppu_internal_regs.v += g_ppu_control.vertical_increment ? 32 : 1;
                
                return res;
            } else {
                // palette reading bypasses buffer entirely, but still updates it

                // address is offset due to mirroring
                g_ppu_internal_regs.read_buf = ppu_memory_read(g_ppu_internal_regs.v - 0x1000);

                return ppu_memory_read(g_ppu_internal_regs.v);
            }

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
        case 0: {
            bool old_gen_nmis = g_ppu_control.gen_nmis;

            memcpy(&g_ppu_control, &val, 1);
            g_ppu_internal_regs.t &= ~(0b11 << 10); // clear bits 10-11
            g_ppu_internal_regs.t |= val & 0b11; // set bits 10-11 to current nametable

            if (!old_gen_nmis && g_ppu_control.gen_nmis && g_ppu_status.vblank) {
                issue_interrupt(INT_NMI);
            }

            break;
        }
        case 1:
            memcpy(&g_ppu_mask, &val, 1);
            break;
        case 2:
            //TODO: not sure what to do here
            break;
        case 3:
            g_ppu_internal_regs.s = val;
            break;
        case 4:
            ((char*) g_oam_ram)[g_ppu_internal_regs.s] = val;
            break;
        case 5:
            // set either x- or y-scroll, depending on whether this is the first or second write
            if (g_ppu_internal_regs.w) {
                g_ppu_internal_regs.t &= ~(0b11111 << 5); // clear bits 5-9
                g_ppu_internal_regs.t |= (val & 0b11111000) << 2; // set bits 5-9

                g_ppu_internal_regs.t &= ~(0b111 << 12);  // clear bits 12-14
                g_ppu_internal_regs.t |= (val & 0b111) << 12; // set bits 12-14
            } else {
                // setting x-scroll
                g_ppu_internal_regs.t &= ~(0b11111); // clear bits 0-4
                g_ppu_internal_regs.t |= val >> 3; // set bits 0-4
                
                g_ppu_internal_regs.x = val & 0x7; // copy fine x to x register
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;
            break;
        case 6:
            // set either the upper or lower address bits, depending on which write this is
            if (g_ppu_internal_regs.w) {
                // clear lower bits
                g_ppu_internal_regs.t &= ~0x00FF;
                // set lower bits
                g_ppu_internal_regs.t |= (val & 0xFF);

                // flush t to v
                g_ppu_internal_regs.v = g_ppu_internal_regs.t;
            } else {
                // clear upper bits
                g_ppu_internal_regs.t &= ~0x7F00;
                // set upper bits
                g_ppu_internal_regs.t |= (val & 0b111111) << 8;
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;

            break;
        case 7: {
            // write to the stored address
            ppu_memory_write(g_ppu_internal_regs.v, val);

            g_ppu_internal_regs.v += g_ppu_control.vertical_increment ? 32 : 1;

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
                return addr - 0x400;
            }
        }
        // name table 2
        case 0x800 ... 0xBFF: {
            if (g_mirror_mode == MIRROR_HORIZONTAL) {
                // use the second half of the memory
                return addr - 0x400;
            } else {
                // use name table 0 since name table 2 is a mirror
                return addr - 0x800;
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
        // pattern table
        case 0x0000 ... 0x1FFF: {
            return g_cartridge->chr_rom[addr];
        }
        // name table
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

void initiate_oam_dma(uint8_t page) {
    for (uint8_t i = 0; i < 0xFF; i++) {
        uint16_t addr = (page << 8) | i;
        ((char*) g_oam_ram)[addr] = memory_read(addr);
    }
}

// this code was shamelessly lifted from https://wiki.nesdev.com/w/index.php/PPU_scrolling
void _update_v_vertical(void) {
    // update vert(v)

    unsigned int v = g_ppu_internal_regs.v;

    // if fine y = 7
    if ((v & 0x7000) == 0x7000) {
        v &= ~0x7000; // clear fine y
        unsigned int y = (v & 0x03E0) >> 5; // get coarse y

        // check if we're at the end of the name table
        if (y == 29) {
            y = 0; // clear coarse y
            v ^= 0x8000; // flip vertical name table bit
        } else if (y == 31) {
            y = 0; // just clear coarse y
            //TODO: not sure why we don't switch the name table here
        } else {
            y += 1; // just increment coarse y
        }

        v = (v & ~0x03E0) | (y << 5); // insert coarse y back into v
    } else {
        // otherwise, just increment fine y
        v += 0x1000;
    }

    g_ppu_internal_regs.v = v;
}

// this code was shamelessly lifted from https://wiki.nesdev.com/w/index.php/PPU_scrolling
void _update_v_horizontal(void) {
    unsigned int v = g_ppu_internal_regs.v;

    if ((v & 0x1F) == 0x1F) {
        // if x = 31 (last tile of nametable), skip to next name table
        v &= ~0x1F; // clear the LSBs
        v ^= 0x0400; // flip the horizontal name table bit
    } else {
        // otherwise, just increment v
        v += 1;
    }

    // write the updated value back to the register
    g_ppu_internal_regs.v = v;
}

void cycle_ppu(void) {
    // if the frame is odd and background rendering is enabled, skip the first cycle
    if (g_scanline == 0 && g_scanline_tick == 0 && g_odd_frame && g_ppu_mask.show_background) {
        g_scanline_tick = 1;
    }

    switch (g_scanline) {
        // visible screen
        case 0 ... 239: {
            if (g_scanline_tick == 0) {
                // idle tick
                break;
            } else if (g_scanline_tick >= 257 && g_scanline_tick <= 320) {
                // hori(v) = hori(t)
                if (g_scanline_tick == 257 && _is_rendering_enabled()) {
                    g_ppu_internal_regs.v &= ~0x41F; // clear horizontal bits
                    g_ppu_internal_regs.v |= g_ppu_internal_regs.t & 0x41F; // copy horizontal bits to v from t
                }

                //TODO: perform sprite tile fetching
            } else {
                unsigned int fetch_pixel_x;
                unsigned int fetch_pixel_y;

                if ((g_scanline_tick >= 1 && g_scanline_tick <= 256)) {
                    fetch_pixel_x = g_scanline_tick + 15; // we fetch two tiles ahead
                    fetch_pixel_y = g_scanline;
                } else if (g_scanline_tick <= 336) { // start fetching for the next scanline
                    if (g_scanline == 239) {
                        // nothing to do since we're on the last scanline
                        break;
                    }
                    fetch_pixel_x = g_scanline_tick - 321; // fetching starts at cycle 321
                    fetch_pixel_y = g_scanline + 1; // we're on the next line
                } else {
                    // these cycles are for garbage NT fetches
                    break;
                }

                switch ((g_scanline_tick - 1) % 8) {
                    case 0: {
                        // flush the palette select data into the primary shift registers
                        g_ppu_internal_regs.palette_shift_l = 0xFF * (g_ppu_internal_regs.attr_table_entry_latch & 1);
                        g_ppu_internal_regs.palette_shift_h = 0xFF * (g_ppu_internal_regs.attr_table_entry_latch >> 1);

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
                        // address = name table base + (v except fine y)
                        uint16_t name_table_addr = NAME_TABLE_BASE_ADDR | (g_ppu_internal_regs.v & 0x0FFF);

                        g_ppu_internal_regs.name_table_entry_latch = ppu_memory_read(name_table_addr);

                        break;
                    }
                    case 3: {
                        // address = attr table base + (voodo)
                        unsigned int v = g_ppu_internal_regs.v;
                        uint16_t attr_table_addr = ATTR_TABLE_BASE_ADDR | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);

                        uint8_t attr_table_byte = ppu_memory_read(attr_table_addr);

                        // check if it's in the bottom half of the table cell
                        if ((fetch_pixel_y % 2) >= 16) {
                            attr_table_byte >>= 4;
                        }

                        // check if it's in the right half of the table cell
                        if ((fetch_pixel_x % 2) >= 16) {
                            attr_table_byte >>= 2;
                        }

                        g_ppu_internal_regs.attr_table_entry_latch = attr_table_byte & 0b11;

                        break;
                    }
                    case 5: {
                        // multiply by 16 since each plane is 8 bytes, and there are 2 planes per tile
                        // then we just add the mod of the current line to get the sub-tile offset
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + (fetch_pixel_y % 8);

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_l_latch = reverse_bits(ppu_memory_read(pattern_addr));

                        break;
                    }
                    case 7: {
                        // basically the same as above, but we add 8 to get the second plane
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + (fetch_pixel_y % 8) + 8;

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_h_latch = reverse_bits(ppu_memory_read(pattern_addr));


                        // only update v if rendering is enabled
                        if (_is_rendering_enabled()) {
                            // only update vertical v at the end of the visible part of the scanline
                            if (g_scanline_tick == 256) {
                                _update_v_vertical();
                            }

                            // update horizontal v after every tile
                            _update_v_horizontal();
                        }

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
                printf("START VBLANK\n");
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
                printf("END VBLANK\n");
                g_ppu_status.vblank = 0;
                g_ppu_status.sprite_0_hit = 0;
                g_ppu_status.sprite_overflow = 0;
            }

            // vert(v) = vert(t)
            if (g_scanline_tick >= 280 && g_scanline_tick <= 304 && _is_rendering_enabled()) {
                g_ppu_internal_regs.v &= ~0x7BE0; // clear vertical bits
                g_ppu_internal_regs.v |= g_ppu_internal_regs.t & 0x7BE0; // copy vertical bits to v from t
            }
        }
    }

    if (g_scanline < RESOLUTION_V && g_scanline_tick < RESOLUTION_H) {
        unsigned int palette_low = ((g_ppu_internal_regs.pattern_shift_h & 1) << 1)
                | (g_ppu_internal_regs.pattern_shift_l & 1);
        
        unsigned int palette_offset;

        if (palette_low) {
            // if the palette low bits are not zero, we select the color normally
            unsigned int palette_high = ((g_ppu_internal_regs.palette_shift_h & 1) << 1)
                    | (g_ppu_internal_regs.palette_shift_l & 1);
            palette_offset = (palette_high << 2) | palette_low;

            if (g_oam_ram[0].x == g_scanline - 1 && g_oam_ram[0].y == g_scanline_tick) {
                g_ppu_status.sprite_0_hit = 1;
            }
        } else {
            // otherwise, we use the default background color
            palette_offset = 0;
        }

        uint16_t palette_entry_addr = PALETTE_DATA_BASE_ADDR + palette_offset;
        const RGBValue rgb = g_palette[ppu_memory_read(palette_entry_addr)];

        set_pixel(g_scanline_tick, g_scanline, rgb);

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

            g_odd_frame = !g_odd_frame;

            flush_frame();

            /*printf("PALETTE DATA\n");
            for (unsigned int i = 0; i < 0x20; i++) {
                printf("$%04x: %02x\n", i, ppu_memory_read(PALETTE_DATA_BASE_ADDR + i));
            }*/
        }
    }
}
