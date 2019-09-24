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

#define PRG_BANK_GRANULARITY 0x4000

#define CHR_RAM_SIZE 0x2000

static unsigned char g_prg_bank;
static unsigned char g_chr_bank;

static uint8_t _color_dreams_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr >= 0x8000) {
        return cart->prg_rom[((g_prg_bank << 15) | (addr - 0x8000)) % cart->prg_size];
    } else {
        return nrom_ram_read(cart, addr);
    }

}

static void _color_dreams_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr >= 0x8000) {
        g_prg_bank = val & 3;
        g_chr_bank = val >> 4;
    } else {
        nrom_ram_write(cart, addr, val);
    }
}

static uint8_t _color_dreams_vram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x2000) {
        return cart->chr_rom[((g_chr_bank << 13) | addr) % cart->chr_size];
    } else {
        return nrom_vram_read(cart, addr);
    }
}

void mapper_init_color_dreams(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_COLOR_DREAMS;
    memcpy(mapper->name, "Color Dreams", strlen("Color Dreams"));
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *_color_dreams_ram_read;
    mapper->ram_write_func  = *_color_dreams_ram_write;
    mapper->vram_read_func  = *_color_dreams_vram_read;
    mapper->vram_write_func = *nrom_vram_write;
    mapper->tick_func       = NULL;
}
