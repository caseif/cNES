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
#include "mappers/mappers.h"
#include "mappers/nrom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PRG_BANK_SHIFT 15
#define PRG_BANK_GRANULARITY (1 << PRG_BANK_SHIFT)

static unsigned char g_prg_bank;
static unsigned char g_nametable;

static uint8_t _axrom_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x8000) {
        return system_lower_memory_read(addr);
    }

    return cart->prg_rom[((g_prg_bank * PRG_BANK_GRANULARITY) | (addr % PRG_BANK_GRANULARITY)) % cart->prg_size];
}

static void _axrom_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x8000) {
        system_lower_memory_write(addr, val);
    }

    g_prg_bank = val & 0x7;
    g_nametable = (val >> 4) & 0x1;
}

static uint8_t _axrom_vram_read(Cartridge *cart, uint16_t addr) {
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        return nrom_vram_read(cart, (addr % 0x800) + (g_nametable ? 0x2800 : 0x2000));
    } else {
        return nrom_vram_read(cart, addr);
    }
}

static void _axrom_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr >= 0x2000 && addr <= 0x3EFF) {
        nrom_vram_write(cart, (addr % 0x800) + (g_nametable ? 0x2800 : 0x2000), val);
    } else {
        nrom_vram_write(cart, addr, val);
    }
}

void mapper_init_axrom(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_CNROM;
    memcpy(mapper->name, "AxROM", strlen("AxROM") + 1);
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *_axrom_ram_read;
    mapper->ram_write_func  = *_axrom_ram_write;
    mapper->vram_read_func  = *_axrom_vram_read;
    mapper->vram_write_func = *_axrom_vram_write;
    mapper->tick_func       = NULL;
}
