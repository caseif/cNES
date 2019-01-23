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
#include "cpu/instrs.h"

#include <stdio.h>

#define SYSTEM_MEMORY 2048
#define STACK_BOTTOM_ADDR 0x100
#define BASE_PC 0x8000

static Cartridge *g_cartridge;

static CpuRegisters g_regs;
static unsigned char g_sys_memory[SYSTEM_MEMORY];

void initialize_cpu(void) {
    g_regs.pc = BASE_PC;
}

void load_cartridge(Cartridge *cartridge) {
    g_cartridge = cartridge;
}

uint8_t memory_read(uint16_t addr) {
    if (addr < 0x2000) {
        return g_sys_memory[addr % 0x800];
    } else if (addr < 0x4000) {
        return 0; //TODO: read from PPU MMIO
    } else if (addr < 0x4020) {
        if (addr == 0x4014) {
            //TODO: I think this is supposed to return the PPU latch value
            return 0;
        } else {
            return 0; //TODO
        }
    } else if (addr < 0x8000) {
        return 0; //TODO
    } else {
        addr -= 0x8000;
        // ROM is mirrored if cartridge only has 1 bank
        if (g_cartridge->prg_size <= 16384) {
            addr %= 0x4000;
        }
        return g_cartridge->prg_rom[addr];
    }
}

void memory_write(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        g_sys_memory[addr % 0x800] = val;
    } else if (addr < 0x4000) {
        //TODO: handle write to PPU MMIO
    } else if (addr < 0x4020) {
        if (addr == 0x4014) {
            //TODO: write to PPU DMA register
        } else {
            //TODO
        }
    }

    // attempts to write to ROM fail silently
}

void stack_push(char val) {
    g_sys_memory[STACK_BOTTOM_ADDR + g_regs.sp--] = val;
}

char stack_pop(void) {
    return g_sys_memory[STACK_BOTTOM_ADDR + g_regs.sp++];
}

static char _next_prg_byte(void) {
    return memory_read(g_regs.pc++);
}

static uint16_t _next_prg_short(void) {
    uint16_t val = *((uint16_t*) &(g_cartridge->prg_rom[g_regs.pc]));
    g_regs.pc += 2;
    return val;
}

void exec_next_instr(void) {
    Instruction *instr = decode_instr(_next_prg_byte());
    printf("decoded instr %s:%s @ $%x\n", mnemonic_to_str(instr->mnemonic), addr_mode_to_str(instr->addr_mode), g_regs.pc - 1);

    g_regs.pc += (get_instr_len(instr->addr_mode) - 1);
}
