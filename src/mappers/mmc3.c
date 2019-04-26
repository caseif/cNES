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

#include "cartridge.h"
#include "system.h"
#include "cpu/cpu.h"
#include "input/input_device.h"
#include "mappers/mappers.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <stdio.h>

#define PRG_RAM_SIZE 0x2000

#define CHR_BANK_GRANULARITY 0x400
#define PRG_BANK_GRANULARITY 0x2000

static unsigned char g_prg_ram[PRG_RAM_SIZE];

// false -> $C000-DFFF fixed, $8000-9FFF swappable
// true  -> $8000-9FFF fixed, $C000-DFFF swappable
static bool g_prg_switch_ranges = false;
// false -> 2 banks at $0000, 4 at $1000
// true  -> 4 banks at $0000, 2 at $1000
static bool g_chr_inversion = false;

static uint8_t g_bank_select = 0;

static uint8_t g_chr_big_1 = 0;
static uint8_t g_chr_big_2 = 0;
static uint8_t g_chr_little_1 = 0;
static uint8_t g_chr_little_2 = 0;
static uint8_t g_chr_little_3 = 0;
static uint8_t g_chr_little_4 = 0;
static uint8_t g_prg_1 = 0;
static uint8_t g_prg_2 = 1;

static uint8_t g_irq_counter;
static uint8_t g_irq_latch;
static bool g_irq_enabled = true;

static uint32_t _mmc3_get_prg_offset(Cartridge *cart, uint16_t addr) {
    assert(addr >= 0x8000);

    uint8_t bank = 0;
    switch (addr) {
        case 0x8000 ... 0x9FFF:
            if (g_prg_switch_ranges) {
                bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 2; // fixed, use second-to-last bank
            } else {
                bank = g_prg_1;
            }
            break;
        case 0xA000 ... 0xBFFF:
            bank = g_prg_2;
            break;
        case 0xC000 ... 0xDFFF:
            if (g_prg_switch_ranges) {
                bank = g_prg_1;
            } else {
                bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 2; // fixed, use second-to-last bank
            }
            break;
        case 0xE000 ... 0xFFFF:
            bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 1; // fixed, used last bank
            break;
    }
    return (bank * PRG_BANK_GRANULARITY) | (addr % PRG_BANK_GRANULARITY);
}

static uint32_t _mmc3_get_chr_offset(Cartridge *cart, uint16_t addr) {
    assert(addr < 0x2000);

    // undo the inversion (so the 0x0000..0x0FFF and 0x1000..0x1FFF ranges are swapped back)
    if (g_chr_inversion) {
        addr ^= 0x1000;
    }

    uint16_t bank_size = addr < 0x1000 ? 0x800 : 0x400; // big banks come first

    uint8_t bank;

    switch (addr) {
        case 0x0000 ... 0x07FF:
            bank = g_chr_big_1;
            break;
        case 0x0800 ... 0x0FFF:
            bank = g_chr_big_2;
            break;
        case 0x1000 ... 0x13FF:
            bank = g_chr_little_1;
            break;
        case 0x1400 ... 0x17FF:
            bank = g_chr_little_2;
            break;
        case 0x1800 ... 0x1BFF:
            bank = g_chr_little_3;
            break;
        case 0x1C00 ... 0x1FFF:
            bank = g_chr_little_4;
            break;
    }

    return (bank * CHR_BANK_GRANULARITY) | (addr % bank_size);
}

static uint8_t _mmc3_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x6000) {
        return system_lower_memory_read(addr);
    } else if (addr < 0x8000) {
        return g_prg_ram[addr % 0x2000];
    }

    uint32_t prg_offset = _mmc3_get_prg_offset(cart, addr);

    if (prg_offset >= cart->prg_size) {
        printf("Invalid PRG read ($%04x is outside PRG ROM range)\n", prg_offset);
        exit(-1);
    }

    return cart->prg_rom[prg_offset];
}

static void _mmc3_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x6000) {
        system_lower_memory_write(addr, val);
        return;
    } else if (addr < 0x8000) {
        g_prg_ram[addr % 0x2000] = val;
        return;
    }

    switch (addr & 0xE001) {
        case 0x8000:
            if (((val >> 7) & 1) ^ g_chr_inversion) {
                g_irq_counter--;
            }

            g_prg_switch_ranges = (val >> 6) & 1;
            g_chr_inversion = (val >> 7) & 1;
            g_bank_select = val & 0x7;
            return;
        case 0x8001:
            switch (g_bank_select) {
                case 0:
                    g_chr_big_1 = val & 0xFE; // ignore last bit for double-width banks
                    break;
                case 1:
                    g_chr_big_2 = val & 0xFE; // ignore last bit for double-width banks
                    break;
                case 2:
                    g_chr_little_1 = val;
                    break;
                case 3:
                    g_chr_little_2 = val;
                    break;
                case 4:
                    g_chr_little_3 = val;
                    break;
                case 5:
                    g_chr_little_4 = val;
                    break;
                case 6:
                    g_prg_1 = val;
                    break;
                case 7:
                    g_prg_2 = val;
                    break;
            }
            return;
        case 0xA000:
            ppu_set_mirroring_mode((val & 1) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL);
            return;
        case 0xA001:
            // unimplemented for MMC3
            return;
        case 0xC000:
            g_irq_latch = val;
            return;
        case 0xC001:
            g_irq_counter = 0;
            return;
        case 0xE000:
            if (g_irq_enabled) {
                cpu_raise_irq_line();
                g_irq_enabled = false;
            }
            return;
        case 0xE001:
            g_irq_enabled = true;
            return;
        default:
            return; // ignore bogus write
    }
}

static uint8_t _mmc3_vram_read(Cartridge *cart, uint16_t addr) {
    switch (addr) {
        case 0x0000 ... 0x1FFF: {
            uint32_t chr_offset = _mmc3_get_chr_offset(cart, addr);
            
            if (chr_offset >= cart->chr_size) {
                printf("Invalid CHR read ($%04x is outside CHR ROM range)\n", chr_offset);
                exit(-1);
            }
            
            return cart->chr_rom[chr_offset];
        }
        // name tables
        case 0x2000 ... 0x3EFF:
            return ppu_name_table_read(addr % 0x1000);
        // palette table
        case 0x3F00 ... 0x3FFF:
            return ppu_palette_table_read(addr % 0x20);
        default:
            return 0; // technically open bus
    }
}

static void _mmc3_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    switch (addr) {
        // PRG ROM
        case 0x0000 ... 0x1FFF:
            return; // do nothing
        // name tables
        case 0x2000 ... 0x3EFF:
            ppu_name_table_write(addr % 0x1000, val);
            return;
        // palette table
        case 0x3F00 ... 0x3FFF:
            ppu_palette_table_write(addr % 0x20, val);
            return;
    }
}

static void _mmc3_tick(void) {
    uint16_t target_tick = ppu_get_swap_pattern_tables() ? 324 : 260;
    if (ppu_get_scanline_tick() >= target_tick && ppu_get_scanline_tick() <= target_tick + 2) {
        if (g_irq_counter == 0) {
            g_irq_counter = g_irq_latch;
        } else {
            g_irq_counter--;
        }

        if (g_irq_counter == 0 && g_irq_enabled) {
            cpu_raise_irq_line();
        }
    }
}

void mapper_init_mmc3(Mapper *mapper) {
    mapper->ram_read_func   = *_mmc3_ram_read;
    mapper->ram_write_func  = *_mmc3_ram_write;
    mapper->vram_read_func  = *_mmc3_vram_read;
    mapper->vram_write_func = *_mmc3_vram_write;
    mapper->tick_func       = *_mmc3_tick;
}
