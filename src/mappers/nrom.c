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
#include "ppu.h"

#include <string.h>

#define CHR_RAM_SIZE 0x2000

// pretty sure this never existed in hardware, but some of blargg's tests rely on it
static unsigned char g_chr_ram[CHR_RAM_SIZE];

uint8_t nrom_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr >= 0x0000 && addr <= 0x7FFF) {
        return system_lower_memory_read(addr);
    } else if (addr >= 0x8000 && addr <= 0xFFFF) {
        uint16_t adj_addr = addr - 0x8000;
        // ROM is mirrored if cartridge only has 1 bank
        if (cart->prg_size <= 16384) {
            adj_addr %= 0x4000;
        }
        return cart->prg_rom[adj_addr];
    } else {
        // nothing here
        return 0;
    }
}

void nrom_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x8000) {
        system_lower_memory_write(addr, val);
    }

    // otherwise, attempts to write to ROM fail silently
    return;
}

uint8_t nrom_vram_read(Cartridge *cart, uint16_t addr) {
    addr %= 0x4000;

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // pattern tables
        if (cart->chr_size == 0) {
            return g_chr_ram[addr];
        }

        if (addr < cart->chr_size) {
            return cart->chr_rom[addr];
        } else {
            return addr & 0xFF; // open bus (typically the lower address byte)
        }
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // name tables
        return ppu_name_table_read(addr % 0x1000);
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        return ppu_palette_table_read(addr % 0x20);
    } else {
        // open bus, generally returns low address byte
        return addr & 0xFF;
    }
}

void nrom_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    addr %= 0x4000;

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // pattern tables
        if (cart->chr_size == 0) {
            g_chr_ram[addr] = val;
        }

    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // name tables
        ppu_name_table_write(addr % 0x1000, val);
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // name tables
        ppu_palette_table_write(addr % 0x20, val);
    } else {
        // unmapped
        return;
    }
}   

void mapper_init_nrom(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_NROM;
    memcpy(mapper->name, "NROM", strlen("NROM") + 1);
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *nrom_ram_read;
    mapper->ram_write_func  = *nrom_ram_write;
    mapper->vram_read_func  = *nrom_vram_read;
    mapper->vram_write_func = *nrom_vram_write;
    mapper->tick_func       = NULL;
}
