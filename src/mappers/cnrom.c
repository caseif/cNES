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

#define CHR_BANK_GRANULARITY 0x2000

#define CHR_RAM_SIZE 0x2000

static unsigned char g_chr_bank;

static uint32_t _cnrom_get_chr_offset(Cartridge *cart, uint16_t addr) {
    assert(addr < 0x2000);

    return ((g_chr_bank * CHR_BANK_GRANULARITY) | (addr % CHR_BANK_GRANULARITY)) % cart->chr_size;
}

static uint8_t _cnrom_vram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x2000) {
        return cart->chr_rom[_cnrom_get_chr_offset(cart, addr)];
    } else {
        return nrom_vram_read(cart, addr);
    }
}

void mapper_init_cnrom(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_CNROM;
    memcpy(mapper->name, "CNROM", strlen("CNROM") + 1);
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *nrom_ram_read;
    mapper->ram_write_func  = *nrom_ram_write;
    mapper->vram_read_func  = *_cnrom_vram_read;
    mapper->vram_write_func = *nrom_vram_write;
    mapper->tick_func       = NULL;
}
