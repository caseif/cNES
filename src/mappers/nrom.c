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
#include "ppu.h"

#include <string.h>

uint8_t nrom_ram_read(Cartridge *cart, uint16_t addr) {
    switch (addr) {
        case 0x0000 ... 0x7FFF:
            return system_lower_memory_read(addr);
        case 0x8000 ... 0xFFFF: {
            uint16_t adj_addr = addr - 0x8000;
            // ROM is mirrored if cartridge only has 1 bank
            if (cart->prg_size <= 16384) {
                adj_addr %= 0x4000;
            }
            return cart->prg_rom[adj_addr];
        }
        default: {
            // nothing here
            return 0;
        }
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

    switch (addr) {
        // pattern tables
        case 0x0000 ... 0x1FFF: {
            return cart->chr_rom[addr];
        }
        // name tables
        case 0x2000 ... 0x3EFF: {
            return ppu_name_table_read(addr % 0x1000);
        }
        case 0x3F00 ... 0x3FFF: {
            return ppu_palette_table_read(addr % 0x20);
        }
        // open bus, generally returns low address byte
        default: {
            return addr & 0xFF;
        }
    }
}

void nrom_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    addr %= 0x4000;

    switch (addr) {
        // pattern tables
        case 0x0000 ... 0x1FFF: {
            break;
        }
        // name tables
        case 0x2000 ... 0x3EFF: {
            ppu_name_table_write(addr % 0x1000, val);
            break;
        }
        case 0x3F00 ... 0x3FFF: {
            ppu_palette_table_write(addr % 0x20, val);
            break;
        }
        // unmapped
        default: {
            return;
        }
    }
}   

void mapper_init_nrom(Mapper *mapper) {
    mapper->id = MAPPER_ID_NROM;
    memcpy(mapper->name, "NROM", strlen("NROM"));
    mapper->init_func       = NULL;
    mapper->ram_read_func   = *nrom_ram_read;
    mapper->ram_write_func  = *nrom_ram_write;
    mapper->vram_read_func  = *nrom_vram_read;
    mapper->vram_write_func = *nrom_vram_write;
    mapper->tick_func       = NULL;
}
