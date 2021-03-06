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
#include "c6502/cpu.h"
#include "input/input_device.h"
#include "mappers/mappers.h"
#include "mappers/nrom.h"
#include "ppu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHR_RAM_SIZE 0x2000

#define CHR_BANK_GRANULARITY 0x1000
#define PRG_BANK_GRANULARITY 0x4000

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
    }

    if (addr < 0x8000) {
        return g_enable_prg_ram ? system_prg_ram_read(addr % 0x2000) : system_bus_read();
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
    }

    if (addr < 0x8000) {
        if (g_enable_prg_ram) {
            system_prg_ram_write(addr % 0x2000, val);
        } else {
            system_bus_write(addr & 0xFF);
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
        if (addr >= 0x0000 && addr <= 0x1FFF) {
            if (cart->chr_size == 0) {
                return system_chr_ram_read(addr);
            }

            uint32_t chr_offset = _mmc1_get_chr_offset(cart, addr);

            if (chr_offset >= cart->chr_size) {
                printf("Invalid CHR read from $%04x ($%04x is outside CHR ROM range)\n", addr, chr_offset);
                exit(-1);
            }
            
            return cart->chr_rom[chr_offset];
        } else if (addr >= 0x2000 && addr <= 0x3EFF) {
            // name tables
            return ppu_name_table_read(addr % 0x1000);
        } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
            // palette table
            return ppu_palette_table_read(addr % 0x20);
        } else {
            // open bus, generally returns low address byte
            return addr & 0xFF;
        }
}

static void _mmc1_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // PRG ROM
        if (cart->chr_size == 0) {
            system_chr_ram_write(addr, val);
        }
        return;
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // name tables
        ppu_name_table_write(addr % 0x1000, val);
        return;
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // palette table
        ppu_palette_table_write(addr % 0x20, val);
        return;
    }
}

void mapper_init_mmc1(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_MMC1;
    memcpy(mapper->name, "MMC1", strlen("MMC1") + 1);
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *_mmc1_ram_read;
    mapper->ram_write_func  = *_mmc1_ram_write;
    mapper->vram_read_func  = *_mmc1_vram_read;
    mapper->vram_write_func = *_mmc1_vram_write;
    mapper->tick_func       = NULL;

    g_mmc1_control.prg_bank_mode = 3;

    ppu_set_mirroring_mode(MIRROR_SINGLE_LOWER);
}
