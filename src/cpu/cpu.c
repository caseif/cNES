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
#include <stdlib.h>
#include <string.h>

#define SYSTEM_MEMORY 2048
#define STACK_BOTTOM_ADDR 0x100
#define BASE_PC 0x8000

typedef struct {
    uint16_t value;
    uint16_t src_addr;
} InstructionParameter;

const InterruptType *INT_RESET = &(InterruptType) {0xFFFA, false, true,  false, false};
const InterruptType *INT_NMI   = &(InterruptType) {0xFFFC, false, false, false, false};
const InterruptType *INT_IRQ   = &(InterruptType) {0xFFFE, true,  true,  false, true};
const InterruptType *INT_BRK   = &(InterruptType) {0xFFFE, false, true,  true,  true};

static Cartridge *g_cartridge;

static CpuRegisters g_regs;
static unsigned char g_sys_memory[SYSTEM_MEMORY];

void initialize_cpu(void) {
    g_regs.pc = BASE_PC;
    memset(g_sys_memory, '\0', SYSTEM_MEMORY);
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

/**
 * Returns value M along with the address it was read from, if applicable.
 */
static InstructionParameter _get_next_m(AddressingMode mode) {
    switch (mode) {
        case IMM: {
            return (InstructionParameter) {_next_prg_byte(), 0};
        }
        case REL: {
            return (InstructionParameter) {_next_prg_byte(), 0};
        }
        case ZRP: {
            uint16_t addr = _next_prg_byte();
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case ZPX: {
            uint16_t addr = _next_prg_byte();
            addr += g_regs.x;
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case ZPY: {
            uint16_t addr = _next_prg_byte();
            addr += g_regs.y;
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case ABS: {
            uint16_t addr = _next_prg_short();
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case ABX: {
            uint16_t addr = (g_regs.x + _next_prg_short());
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case ABY: {
            uint16_t addr = (g_regs.y + _next_prg_short());
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case IND: {
            uint16_t orig_addr = _next_prg_short();
            uint8_t addr_low = memory_read(orig_addr);
            uint8_t addr_high = memory_read(orig_addr + 1);
            uint16_t addr = (addr_low | (addr_high << 8));
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case IZX: {
            uint16_t orig_addr = (g_regs.x + _next_prg_byte());
            uint8_t addr_low = memory_read(orig_addr);
            uint8_t addr_high = memory_read(orig_addr + 1);
            uint16_t addr = (addr_low | (addr_high << 8));
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case IZY: {
            uint16_t orig_addr = _next_prg_byte();
            uint8_t addr_low = memory_read(orig_addr);
            uint8_t addr_high = memory_read(orig_addr + 1);
            uint16_t addr = (g_regs.y + (addr_low | (addr_high << 8)));
            return (InstructionParameter) {memory_read(addr), addr};
        }
        case IMP: {
            return (InstructionParameter) {0, 0};
        }
        default: {
            printf("Unhandled addressing mode %s", addr_mode_to_str(mode));
            exit(-1);
        }
    }
}

static void _do_branch(int8_t offset) {
    g_regs.pc += offset;
}

static void _set_alu_flags(uint16_t val) {
    g_regs.status.zero = val ? 0 : 1;
    g_regs.status.negative = (val & 0x80) ? 1 : 0;
}

static void _do_shift(uint8_t m, uint16_t src_addr, bool implicit, bool right, bool rotate) {
    // fetch the target value either from the accumulator or from memory
    uint8_t val = implicit ? g_regs.acc : m;

    // rotation mask - contains information about the carry bit
    uint8_t rMask = 0;
    if (rotate) {
        // set if only if we're rotating
        rMask = g_regs.status.carry;
        if (right) {
            // if we're rotating to the right, the carry bit gets copied to bit 7
            rMask <<= 7;
        }
    }

    // carry mask - set to the bit which will be copied to the carry flag
    uint8_t cMask = right ? 0x01 : 0x80;
    g_regs.status.carry = (val & cMask) ? 1 : 0;

    // compute the result by shifting and applying the rotation mask
    uint8_t res = (right ? (g_regs.acc >> 1) : (g_regs.acc << 1)) | rMask;

    // set the zero and negative flags based on the result
    _set_alu_flags(res);

    // write the result to either the accumulator or to memory, depending on the addressing mode
    if (implicit) {
        g_regs.acc = res;
    } else {
        memory_write(src_addr, res);
    }
}

static void _do_cmp(uint8_t reg, uint16_t m) {
    if (reg >= 0x80) {
        g_regs.status.negative = 1;
    }

    if (reg >= m) {
        g_regs.status.carry = 1;
        g_regs.status.zero = reg == m ? 1 : 0;
    } else {
        g_regs.status.carry = 0;
    }
}

void issue_interrupt(const InterruptType *type) {
        // check if the interrupt should be masked
        if (type->maskable && g_regs.status.interrupt_disable) {
            return;
        }

        // push PC and P
        if (type->push_pc) {
            stack_push(g_regs.pc >> 8);      // push MSB
            stack_push(g_regs.pc & 0xFF);    // push LSB

            uint8_t status_serial;
            memcpy(&status_serial, &g_regs.status, 1);

            stack_push(status_serial);;
        }

        // set B flag
        if (type->set_b) {
            g_regs.status.break_command = 1;
        }

        // set I flag
        if (type->set_i) {
            g_regs.status.interrupt_disable = 1;
        }

        // little-Endian, so the LSB comes first
        uint16_t vector = memory_read(type->vector_loc) | ((memory_read(type->vector_loc + 1)) << 8);

        // set the PC
        g_regs.pc = vector;
    }

void _exec_instr(const Instruction *instr, InstructionParameter param) {
    uint16_t m = param.value;
    uint16_t addr = param.src_addr;

    switch (instr->mnemonic) {
        // storage
        case LDA:
            g_regs.acc = m;

            _set_alu_flags(g_regs.acc);

            break;
        case LDX:
            g_regs.x = m;

            _set_alu_flags(g_regs.x);

            break;
        case LDY:
            g_regs.y = m;

            _set_alu_flags(g_regs.y);

            break;
        case STA:
            memory_write(addr, g_regs.acc);
            break;
        case STX:
            memory_write(addr, g_regs.x);
            break;
        case STY:
            memory_write(addr, g_regs.y);
            break;
        case TAX:
            g_regs.x = g_regs.acc;

            _set_alu_flags(g_regs.x);

            break;
        case TAY:
            g_regs.y = g_regs.acc;

            _set_alu_flags(g_regs.y);

            break;
        case TSX:
            g_regs.x = g_regs.sp;

            _set_alu_flags(g_regs.x);

            break;
        case TXA:
            g_regs.acc = g_regs.x;

            _set_alu_flags(g_regs.acc);

            break;
        case TYA:
            g_regs.acc = g_regs.y;

            _set_alu_flags(g_regs.acc);

            break;
        case TXS:
            g_regs.sp = g_regs.x;
            break;
        // math
        case ADC: {
            uint8_t acc0 = g_regs.acc;

            g_regs.acc = (acc0 + m + g_regs.status.carry);

            _set_alu_flags(g_regs.acc);

            uint8_t a7 = (acc0 >> 7);
            uint8_t m7 = (m >> 7);

            // unsigned overflow will occur if at least two among the most significant operand bits and the carry bit are set
            g_regs.status.carry = ((a7 & m7) | (a7 & g_regs.status.carry) | (m7 & g_regs.status.carry)) ? 1 : 0;

            // signed overflow will occur if the sign of both inputs if different from the sign of the result
            g_regs.status.overflow = ((acc0 ^ g_regs.acc) & (m ^ g_regs.acc) & 0x80) ? 1 : 0;

            break;
        }
        case SBC: {
            uint8_t acc0 = g_regs.acc;

            uint8_t not_carry = g_regs.status.carry ? 0 : 1;

            g_regs.acc = (acc0 - m - not_carry);

            _set_alu_flags(g_regs.acc);

            g_regs.status.carry = ((acc0 >> 7) + ((0xFF - m) >> 7) + (((g_regs.acc & 0x40) & ((0xFF - m) & 0x40)) >> 6) <= 1) ? 1 : 0;

            g_regs.status.overflow = ((acc0 ^ g_regs.acc) & ((0xFF - m) ^ g_regs.acc) & 0x80) ? 1 : 0;

            break;
        }
        case DEC: {
            uint16_t decRes = m - 1;

            memory_write(addr, m - 1);

            _set_alu_flags(decRes);

            break;
        }
        case DEX:
            g_regs.x--;

            _set_alu_flags(g_regs.x);

            break;
        case DEY:
            g_regs.y--;

            _set_alu_flags(g_regs.y);

            break;
        case INC: {
            uint16_t incRes = m + 1;

            memory_write(addr, incRes);

            _set_alu_flags(incRes);

            break;
        }
        case INX:
            g_regs.x++;

            _set_alu_flags(g_regs.x);

            break;
        case INY:
            g_regs.y++;

            _set_alu_flags(g_regs.y);

            break;
        // logic
        case AND:
            g_regs.acc = g_regs.acc & m;

            _set_alu_flags(g_regs.acc);

            break;
        case ASL:
            _do_shift(m, addr, instr->addr_mode == IMP, false, false);
            break;
        case LSR:
            _do_shift(m, addr, instr->addr_mode == IMP, true, false);
            break;
        case EOR:
            g_regs.acc = g_regs.acc ^ m;

            _set_alu_flags(g_regs.acc);

            break;
        case ORA:
            g_regs.acc = g_regs.acc | m;

            _set_alu_flags(g_regs.acc);

            break;
        case ROL:
            _do_shift(m, addr, instr->addr_mode == IMP, false, true);

            _set_alu_flags(g_regs.acc);

            break;
        case ROR:
            _do_shift(m, addr, instr->addr_mode == IMP, true, true);

            _set_alu_flags(g_regs.acc);

            break;
        // branching
        case BCC:
            if (!g_regs.status.carry) {
                _do_branch(m);
            }
            break;
        case BCS:
            if (g_regs.status.carry) {
                _do_branch(m);
            }
            break;
        case BNE:
            if (!g_regs.status.zero) {
                _do_branch(m);
            }
            break;
        case BEQ:
            if (g_regs.status.zero) {
                _do_branch(m);
            }
            break;
        case BPL:
            if (!g_regs.status.negative) {
                _do_branch(m);
            }
            break;
        case BMI:
            if (g_regs.status.negative) {
                _do_branch(m);
            }
            break;
        case BVC:
            if (!g_regs.status.overflow) {
                _do_branch(m);
            }
            break;
        case BVS:
            if (g_regs.status.overflow) {
                _do_branch(m);
            }
            break;
        case JMP:
            g_regs.pc = addr;
            break;
        case JSR:
            stack_push((g_regs.pc >> 8) & 0xFF); // push MSB of PC
            stack_push(g_regs.pc & 0xFF);        // push LSB of PC

            g_regs.pc = addr;

            break;
        case RTS: {
            uint8_t pcl = stack_pop(); // pop LSB of PC
            uint8_t pcm = stack_pop(); // pop MSB of PC

            g_regs.pc = ((pcm << 8) | pcl) + 1;

            break;
        }
        // registers
        case CLC:
            g_regs.status.carry = 0;
            break;
        case CLD:
            g_regs.status.decimal = 0;
            break;
        case CLI:
            g_regs.status.interrupt_disable = 0;
            break;
        case CLV:
            g_regs.status.overflow = 0;
            break;
        case CMP:
            _do_cmp(g_regs.acc, m);
            break;
        case CPX:
            _do_cmp(g_regs.x, m);
            break;
        case CPY:
            _do_cmp(g_regs.y, m);
            break;
        case SEC:
            g_regs.status.carry = 1;
            break;
        case SED:
            g_regs.status.decimal = 1;
            break;
        case SEI:
            g_regs.status.interrupt_disable = 1;
            break;
        // stack
        case PHA:
            stack_push(g_regs.acc);
            break;
        case PHP: {
            uint8_t serial;
            memcpy(&serial, &g_regs.status, 1);
            stack_push(serial);
            break;
        }
        case PLA:
            g_regs.acc = stack_pop();

            _set_alu_flags(g_regs.acc);

            break;
        case PLP: {
            uint8_t serial = stack_pop();
            memcpy(&g_regs.status, &serial, 1);
            break;
        }
        // system
        case BRK: {
            issue_interrupt(INT_BRK);
            break;
        }
        case RTI: {
            // pop flags
            uint8_t status_serial = stack_pop();
            memcpy(&g_regs.status, &status_serial, 1);

            // ORDER IS IMPORTANT
            g_regs.pc = stack_pop() | (stack_pop() << 8);

            break;
        }
        case NOP:
            // no-op
            break;
        case KIL:
            printf("Encountered KIL instruction @ $%x", g_regs.pc - 1);
            exit(-1);
        default:
            //TODO
            // no-op
            break;
        //default:
        //    throw new UnsupportedOperationException("Unsupported instruction " + instr.getOpcode().name());
    }

    if (g_regs.pc - 0x8000 >= g_cartridge->prg_size) {
        printf("Execution address exceeded PRG-ROM bounds (pc=$%x)\n", g_regs.pc);
        exit(-1);
    }
}

void exec_next_instr(void) {
    const Instruction *instr = decode_instr(_next_prg_byte());

    InstructionParameter param = _get_next_m(instr->addr_mode);

    printf("decoded instr %s:%s with param $%x (addr $%x) @ $%x\n",
            mnemonic_to_str(instr->mnemonic), addr_mode_to_str(instr->addr_mode), param.value, param.src_addr, g_regs.pc - get_instr_len(instr));

    _exec_instr(instr, param);
}


