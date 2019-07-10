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

#include "cpu/cpu.h"
#include "ppu.h"
#include "state.h"
#include "system.h"

void _compute_sha256(Cartridge *cart, unsigned char *buf) {
    //TODO
}

SaveState *create_save_state(Cartridge *cart) {
    SaveState *state = malloc(sizeof(SaveState));

    memcpy(state->magic, STATE_MAGIC, sizeof(state->magic));

    _compute_sha256(cart, state->cart_sha256);
    
    memcpy((void*) state->sys_mem, system_get_ram(), 0);
    memcpy((void*) state->vram, NULL, 0);
    memcpy((void*) state->prg_ram, NULL, 0);
    memcpy((void*) state->chr_ram, NULL, 0);

    memcpy(&state->cpu_regs, (void*) cpu_get_regs(), sizeof(CpuRegisters));
    memcpy(&state->ppu_ctrl, (void*) ppu_get_control_regs(), sizeof(PpuControl));
    memcpy(&state->ppu_mask, (void*) ppu_get_mask_regs(), sizeof(PpuMask));
    memcpy(&state->ppu_status, (void*) ppu_get_status_regs(), sizeof(PpuStatus));
    memcpy(&state->ppu_internal_regs, (void*) ppu_get_internal_regs(), sizeof(PpuInternalRegisters));

    state->opcode_reg;
    state->cur_operand;
    state->latched_val;
    state->addr_bus;
    state->data_bus;
    state->instr_cycle;
    state->burn_cycles;

    state->queued_int;
    state->cur_int;

    state->cycle_index;
    state->total_cycle_count;
    state->ppu_scanline;
    state->ppu_scanline_tick;

    state->mapper_regs;
}
