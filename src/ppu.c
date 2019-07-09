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
#include "system.h"
#include "util.h"
#include "cpu/cpu.h"
#include "ppu.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCANLINE_COUNT 262
#define CYCLES_PER_SCANLINE 341

#define VBL_SCANLINE 241
#define VBL_SCANLINE_TICK 1

#define VRAM_SIZE 0x800
#define PALETTE_RAM_SIZE 0x20
#define OAM_PRIMARY_SIZE 0x100
#define OAM_SECONDARY_SIZE 0x20

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

#define PRINT_VRAM_WRITES 0

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
    {0x66, 0x66, 0x66}, {0x00, 0x1E, 0x9A}, {0x0E, 0x09, 0xA8}, {0x44, 0x00, 0x93},
    {0x71, 0x00, 0x60}, {0x89, 0x01, 0x1D}, {0x86, 0x13, 0x00}, {0x69, 0x29, 0x00},
    {0x39, 0x3E, 0x00}, {0x04, 0x4C, 0x00}, {0x00, 0x4F, 0x00}, {0x00, 0x47, 0x2B},
    {0x00, 0x35, 0x6C}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xAD, 0xAD, 0xAD}, {0x00, 0x50, 0xF1}, {0x3B, 0x34, 0xFF}, {0x80, 0x22, 0xE8},
    {0xBB, 0x1E, 0xA5}, {0xDB, 0x29, 0x4E}, {0xD7, 0x40, 0x00}, {0xB1, 0x5E, 0x00},
    {0x73, 0x79, 0x00}, {0x2D, 0x8B, 0x00}, {0x00, 0x8F, 0x08}, {0x00, 0x84, 0x60},
    {0x00, 0x6D, 0xB5}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF}, {0x4B, 0xA0, 0xFF}, {0x8A, 0x84, 0xFF}, {0xD1, 0x72, 0xFF},
    {0xFF, 0x6D, 0xF7}, {0xFF, 0x79, 0x9E}, {0xFF, 0x90, 0x47}, {0xFF, 0xAE, 0x0A},
    {0xC4, 0xCA, 0x00}, {0x7D, 0xDC, 0x13}, {0x41, 0xE1, 0x57}, {0x21, 0xD5, 0xB0},
    {0x25, 0xBE, 0xFF}, {0x4F, 0x4F, 0x4F}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF}, {0xB6, 0xD8, 0xFF}, {0xD0, 0xCD, 0xFF}, {0xED, 0xC6, 0xFF},
    {0xFF, 0xC4, 0xFC}, {0xFF, 0xC8, 0xD8}, {0xFF, 0xD2, 0xB4}, {0xFF, 0xDE, 0x9C},
    {0xE7, 0xE9, 0x94}, {0xCA, 0xF1, 0x9F}, {0xB2, 0xF3, 0xBB}, {0xA5, 0xEE, 0xDF},
    {0xA6, 0xE5, 0xFF}, {0xB8, 0xB8, 0xB8}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

static MirroringMode g_mirror_mode;

PpuControl g_ppu_control;
PpuMask g_ppu_mask;
PpuStatus g_ppu_status;
PpuInternalRegisters g_ppu_internal_regs;

unsigned char g_name_table_mem[VRAM_SIZE];
unsigned char g_palette_ram[PALETTE_RAM_SIZE];
Sprite g_oam_ram[OAM_PRIMARY_SIZE / sizeof(Sprite)];
Sprite g_secondary_oam_ram[OAM_SECONDARY_SIZE / sizeof(Sprite)];

static bool g_odd_frame;
static uint16_t g_scanline;
static uint16_t g_scanline_tick;

// ugly VBL flag suppression hack
static bool g_vbl_flag_suppression = false;

static RenderMode g_render_mode;

bool ppu_is_rendering_enabled(void) {
    return g_ppu_mask.show_background || g_ppu_mask.show_sprites;
}

void initialize_ppu() {
    g_ppu_control = (PpuControl) {0};
    g_ppu_mask = (PpuMask) {0};

    memset(g_name_table_mem, 0xFF, sizeof(g_name_table_mem));
    memset(g_palette_ram, 0xFF, sizeof(g_palette_ram));
    memset(g_oam_ram, 0xFF, sizeof(g_oam_ram));
    
    g_odd_frame = false;
    g_scanline = 0;
    g_scanline_tick = 0;
}

void ppu_set_mirroring_mode(MirroringMode mirror_mode) {
    g_mirror_mode = mirror_mode;
}

uint16_t ppu_get_scanline(void) {
    return g_scanline;
}

uint16_t ppu_get_scanline_tick(void) {
    return g_scanline_tick;
}

bool ppu_get_swap_pattern_tables(void) {
    return g_ppu_control.background_table;
}

static void _update_ppu_open_bus(uint8_t val, uint8_t bitmask) {
    g_ppu_internal_regs.ppu_open_bus &= ~bitmask;
    g_ppu_internal_regs.ppu_open_bus |= val & bitmask;

    for (unsigned int i = 0; i < 8; i++) {
        // only refresh bits within the bitmask
        if (bitmask & (1 << i)) {
            g_ppu_internal_regs.ppu_open_bus_decay_timers[i] = PPU_OPEN_BUS_DECAY_CYCLES;
        }
    }
}

static void _check_open_bus_decay(void) {
    for (unsigned int i = 0; i < 8; i++) {
        // we don't want to decrement the timer if it's already 0
        if (g_ppu_internal_regs.ppu_open_bus_decay_timers[i] == 0) {
            continue;
        }
        if (--g_ppu_internal_regs.ppu_open_bus_decay_timers[i] == 0) {
            g_ppu_internal_regs.ppu_open_bus &= ~(1 << i);
        } else if (g_ppu_internal_regs.ppu_open_bus_decay_timers[i] < 1000) {
        }
    }
}

uint8_t ppu_read_mmio(uint8_t index) {
    assert(index <= 7);

    switch (index) {
        case 0:
        case 1:
        case 3:
        case 5:
        case 6:
            break; // just return the current open bus value
        case 2: {
            uint8_t res = g_ppu_status.serial;

            // reading this register resets this latch
            g_ppu_internal_regs.w = 0;
            // and the vblank flag
            g_ppu_status.vblank = 0;

            cpu_clear_nmi_line();

            if (g_scanline == VBL_SCANLINE && g_scanline_tick == VBL_SCANLINE_TICK - 1) {
                g_vbl_flag_suppression = true;
            }

            _update_ppu_open_bus(res, 0xE0);

            break;
        }
        case 4: {
            uint8_t res = ((unsigned char*) g_oam_ram)[g_ppu_internal_regs.s];

            // weird special case
            if (g_ppu_internal_regs.s % 4 == 2) {
                res &= 0xE3;
            }
            
            _update_ppu_open_bus(res, 0xFF);

            break;
        }
        case 7: {
            uint8_t res;
            if (g_ppu_internal_regs.v.addr < 0x3F00) {
                // most VRAM goes through a read buffer
                res = g_ppu_internal_regs.read_buf;

                g_ppu_internal_regs.read_buf = system_vram_read(g_ppu_internal_regs.v.addr);

                g_ppu_internal_regs.v.addr += g_ppu_control.vertical_increment ? 32 : 1;

                _update_ppu_open_bus(res, 0xFF);
            } else {
                // palette reading bypasses buffer, but still updates it in a weird way

                // address is offset since the buffer is updated with the mirrored NT data "under" the palette data
                g_ppu_internal_regs.read_buf = system_vram_read(g_ppu_internal_regs.v.addr - 0x1000);

                res = system_vram_read(g_ppu_internal_regs.v.addr);

                _update_ppu_open_bus(res, 0x3F);
            }
            break;
        }
        default: {
            assert(false);
        }
    }

    // we can get away with this because for registers that are readable, the open bus has already been updated
    // with the appropriate value
    // doing it this way simplifies handling of registers where some bits of the read value are from the open bus
    return g_ppu_internal_regs.ppu_open_bus;
}

void ppu_write_mmio(uint8_t index, uint8_t val) {
    assert(index <= 7);

    switch (index) {
        case 0: {
            bool old_gen_nmis = g_ppu_control.gen_nmis;

            g_ppu_control.serial = val;

            g_ppu_internal_regs.t.addr &= ~(0b11 << 10); // clear bits 10-11
            g_ppu_internal_regs.t.addr |= (val & 0b11) << 10; // set bits 10-11 to current nametable

            if (!old_gen_nmis && g_ppu_control.gen_nmis && g_ppu_status.vblank) {
                cpu_raise_nmi_line();
            }

            break;
        }
        case 1:
            g_ppu_mask.serial = val;
            break;
        case 2:
            // I don't think anything happens here besides the open bus update
            break;
        case 3:
            g_ppu_internal_regs.s = val;
            break;
        case 4:
            ((unsigned char*) g_oam_ram)[g_ppu_internal_regs.s++] = val;
            break;
        case 5:
            // set either x- or y-scroll, depending on whether this is the first or second write
            if (g_ppu_internal_regs.w) {
                // setting y-scroll
                g_ppu_internal_regs.t.addr &= ~(0b11111 << 5); // clear bits 5-9
                g_ppu_internal_regs.t.addr |= (val & 0b11111000) << 2; // set bits 5-9

                g_ppu_internal_regs.t.addr &= ~(0b111 << 12);  // clear bits 12-14
                g_ppu_internal_regs.t.addr |= (val & 0b111) << 12; // set bits 12-14
            } else {
                // setting x-scroll
                g_ppu_internal_regs.t.addr &= ~(0b11111); // clear bits 0-4
                g_ppu_internal_regs.t.addr |= val >> 3; // set bits 0-4
                
                g_ppu_internal_regs.x = val & 0x7; // copy fine x to x register
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;
            break;
        case 6:
            // set either the upper or lower address bits, depending on which write this is

            #if PRINT_VRAM_WRITES
            printf("PPU address (%s): %02x\n", g_ppu_internal_regs.w ? "low" : "high", val);
            #endif

            if (g_ppu_internal_regs.w) {
                // clear lower bits
                g_ppu_internal_regs.t.addr &= ~0x00FF;
                // set lower bits
                g_ppu_internal_regs.t.addr |= (val & 0xFF);

                // flush t to v
                g_ppu_internal_regs.v.addr = g_ppu_internal_regs.t.addr;
            } else {
                // clear upper bits
                g_ppu_internal_regs.t.addr &= ~0x7F00;
                // set upper bits
                g_ppu_internal_regs.t.addr |= (val & 0b111111) << 8;
                // set MSB to 0
                g_ppu_internal_regs.t.addr &= ~0x4000;
            }

            // flip w flag
            g_ppu_internal_regs.w = !g_ppu_internal_regs.w;

            break;
        case 7: {
            // write to the stored address

            #if PRINT_VRAM_WRITES
            printf("PPU write: $%04x, %02x\n", g_ppu_internal_regs.v.addr, val);
            #endif

            system_vram_write(g_ppu_internal_regs.v.addr, val);

            g_ppu_internal_regs.v.addr += g_ppu_control.vertical_increment ? 32 : 1;

            break;
        }
        default:
            assert(false);
    }

    _update_ppu_open_bus(val, 0xFF);
}

uint16_t _translate_name_table_address(uint16_t addr) {
    assert(addr < 0x1000);

    if (g_mirror_mode == MIRROR_SINGLE_LOWER) {
        return addr % 0x400;
    } else if (g_mirror_mode == MIRROR_SINGLE_UPPER) {
        return (addr % 0x400) + 0x400;
    }

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
            } else if (g_mirror_mode == MIRROR_HORIZONTAL) {
                // look up the data in name table 0 since name table 1 is a mirror
                return addr - 0x400;
            } else {
                printf("Got bad mirroring mode %d\n", g_mirror_mode);
                exit(1);
            }
        }
        // name table 2
        case 0x800 ... 0xBFF: {
            if (g_mirror_mode == MIRROR_HORIZONTAL) {
                // use the second half of the memory
                return addr - 0x400;
            } else if (g_mirror_mode == MIRROR_VERTICAL) {
                // use name table 0 since name table 2 is a mirror
                return addr - 0x800;
            } else {
                printf("Got bad mirroring mode %d\n", g_mirror_mode);
                exit(1);
            }
        }
        // name table 3
        case 0xC00 ... 0xFFF: {
            // it's always in the second half
            return addr - 0x800;
        }
        default: {
            printf("Bad nametable address $%04X\n", addr);
            exit(-1); // shouldn't happen
        }
    }
}

uint8_t ppu_name_table_read(uint16_t addr) {
    assert(addr < 0x1000);
    return g_name_table_mem[_translate_name_table_address(addr)];
}

void ppu_name_table_write(uint16_t addr, uint8_t val) {
    assert(addr < 0x1000);
    g_name_table_mem[_translate_name_table_address(addr)] = val;
}

uint8_t ppu_palette_table_read(uint8_t index) {
    assert(index < 0x20);

    // certain indices are just mirrors
    switch (index) {
        case 0x10:
        case 0x14:
        case 0x18:
        case 0x1C:
            index -= 0x10;
            break;
    }

    if (g_ppu_mask.monochrome) {
        index &= 0xF0;
    }

    return g_palette_ram[index];
}

void ppu_palette_table_write(uint8_t index, uint8_t val) {
    assert(index < 0x20);

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
}

void ppu_start_oam_dma(uint8_t page) {
    for (unsigned int i = 0; i <= 0xFF; i++) {
        uint16_t addr = (page << 8) | i;
        ((char*) g_oam_ram)[(uint8_t) (g_ppu_internal_regs.s + i)] = system_ram_read(addr);
    }
}

// this code was shamelessly lifted from https://wiki.nesdev.com/w/index.php/PPU_scrolling
void _update_v_vertical(void) {
    // update vert(v)

    unsigned int v = g_ppu_internal_regs.v.addr;

    // if fine y = 7
    if ((v & 0x7000) == 0x7000) {
        v &= ~0x7000; // clear fine y
        unsigned int y = (v & 0x03E0) >> 5; // get coarse y

        // check if we're at the end of the name table
        if (y == 29) {
            y = 0; // clear coarse y
            v ^= 0x0800; // flip vertical name table bit
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

    g_ppu_internal_regs.v.addr = v;
}

// this code was shamelessly lifted from https://wiki.nesdev.com/w/index.php/PPU_scrolling
void _update_v_horizontal(void) {
    unsigned int v = g_ppu_internal_regs.v.addr;

    if ((v & 0x1F) == 0x1F) {
        // if x = 31 (last tile of nametable), skip to next name table
        v &= ~0x1F; // clear the LSBs
        v ^= 0x0400; // flip the horizontal name table bit
    } else {
        // otherwise, just increment v
        v += 1;
    }

    // write the updated value back to the register
    g_ppu_internal_regs.v.addr = v;
}

void _do_general_cycle_routine(void) {
    switch (g_scanline) {
        // pre-render line
        case PRE_RENDER_LINE: {
            // clear status
            if (g_scanline_tick == 1) {
                g_ppu_status.vblank = 0;
                g_ppu_status.sprite_0_hit = 0;
                g_ppu_status.sprite_overflow = 0;
            }

            // vert(v) = vert(t)
            if (g_scanline_tick >= 280 && g_scanline_tick <= 304 && ppu_is_rendering_enabled()) {
                g_ppu_internal_regs.v.addr &= ~0x7BE0; // clear vertical bits
                g_ppu_internal_regs.v.addr |= g_ppu_internal_regs.t.addr & 0x7BE0; // copy vertical bits to v from t
            }

            // intentional fall-through
        }
        // visible screen
        case FIRST_VISIBLE_LINE ... LAST_VISIBLE_LINE: {
            if (g_scanline_tick == 0) {
                // idle tick
                break;
            } else if (g_scanline_tick > LAST_VISIBLE_CYCLE && g_scanline_tick <= 320) {
                // hori(v) = hori(t)
                if (g_scanline_tick == 257 && ppu_is_rendering_enabled()) {
                    g_ppu_internal_regs.v.addr &= ~0x41F; // clear horizontal bits
                    g_ppu_internal_regs.v.addr |= g_ppu_internal_regs.t.addr & 0x41F; // copy horizontal bits to v from t
                }
            } else {
                if (g_scanline_tick > LAST_VISIBLE_CYCLE && g_scanline_tick < 321) {
                    // these cycles are for garbage NT fetches
                    break;
                }

                switch ((g_scanline_tick - 1) % 8) {
                    case 0: {
                        // copy the palette data from the secondary latch to the primary
                        g_ppu_internal_regs.attr_table_entry_latch = g_ppu_internal_regs.attr_table_entry_latch_secondary;

                        // clear upper bits
                        g_ppu_internal_regs.pattern_shift_l &= ~0xFF00;
                        g_ppu_internal_regs.pattern_shift_h &= ~0xFF00;
                        // set upper bits
                        g_ppu_internal_regs.pattern_shift_l |= g_ppu_internal_regs.pattern_bitmap_l_latch << 8;
                        g_ppu_internal_regs.pattern_shift_h |= g_ppu_internal_regs.pattern_bitmap_h_latch << 8;

                        break;
                    }
                    // fetch name table entry
                    case 1: {
                        // address = name table base + (v except fine y)
                        uint16_t name_table_addr = NAME_TABLE_BASE_ADDR | (g_ppu_internal_regs.v.addr & 0x0FFF);

                        g_ppu_internal_regs.name_table_entry_latch = system_vram_read(name_table_addr);

                        break;
                    }
                    case 3: {
                        // address = attr table base + (name table offset) + (shifted v)
                        unsigned int v = g_ppu_internal_regs.v.addr;

                        uint16_t attr_table_addr = ATTR_TABLE_BASE_ADDR | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);

                        uint8_t attr_table_byte = system_vram_read(attr_table_addr);

                        // check if it's in the bottom half of the table cell
                        if (g_ppu_internal_regs.v.y_coarse & 0b10) {
                            attr_table_byte >>= 4;
                        }

                        // check if it's in the right half of the table cell
                        if (g_ppu_internal_regs.v.x_coarse & 0b10) {
                            attr_table_byte >>= 2;
                        }

                        g_ppu_internal_regs.attr_table_entry_latch_secondary = attr_table_byte & 0b11;

                        break;
                    }
                    case 5: {
                        // multiply by 16 since each plane is 8 bytes, and there are 2 planes per tile
                        // then we just add the mod of the current line to get the sub-tile offset
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + g_ppu_internal_regs.v.y_fine;

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_l_latch = reverse_bits(system_vram_read(pattern_addr));

                        break;
                    }
                    case 7: {
                        // basically the same as above, but we add 8 to get the second plane
                        unsigned int pattern_offset = g_ppu_internal_regs.name_table_entry_latch * 16
                                + g_ppu_internal_regs.v.y_fine + 8;

                        uint16_t pattern_addr = (g_ppu_control.background_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                + pattern_offset;

                        g_ppu_internal_regs.pattern_bitmap_h_latch = reverse_bits(system_vram_read(pattern_addr));

                        // only update v if rendering is enabled
                        if (ppu_is_rendering_enabled()) {
                            // only update vertical v at the end of the visible part of the scanline
                            if (g_scanline_tick == LAST_VISIBLE_CYCLE) {
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
        case VBL_SCANLINE: {
            if (g_scanline_tick == VBL_SCANLINE_TICK) {
                if (!g_vbl_flag_suppression) {
                    g_ppu_status.vblank = 1;
                }
                g_vbl_flag_suppression = false;

                if (g_ppu_control.gen_nmis) {
                    cpu_raise_nmi_line();
                }
            }

            break;
        }
    }
}

void _do_sprite_evaluation(void) {
    if ((g_scanline >= FIRST_VISIBLE_LINE && g_scanline <= LAST_VISIBLE_LINE)) {
        switch (g_scanline_tick) {
            // idle tick
            case 0:
                // reset some registers
                g_ppu_internal_regs.m = 0;
                g_ppu_internal_regs.n = 0;
                g_ppu_internal_regs.o = 0;
                // copy sprite 0 flag to flag for current scanline
                g_ppu_internal_regs.sprite_0_scanline = g_ppu_internal_regs.sprite_0_next_scanline;
                g_ppu_internal_regs.sprite_0_next_scanline = false;
                break;
            case 1 ... 64:
                // clear secondary OAM byte-by-byte, but only on even ticks
                if (g_scanline_tick % 2 == 0) {
                    ((char*) g_secondary_oam_ram)[(uint8_t) (g_scanline_tick / 2 - 1)] = 0xFF;
                }
                break;
            case 65 ... 256: {
                if (g_ppu_internal_regs.n >= sizeof(g_oam_ram) / sizeof(Sprite)) {
                    // we've reached the end of OAM
                    break;
                }

                if (g_scanline_tick % 2 == 1) {
                    // read from primary OAM on odd ticks
                    Sprite sprite = g_oam_ram[g_ppu_internal_regs.n];

                    switch (g_ppu_internal_regs.m) {
                        case 0: {
                            uint8_t val = sprite.y;

                            // check if the sprite is on the next scanline
                            // we compare to the current line since sprites are rendered a line late
                            if (val <= g_scanline && g_scanline - val <= (g_ppu_control.tall_sprites ? 15 : 7)) {
                                // increment m if it is
                                g_ppu_internal_regs.m++;

                                // store the byte in a latch for writing on the next cycle
                                g_ppu_internal_regs.sprite_attr_latch = val;
                                g_ppu_internal_regs.has_latched_sprite = true;

                                // if we've already hit the max sprites per line, set the overflow flag
                                if (g_ppu_internal_regs.o >= 8) {
                                    g_ppu_status.sprite_overflow = 1;
                                }
                            } else {
                                // move to next sprite
                                g_ppu_internal_regs.n++;
                            }

                            break;
                        }
                        case 1: {
                            uint8_t val = sprite.tile_num;

                            // store the byte in a latch for writing on the next cycle
                            g_ppu_internal_regs.sprite_attr_latch = val;
                            g_ppu_internal_regs.has_latched_sprite = true;

                            // increment m since we've already decided to copy this sprite
                            g_ppu_internal_regs.m++;

                            break;
                        }
                        case 2: {
                            uint8_t val = sprite.attrs_serial;

                            // store the byte in a latch for writing on the next cycle
                            g_ppu_internal_regs.sprite_attr_latch = val;
                            g_ppu_internal_regs.has_latched_sprite = true;

                            // increment m, same as above
                            g_ppu_internal_regs.m++;

                            break;
                        }
                        case 3: {
                            uint8_t val = sprite.x;

                            // store the byte in a latch for writing on the next cycle
                            g_ppu_internal_regs.sprite_attr_latch = val;
                            g_ppu_internal_regs.has_latched_sprite = true;

                            // increment m, same as above
                            g_ppu_internal_regs.m++;

                            break;
                        }
                    }
                } else {
                    // write the latched byte to secondary oam, if applicable
                    if (g_ppu_internal_regs.has_latched_sprite) {
                        if (g_ppu_internal_regs.o < 8) {
                            assert(g_ppu_internal_regs.m <= 4);
                            ((char*) &g_secondary_oam_ram[g_ppu_internal_regs.o])[g_ppu_internal_regs.m - 1] = g_ppu_internal_regs.sprite_attr_latch;
                            g_ppu_internal_regs.has_latched_sprite = false;
                        }
                    }

                    // reset our registers
                    if (g_ppu_internal_regs.m == 4) {
                        if (g_ppu_internal_regs.n == 0) {
                            g_ppu_internal_regs.sprite_0_next_scanline = true;
                        }
                        // reset m and increment n/o
                        g_ppu_internal_regs.n++;
                        g_ppu_internal_regs.o++;

                        g_ppu_internal_regs.m = 0;
                    }
                }

                break;
            }
        }
        if ((g_scanline >= FIRST_VISIBLE_LINE && g_scanline <= LAST_VISIBLE_LINE) || g_scanline == PRE_RENDER_LINE) {
            if (g_scanline_tick >= 257 && g_scanline_tick <= 320) {
                // sprite tile fetching

                if (g_scanline_tick == 257) {
                    g_ppu_internal_regs.loaded_sprites = g_ppu_internal_regs.o;
                }

                if (g_scanline_tick == 257) {
                    // reset secondary oam index
                    g_ppu_internal_regs.o = 0;
                }

                unsigned int index = g_ppu_internal_regs.o;
                switch ((g_scanline_tick - 1) % 8) {
                    case 0: {
                        g_ppu_internal_regs.sprite_y_latch = g_secondary_oam_ram[index].y;

                        break;
                    }
                    case 1: {
                        g_ppu_internal_regs.sprite_tile_index_latch = g_secondary_oam_ram[index].tile_num;

                        break;
                    }
                    case 2: {
                        g_ppu_internal_regs.sprite_attr_latches[index] = g_secondary_oam_ram[index].attrs;

                        break;
                    }
                    case 3: {
                        g_ppu_internal_regs.sprite_x_counters[index] = g_secondary_oam_ram[index].x;
                        // set the death counter latch to the sprite width
                        g_ppu_internal_regs.sprite_death_counters[index] = 8;

                        break;
                    }
                    case 5: {
                        // fetch tile lower byte
                        SpriteAttributes attrs = g_ppu_internal_regs.sprite_attr_latches[index];

                        if (index < g_ppu_internal_regs.loaded_sprites) {
                            uint16_t tile_index = g_ppu_internal_regs.sprite_tile_index_latch;

                            uint8_t cur_y = g_scanline - g_ppu_internal_regs.sprite_y_latch;
                            bool bottom_tile = false;
                            if (g_ppu_control.tall_sprites) {
                                bottom_tile = (cur_y > 7) ^ attrs.flip_ver;
                                if (cur_y > 7) {
                                    cur_y -= 8;
                                }
                            }
                            if (attrs.flip_ver) {
                                cur_y = 7 - cur_y;
                            }

                            uint16_t addr;
                            if (g_ppu_control.tall_sprites) {
                                uint16_t adj_tile_index = (tile_index & 0xFE) | (bottom_tile ? 1 : 0);
                                addr = ((tile_index & 1) * 0x1000) | (adj_tile_index * 16 + cur_y);
                            } else {
                                addr = (g_ppu_control.sprite_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                        | (tile_index * 16 + cur_y);
                            }

                            uint8_t res = system_vram_read(addr);

                            if (!attrs.flip_hor) {
                                res = reverse_bits(res);
                            }

                            g_ppu_internal_regs.sprite_tile_shift_l[index] = res;
                        } else {
                            // load transparent bitmap
                            g_ppu_internal_regs.sprite_tile_shift_l[index] = 0;
                        }

                        break;
                    }
                    case 7: {
                        // fetch tile upper byte
                        // same as above, but we add 8 to the address
                        SpriteAttributes attrs = g_ppu_internal_regs.sprite_attr_latches[index];

                        if (index < g_ppu_internal_regs.loaded_sprites) {
                            uint16_t tile_index = g_ppu_internal_regs.sprite_tile_index_latch;

                            uint8_t cur_y = g_scanline - g_ppu_internal_regs.sprite_y_latch;
                            bool bottom_tile = false;
                            if (g_ppu_control.tall_sprites) {
                                bottom_tile = (cur_y > 7) ^ attrs.flip_ver;
                                if (cur_y > 7) {
                                    cur_y -= 8;
                                }
                            }
                            if (attrs.flip_ver) {
                                cur_y = 7 - cur_y;
                            }

                            uint16_t addr;
                            if (g_ppu_control.tall_sprites) {
                                uint16_t adj_tile_index = (tile_index & 0xFE) | (bottom_tile ? 1 : 0);
                                addr = ((tile_index & 1) * 0x1000) | (adj_tile_index * 16 + cur_y + 8);
                            } else {
                                addr = (g_ppu_control.sprite_table ? PT_RIGHT_ADDR : PT_LEFT_ADDR)
                                        | (tile_index * 16 + cur_y + 8);
                            }

                            uint8_t res = system_vram_read(addr);

                            if (!attrs.flip_hor) {
                                res = reverse_bits(res);
                            }

                            g_ppu_internal_regs.sprite_tile_shift_h[index] = res;
                        } else {
                            // load transparent bitmap
                            g_ppu_internal_regs.sprite_tile_shift_h[index] = 0;
                        }

                        g_ppu_internal_regs.o++;

                        break;
                    }
                    default: {
                        // twiddle our thumbs
                        break;
                    }
                }
            }
        }
    }
}

RenderMode get_render_mode(void) {
    return g_render_mode;
}

void set_render_mode(RenderMode mode) {
    g_render_mode = mode;
}

void render_pixel(uint8_t x, uint8_t y, RGBValue rgb) {
    bool use_nt = false;
    uint8_t pt_tile = 0;
    uint8_t palette_num = 0;

    switch (g_render_mode) {
        case RM_NORMAL:
        default:
            set_pixel(x, y, rgb);
            break;
        case RM_NT0:
        case RM_NT1:
        case RM_NT2:
        case RM_NT3: {
            use_nt = true;
            uint8_t nt_index = g_render_mode - RM_NT0;
            uint16_t nt_base = NAME_TABLE_BASE_ADDR | (nt_index * NAME_TABLE_INTERVAL);
            pt_tile = system_vram_read(nt_base | ((y / NAME_TABLE_GRANULARITY) * NAME_TABLE_WIDTH + (x / NAME_TABLE_GRANULARITY)));
            palette_num = system_vram_read(nt_base | ((y / ATTR_TABLE_GRANULARITY) * ATTR_TABLE_WIDTH + (x / ATTR_TABLE_GRANULARITY)));
            if ((y % 32) >= 16) {
                palette_num >>= 4;
            }
            if ((x % 32) >= 16) {
                palette_num >>= 2;
            }
            palette_num &= 0b11;
            // intentional fall-through
        }
        case RM_PT: {
            if (!use_nt) {
                pt_tile = (y / 8) * 16 + (x % 128) / 8;
            }

            uint16_t pattern_offset = pt_tile * 16 + (y % NAME_TABLE_GRANULARITY);

            uint16_t pattern_addr = ((use_nt ? g_ppu_control.background_table : x >= 128)
                    ? PT_RIGHT_ADDR
                    : PT_LEFT_ADDR)
                    + pattern_offset;

            uint8_t pattern_pixel = ((system_vram_read(pattern_addr) >> (7 - (x % 8))) & 1) | (((system_vram_read(pattern_addr + 8) >> (7 - (x % 8))) & 1) << 1);

            int8_t palette_index = system_vram_read(PALETTE_DATA_BASE_ADDR | (pattern_pixel ? (palette_num << 2) : 0) | pattern_pixel);
            RGBValue pixel_rgb = g_palette[palette_index];

            set_pixel(x, y, pixel_rgb);
            
            break;
        }
    }
}

void cycle_ppu(void) {
    // if the frame is odd and background rendering is enabled, skip the first cycle
    if (g_scanline == 0 && g_scanline_tick == 0 && g_odd_frame && g_ppu_mask.show_background) {
        g_scanline_tick = 1;
    }

    _do_general_cycle_routine();

    if (ppu_is_rendering_enabled()) {
        _do_sprite_evaluation();
    }

    unsigned int draw_pixel_x = g_scanline_tick - 1;
    unsigned int draw_pixel_y = g_scanline;

    if (g_scanline < RESOLUTION_V && g_scanline_tick > 0 && g_scanline_tick <= RESOLUTION_H) {
        unsigned int palette_low = (((g_ppu_internal_regs.pattern_shift_h >> g_ppu_internal_regs.x) & 1) << 1)
                | ((g_ppu_internal_regs.pattern_shift_l >> g_ppu_internal_regs.x) & 1);

        unsigned int bg_palette_offset;

        bool transparent_background = false;

        if (palette_low && !(!g_ppu_mask.show_background_left && g_scanline_tick <= 8)) {
            // if the palette low bits are not zero, we select the color normally
            unsigned int palette_high = (((g_ppu_internal_regs.palette_shift_h >> g_ppu_internal_regs.x) & 1) << 1)
                    | ((g_ppu_internal_regs.palette_shift_l >> g_ppu_internal_regs.x) & 1);
            bg_palette_offset = (palette_high << 2) | palette_low;
        } else {
            // otherwise, we use the default background color
            bg_palette_offset = 0;
            transparent_background = true;
        }

        uint8_t final_palette_offset;
        if (g_ppu_mask.show_background) {
            final_palette_offset = bg_palette_offset;
        } else {
            final_palette_offset = 0x0f;
        }

        // time to read sprite data

        // don't render sprites if sprite rendering is disabled, or if they should be clipped
        if (g_ppu_mask.show_sprites && !(!g_ppu_mask.show_sprites_left && g_scanline_tick <= 8)) {
            // iterate all sprites for the current scanline
            for (unsigned int i = 0; i < g_ppu_internal_regs.loaded_sprites; i++) {
                // if the x counter hasn't run down to zero, skip it
                if (g_ppu_internal_regs.sprite_x_counters[i]) {
                    continue;
                }

                // if the death counter went to zero, this sprite is done rendering
                if (!g_ppu_internal_regs.sprite_death_counters[i]) {
                    continue;
                }

                unsigned int palette_low = ((g_ppu_internal_regs.sprite_tile_shift_h[i] & 1) << 1)
                                            | (g_ppu_internal_regs.sprite_tile_shift_l[i] & 1);
                // if the pixel is transparent, just continue
                if (!palette_low) {
                    continue;
                }

                if (g_ppu_internal_regs.sprite_0_scanline
                        && i == 0
                        && g_ppu_mask.show_background
                        && !transparent_background
                        && g_scanline_tick != 256) {
                    g_ppu_status.sprite_0_hit = 1; // set the hit flag
                }

                SpriteAttributes attrs = g_ppu_internal_regs.sprite_attr_latches[i];

                uint8_t palette_high = 0x4 | attrs.palette_index;
                uint8_t sprite_palette_offset = (palette_high << 2) | palette_low;

                if (!attrs.low_priority || transparent_background) {
                    final_palette_offset = sprite_palette_offset;
                }
                break;
            }
        }

        uint16_t palette_entry_addr = PALETTE_DATA_BASE_ADDR | final_palette_offset;

        uint8_t palette_index = system_vram_read(palette_entry_addr);

        RGBValue rgb = g_palette[palette_index % (sizeof(g_palette) / sizeof(RGBValue))];

        render_pixel(draw_pixel_x, draw_pixel_y, rgb);

        for (int i = 0; i < 8; i++) {
            if (g_ppu_internal_regs.sprite_x_counters[i]) {
                g_ppu_internal_regs.sprite_x_counters[i]--;
            } else {
                if (g_ppu_internal_regs.sprite_death_counters[i]) {
                    g_ppu_internal_regs.sprite_death_counters[i]--;
                    g_ppu_internal_regs.sprite_tile_shift_l[i] >>= 1;
                    g_ppu_internal_regs.sprite_tile_shift_h[i] >>= 1;
                }
            }
        }
    }

    if (g_scanline < RESOLUTION_V
            && ((g_scanline_tick > 0 && g_scanline_tick <= RESOLUTION_H)
                    || (g_scanline_tick >= 329 && g_scanline_tick <= 336))) {
        // shift the internal registers
        g_ppu_internal_regs.pattern_shift_h >>= 1;
        g_ppu_internal_regs.pattern_shift_l >>= 1;
        g_ppu_internal_regs.palette_shift_h >>= 1;
        g_ppu_internal_regs.palette_shift_l >>= 1;
        // feed the attribute registers from the latch(es)
        g_ppu_internal_regs.palette_shift_h |= (g_ppu_internal_regs.attr_table_entry_latch & 0b10) << 6;
        g_ppu_internal_regs.palette_shift_l |= (g_ppu_internal_regs.attr_table_entry_latch & 0b01) << 7;
    }

    if (++g_scanline_tick >= CYCLES_PER_SCANLINE) {
        g_scanline_tick = 0;

        if (++g_scanline >= SCANLINE_COUNT) {
            g_scanline = 0;

            g_odd_frame = !g_odd_frame;

            flush_frame();
        }
    }

    _check_open_bus_decay();
}

void dump_vram(void) {
    FILE *out_file = fopen("vram.bin", "w+");

    if (!out_file) {
        printf("Failed to dump VRAM (couldn't open file: %s)\n", strerror(errno));
        return;
    }

    fwrite(g_name_table_mem, VRAM_SIZE, 1, out_file);
    fwrite(g_palette_ram, PALETTE_RAM_SIZE, 1, out_file);

    fclose(out_file);
}

void dump_oam(void) {
    FILE *out_file = fopen("oam.bin", "w+");

    if (!out_file) {
        printf("Failed to dump OAM (couldn't open file: %s)\n", strerror(errno));
        return;
    }

    fwrite(g_oam_ram, OAM_PRIMARY_SIZE, 1, out_file);

    fclose(out_file);
}
