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

static uint8_t _mmc3_ram_read(Cartridge *cart, uint16_t addr) {
    return 0; //TODO
}

static void _mmc3_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    //TODO
}

static uint8_t _mmc3_vram_read(Cartridge *cart, uint16_t addr) {
    return 0; //TODO
}

static void _mmc3_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    //TODO
}

void mapper_init_mmc3(Mapper *mapper) {
    mapper->ram_read_func   = *_mmc3_ram_read;
    mapper->ram_write_func  = *_mmc3_ram_write;
    mapper->vram_read_func  = *_mmc3_vram_read;
    mapper->vram_write_func = *_mmc3_vram_write;
}
