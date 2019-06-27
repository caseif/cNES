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

#define MMC3_DEBUG_LOGGING 0

#define PRG_RAM_SIZE 0x2000
#define CHR_RAM_SIZE 0x2000

#define CHR_BANK_GRANULARITY 0x1000
#define PRG_BANK_GRANULARITY 0x4000

static unsigned char g_prg_ram[PRG_RAM_SIZE];
static unsigned char g_chr_ram[CHR_RAM_SIZE];

static uint8_t g_write_count = 0;
static uint8_t g_write_val = 0;

static struct {
        unsigned int chr_bank_mode:1;
        unsigned int prg_bank_mode:2;
        unsigned int mirroring:2;
} g_mmc1_control;
static unsigned char g_chr_bank_0;
static unsigned char g_chr_bank_1;
static unsigned char g_prg_bank;
static bool g_enable_prg_ram = true;

static uint32_t _mmc1_get_prg_offset(Cartridge *cart, uint16_t addr) {
    assert(addr >= 0x8000);

    uint8_t bank;
    
    switch (g_mmc1_control.prg_bank_mode) {
        case 0:
        case 1:
            // switch both banks at once
            // add 1 if the address is in the upper half of PRG
            bank = (g_prg_bank & 0x1E) + (addr & 0x4000 ? 1 : 0);
            break;
        case 2:
            // fix lower bank to first, switch upper
            if (addr & 0x4000) {
                // upper half
                bank = g_prg_bank;
            } else {
                // lower half
                bank = 0;
            }
            break;
        case 3:
            // fix upper bank to last, switch lower
            if (addr & 0x4000) {
                // upper half
                bank = cart->prg_size / PRG_BANK_GRANULARITY - 1;
            } else {
                // lower half
                bank = g_prg_bank;
            }
            break;
    }

    return ((bank * PRG_BANK_GRANULARITY) | (addr % PRG_BANK_GRANULARITY)) % cart->prg_size;
}

static uint32_t _mmc1_get_chr_offset(Cartridge *cart, uint16_t addr) {
    assert(addr < 0x2000);

    uint8_t bank;
    if (g_mmc1_control.chr_bank_mode) {
        // two single-width switchable banks
        if (addr & 0x1000) {
            // upper bank
            bank = g_chr_bank_1;
        } else {
            // lower bank
            bank = g_chr_bank_0;
        }
    } else {
        // one double-width switchable bank
        // add 1 if the address is in the upper half of CHR
        bank = (g_chr_bank_0 & 0x1E) + (addr & 0x1000 ? 1 : 0);
    }

    return ((bank * CHR_BANK_GRANULARITY) | (addr % 0x1000)) % cart->chr_size;
}

static uint8_t _mmc1_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x6000) {
        return system_lower_memory_read(addr);
    } else if (addr < 0x8000) {
        return g_enable_prg_ram ? g_prg_ram[addr % 0x2000] : 0;
    }

    uint32_t prg_offset = _mmc1_get_prg_offset(cart, addr);

    if (prg_offset >= cart->prg_size) {
        printf("Invalid PRG read from $%04x ($%04x is outside PRG ROM range)\n", addr, prg_offset);
        exit(-1);
    }

    return cart->prg_rom[prg_offset];
}

static void _mmc1_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x6000) {
        system_lower_memory_write(addr, val);
        return;
    } else if (addr < 0x8000) {
        if (g_enable_prg_ram) {
            g_prg_ram[addr % 0x2000] = val;
        }
        return;
    }

    if (val & 0x80) {
        g_write_count = 0;
        g_write_val = 0;
        g_mmc1_control.prg_bank_mode = 3;
        return;
    }

    g_write_val |= (val & 0x01) << g_write_count;
    g_write_count++;

    if (g_write_count == 5) {
        switch (addr & 0xE000) {
            case 0x8000:
                g_mmc1_control.mirroring = g_write_val & 0x03;
                g_mmc1_control.prg_bank_mode = (g_write_val >> 2) & 0x03;
                g_mmc1_control.chr_bank_mode = (g_write_val >> 4) & 0x01;

                printf("mirror mode: %d\n", g_mmc1_control.mirroring);
                switch (g_mmc1_control.mirroring) {
                    case 0:
                        ppu_set_mirroring_mode(MIRROR_SINGLE_LOWER);
                        break;
                    case 1:
                        ppu_set_mirroring_mode(MIRROR_SINGLE_UPPER);
                        break;
                    case 2:
                        ppu_set_mirroring_mode(MIRROR_VERTICAL);
                        break;
                    case 3:
                        ppu_set_mirroring_mode(MIRROR_HORIZONTAL);
                        break;
                }

                break;
            case 0xA000:
                g_chr_bank_0 = g_write_val & 0x1F;
                break;
            case 0xC000:
                g_chr_bank_1 = g_write_val & 0x1F;
                break;
            case 0xE000:
                // technically the high bit is ignored, but it doesn't matter here
                g_prg_bank = g_write_val & 0x1F;
                g_enable_prg_ram = !((g_write_val & 0x1F) >> 4);
                break;
        }

        g_write_val = 0;
        g_write_count = 0;
    }
}

static uint8_t _mmc1_vram_read(Cartridge *cart, uint16_t addr) {
    switch (addr) {
        case 0x0000 ... 0x1FFF: {
            if (cart->chr_size == 0) {
                return g_chr_ram[addr];
            }

            uint32_t chr_offset = _mmc1_get_chr_offset(cart, addr);

            if (chr_offset >= cart->chr_size) {
                printf("Invalid CHR read from $%04x ($%04x is outside CHR ROM range)\n", addr, chr_offset);
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

static void _mmc1_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    switch (addr) {
        // PRG ROM
        case 0x0000 ... 0x1FFF:
            if (cart->chr_size == 0) {
                g_chr_ram[addr] = val;
            }
            return;
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

void mapper_init_mmc1(Mapper *mapper) {
    mapper->ram_read_func   = *_mmc1_ram_read;
    mapper->ram_write_func  = *_mmc1_ram_write;
    mapper->vram_read_func  = *_mmc1_vram_read;
    mapper->vram_write_func = *_mmc1_vram_write;
    mapper->tick_func       = NULL;

    g_mmc1_control.prg_bank_mode = 3;

    ppu_set_mirroring_mode(MIRROR_SINGLE_LOWER);
}
