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

#include "cpu/cpu.h"
#include "ppu.h"

#define STATE_MAGIC "CNES"

typedef struct {
    char magic[4];

    unsigned char cart_sha256[32]; // for validation

    unsigned char sys_mem[0x800];
    unsigned char vram[0x2000];
    unsigned char oam_primary[0];
    unsigned char oam_secondary[0];
    unsigned char prg_ram[0x2000];
    unsigned char chr_ram[0x2000];

    CpuRegisters cpu_regs;
    PpuControl ppu_ctrl;
    PpuMask ppu_mask;
    PpuStatus ppu_status;
    PpuInternalRegisters ppu_internal_regs;

    uint32_t opcode_reg;
    uint16_t cur_operand;
    uint8_t latched_val;
    uint16_t addr_bus;
    uint8_t data_bus;
    uint32_t instr_cycle;
    uint16_t burn_cycles; //TODO: remove

    uint8_t queued_int; // queued interrupt
    uint8_t cur_int; // current interrupt

    uint8_t cycle_index;
    uint32_t total_cycle_count;
    uint32_t ppu_scanline;
    uint32_t ppu_scanline_tick;

    unsigned char mapper_regs[64];
} SaveState;

SaveState *create_save_state(Cartridge *cart);
