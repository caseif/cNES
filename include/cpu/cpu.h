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
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    unsigned char carry:1 PACKED;
    unsigned char zero:1 PACKED;
    unsigned char interrupt_disable:1 PACKED;
    unsigned char decimal:1 PACKED;
    unsigned char break_command:1 PACKED;
    unsigned char unused:1 PACKED;
    unsigned char overflow:1 PACKED;
    unsigned char negative:1 PACKED;
} StatusRegister;

typedef struct {
    StatusRegister status;
    uint16_t pc;
    uint8_t sp;
    uint8_t acc;
    uint8_t x;
    uint8_t y;
} CpuRegisters;

typedef struct {
    uint16_t vector_loc;
    bool maskable;
    bool push_pc;
    bool set_b;
    bool set_i;
} InterruptType;

extern const InterruptType INT_RESET;
extern const InterruptType INT_NMI;
extern const InterruptType INT_IRQ;
extern const InterruptType INT_BRK;

void initialize_cpu(void);

void cpu_init_pc(uint16_t addr);

uint8_t cpu_ram_read(uint16_t addr);

void cpu_ram_write(uint16_t addr, uint8_t val);

void cpu_start_oam_dma(uint8_t page);

void set_nmi_line();

void clear_nmi_line();

void issue_interrupt(const InterruptType *type);

void cycle_cpu(void);

void dump_ram(void);
