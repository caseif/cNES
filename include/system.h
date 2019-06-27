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

#define PRG_RAM_SIZE 0x2000

unsigned char g_prg_ram[PRG_RAM_SIZE];

void initialize_system(Cartridge *cart);

uint8_t system_ram_read(uint16_t addr);

void system_ram_write(uint16_t addr, uint8_t val);

uint8_t system_vram_read(uint16_t addr);

void system_vram_write(uint16_t addr, uint8_t val);

uint8_t system_lower_memory_read(uint16_t addr);

void system_lower_memory_write(uint16_t addr, uint8_t val);

void do_system_loop(void);

void break_execution(void);

void continue_execution(void);

void step_execution(void);

bool is_execution_halted(void);

void kill_execution(void);
