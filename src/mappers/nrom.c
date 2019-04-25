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
#include "cpu/cpu.h"
#include "input/input_device.h"
#include "mappers/mappers.h"
#include "ppu/ppu.h"

static uint8_t _nrom_ram_read(Cartridge *cart, uint16_t addr) {
    switch (addr) {
        case 0 ... 0x1FFF: {
            return cpu_ram_read(addr % 0x800);
        }
        case 0x2000 ... 0x3FFF: {
            return ppu_read_mmio((uint8_t) (addr % 8));
        }
        case 0x4014: {
            //TODO: DMA register
            return 0;
        }
        case 0x4000 ... 0x4013:
        case 0x4015: {
            //TODO: APU MMIO
            return 0;
        }
        case 0x4016 ... 0x4017: {
            return controller_poll(addr - 0x4016);
        }
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

static void _nrom_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0 ... 0x1FFF: {
            cpu_ram_write(addr % 0x800, val);
            return;
        }
        case 0x2000 ... 0x3FFF: {
            ppu_write_mmio((uint8_t) (addr % 8), val);
            return;
        }
        case 0x4014: {
            cpu_start_oam_dma(val);
            return;
        }
        case 0x4000 ... 0x4013:
        case 0x4015: {
            //TODO: APU MMIO
            return;
        }
        case 0x4016 ... 0x4017: {
            controller_push(addr - 0x4016, val);
            return;
        }
        case 0x6000 ... 0x6FFF: {
            /*g_debug_buffer[addr - 0x6000] = val;

            if (g_debug_buffer[1] == 0xDE && g_debug_buffer[2] == 0xB0 && g_debug_buffer[3]) {
                if (addr == 0x6000 && val != 0x80) {
                    printf("Error code %02x written\n", val);
                    if (g_debug_buffer[4]) {
                        printf("Error message: %s\n", (char*) (g_debug_buffer + 4));
                    }
                }
            }*/

            return;
        }
        case 0x8000 ... 0xFFFF: {
            // attempts to write to ROM fail silently
            return;
        }
    }
}

static uint8_t _nrom_vram_read(Cartridge *cart, uint16_t addr) {
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
        // unmapped
        default: {
            return 0;
        }
    }
}

static void _nrom_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    addr %= 0x4000;

    switch (addr) {
        // pattern tables
        case 0x0000 ... 0x1FFF: {
            //TODO: unsupported altogether for now
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
    mapper->ram_read_func   = *_nrom_ram_read;
    mapper->ram_write_func  = *_nrom_ram_write;
    mapper->vram_read_func  = *_nrom_vram_read;
    mapper->vram_write_func = *_nrom_vram_write;
}
