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
#include "mappers/mappers.h"
#include "mappers/nrom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PRG_BANK_GRANULARITY 0x4000

#define CHR_RAM_SIZE 0x2000

static unsigned char g_prg_bank;

static unsigned char chr_ram[CHR_RAM_SIZE];

static void _unrom_init(Cartridge *cart) {
    memcpy(chr_ram, cart->chr_rom, cart->chr_size < CHR_RAM_SIZE ? cart->chr_size : CHR_RAM_SIZE);
}

static uint32_t _unrom_get_prg_offset(Cartridge *cart, uint16_t addr) {
    assert(addr >= 0x8000);

    uint8_t bank;

    if (addr >= 0xC000) {
        // upper half
        bank = cart->prg_size / PRG_BANK_GRANULARITY - 1;
    } else {
        // lower half
        bank = g_prg_bank;
    }

    return ((bank * PRG_BANK_GRANULARITY) | (addr % PRG_BANK_GRANULARITY)) % cart->prg_size;
}

static uint8_t _unrom_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x6000) {
        return system_lower_memory_read(addr);
    } else if (addr < 0x8000) {
        return 0;
    }

    uint32_t prg_offset = _unrom_get_prg_offset(cart, addr);

    if (prg_offset >= cart->prg_size) {
        printf("Invalid PRG read from $%04x ($%04x is outside PRG ROM range)\n", addr, prg_offset);
        exit(-1);
    }

    return cart->prg_rom[prg_offset];
}

static void _unrom_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x6000) {
        system_lower_memory_write(addr, val);
    } else if (addr >= 0x8000) {
        g_prg_bank = val;
    }
}

static uint8_t _unrom_vram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x2000) {
        return chr_ram[addr];
    } else {
        return nrom_vram_read(cart, addr);
    }
}

static void _unrom_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        chr_ram[addr] = val;
    } else {
        nrom_vram_write(cart, addr, val);
    }
}

void mapper_init_unrom(Mapper *mapper) {
    mapper->init_func       = *_unrom_init;
    mapper->ram_read_func   = *_unrom_ram_read;
    mapper->ram_write_func  = *_unrom_ram_write;
    mapper->vram_read_func  = *_unrom_vram_read;
    mapper->vram_write_func = *_unrom_vram_write;
    mapper->tick_func       = NULL;
}
