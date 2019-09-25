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

#pragma once

#include "cartridge.h"

#define SYSTEM_MEMORY_SIZE 0x800
#define PRG_RAM_SIZE 0x2000
#define CHR_RAM_SIZE 0x2000

void initialize_system(Cartridge *cart);

uint8_t system_bus_read(void);

void system_bus_write(uint8_t val);

uint8_t system_prg_ram_read(uint16_t addr);

void system_prg_ram_write(uint16_t addr, uint8_t val);

uint8_t system_chr_ram_read(uint16_t addr);

void system_chr_ram_write(uint16_t addr, uint8_t val);

void system_register_chip_ram(Cartridge *cart, unsigned char *ram, size_t size);

void system_ram_init(void);

uint8_t system_ram_read(uint16_t addr);

void system_ram_write(uint16_t addr, uint8_t val);

uint8_t system_memory_read(uint16_t addr);

void system_memory_write(uint16_t addr, uint8_t val);

uint8_t system_vram_read(uint16_t addr);

void system_vram_write(uint16_t addr, uint8_t val);

uint8_t system_lower_memory_read(uint16_t addr);

void system_lower_memory_write(uint16_t addr, uint8_t val);

void system_dump_ram(void);

void system_start_oam_dma(uint8_t page);

void do_system_loop(void);

void break_execution(void);

void continue_execution(void);

void step_execution(void);

bool is_execution_halted(void);

void kill_execution(void);
