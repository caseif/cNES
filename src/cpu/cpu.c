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
#include "ppu/ppu.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSTEM_MEMORY 2048
#define STACK_BOTTOM_ADDR 0x100
#define BASE_PC 0x8000
#define BASE_SP 0xFF
#define DEFAULT_STATUS 0x20 // unused flag is set by default

const InterruptType *INT_RESET = &(InterruptType) {0xFFFA, false, true,  false, false};
const InterruptType *INT_NMI   = &(InterruptType) {0xFFFC, false, false, false, false};
const InterruptType *INT_IRQ   = &(InterruptType) {0xFFFE, true,  true,  false, true};
const InterruptType *INT_BRK   = &(InterruptType) {0xFFFE, false, true,  true,  true};

static Cartridge *g_cartridge;

static DataBlob g_prg_rom;

unsigned char g_sys_memory[SYSTEM_MEMORY];
CpuRegisters g_cpu_regs;

uint8_t g_burn_cycles = 0;

void initialize_cpu(void) {
    g_cpu_regs.pc = BASE_PC;
    g_cpu_regs.sp = BASE_SP;
    memset(&g_cpu_regs.status, DEFAULT_STATUS, 1);
    memset(g_sys_memory, '\0', SYSTEM_MEMORY);
}

void load_cartridge(Cartridge *cartridge) {
    g_cartridge = cartridge;
    load_program((DataBlob) {cartridge->prg_rom, cartridge->prg_size});
}

void load_program(DataBlob program_blob) {
    g_prg_rom = program_blob;
}

uint8_t memory_read(uint16_t addr) {
    switch (addr) {
        case 0 ... 0x1FFF: {
            return g_sys_memory[addr % 0x800];
        }
        case 0x2000 ... 0x2007: {
            return read_ppu_mmio((uint8_t) (addr - 0x2000));
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
            //TODO: controller input
            return 0;
        }
        case 0x8000 ... 0xFFFF: {
            addr -= 0x8000;
            // ROM is mirrored if cartridge only has 1 bank
            if (g_prg_rom.size <= 16384) {
                addr %= 0x4000;
            }
            return g_prg_rom.data[addr];
        }
        default: {
            // nothing here
            return 0;
        }
    }
}

void memory_write(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0 ... 0x1FFF: {
            g_sys_memory[addr % 0x800] = val;
            return;
        }
        case 0x2000 ... 0x2007: {
            write_ppu_mmio((uint8_t) (addr - 0x2000), val);
            return;
        }
        case 0x4014: {
            //TODO: DMA register
            return;
        }
        case 0x4000 ... 0x4013:
        case 0x4015: {
            //TODO: APU MMIO
            return;
        }
        case 0x4016 ... 0x4017: {
            //TODO: controller input
            return;
        }
        case 0x8000 ... 0xFFFF: {
            // attempts to write to ROM fail silently
            return;
        }
    }
}

void stack_push(char val) {
    g_sys_memory[STACK_BOTTOM_ADDR + g_cpu_regs.sp--] = val;
}

unsigned char stack_pop(void) {
    return g_sys_memory[STACK_BOTTOM_ADDR + ++g_cpu_regs.sp];
}

static unsigned char _next_prg_byte(void) {
    return memory_read(g_cpu_regs.pc++);
}

static uint16_t _next_prg_short(void) {
    return _next_prg_byte() + (_next_prg_byte() << 8);
}

/**
 * Returns value M along with the address it was read from, if applicable.
 */
static InstructionParameter _get_next_m(AddressingMode mode) {
    switch (mode) {
        case IMM: {
            uint8_t val = _next_prg_byte();
            return (InstructionParameter) {val, val, 0};
        }
        case REL: {
            uint8_t val = _next_prg_byte();
            return (InstructionParameter) {val, val, 0};
        }
        case ZRP: {
            uint8_t addr = _next_prg_byte();
            return (InstructionParameter) {memory_read(addr), addr, addr};
        }
        case ZPX: {
            uint8_t base_addr = _next_prg_byte();
            uint8_t addr = base_addr + g_cpu_regs.x;
            return (InstructionParameter) {memory_read(addr), base_addr, addr};
        }
        case ZPY: {
            uint8_t base_addr = _next_prg_byte();
            uint8_t addr = base_addr + g_cpu_regs.y;
            return (InstructionParameter) {memory_read(addr), base_addr, addr};
        }
        case ABS: {
            uint16_t addr = _next_prg_short();
            return (InstructionParameter) {memory_read(addr), addr, addr};
        }
        case ABX: {
            uint16_t base_addr = _next_prg_short();
            uint16_t addr = g_cpu_regs.x + base_addr;
            return (InstructionParameter) {memory_read(addr), base_addr, addr};
        }
        case ABY: {
            uint16_t base_addr = _next_prg_short();
            uint16_t addr = g_cpu_regs.y + base_addr;
            return (InstructionParameter) {memory_read(addr), base_addr, addr};
        }
        case IND: {
            uint16_t orig_addr = _next_prg_short();
            uint8_t addr_low = memory_read(orig_addr);
            // if the indirect target is the last byte of a page, the target
            // high byte will incorrectly be read from the first byte of the
            // same page
            uint16_t high_target = ((orig_addr & 0xFF) == 0xFF) ? (orig_addr - 0xFF) : (orig_addr + 1);
            uint8_t addr_high = memory_read(high_target);
            uint16_t addr = (addr_low | (addr_high << 8));
            return (InstructionParameter) {memory_read(addr), orig_addr, addr};
        }
        case IZX: {
            uint16_t base_addr = _next_prg_byte();
            uint16_t orig_addr = (g_cpu_regs.x + base_addr);
            uint8_t addr_low = memory_read(orig_addr);
            uint8_t addr_high = memory_read(orig_addr + 1);
            uint16_t addr = (addr_low | (addr_high << 8));
            return (InstructionParameter) {memory_read(addr), base_addr, addr};
        }
        case IZY: {
            uint16_t orig_addr = _next_prg_byte();
            uint8_t addr_low = memory_read(orig_addr);
            uint8_t addr_high = memory_read(orig_addr + 1);
            uint16_t addr = (g_cpu_regs.y + (addr_low | (addr_high << 8)));
            return (InstructionParameter) {memory_read(addr), orig_addr, addr};
        }
        case IMP: {
            return (InstructionParameter) {0, 0, 0};
        }
        default: {
            printf("Unhandled addressing mode %s", addr_mode_to_str(mode));
            exit(-1);
        }
    }
}

static void _do_branch(int8_t offset) {
    g_cpu_regs.pc += offset;
    g_burn_cycles += 1; // taking a branch incurs a 1-cycle penalty
}

static void _set_alu_flags(uint16_t val) {
    g_cpu_regs.status.zero = val ? 0 : 1;
    g_cpu_regs.status.negative = (val & 0x80) ? 1 : 0;
}

static void _do_shift(uint8_t m, uint16_t src_addr, bool implicit, bool right, bool rotate) {
    // fetch the target value either from the accumulator or from memory
    uint8_t val = implicit ? g_cpu_regs.acc : m;

    // rotation mask - contains information about the carry bit
    uint8_t r_mask = 0;
    if (rotate) {
        // set if only if we're rotating
        r_mask = g_cpu_regs.status.carry;
        if (right) {
            // if we're rotating to the right, the carry bit gets copied to bit 7
            r_mask <<= 7;
        }
    }

    // carry mask - set to the bit which will be copied to the carry flag
    uint8_t c_mask = right ? 0x01 : 0x80;
    g_cpu_regs.status.carry = (val & c_mask) ? 1 : 0;

    // compute the result by shifting and applying the rotation mask
    uint8_t res = (right ? (g_cpu_regs.acc >> 1) : (g_cpu_regs.acc << 1)) | r_mask;

    // set the zero and negative flags based on the result
    _set_alu_flags(res);

    // write the result to either the accumulator or to memory, depending on the addressing mode
    if (implicit) {
        g_cpu_regs.acc = res;
    } else {
        memory_write(src_addr, res);
    }
}

static void _do_cmp(uint8_t reg, uint16_t m) {
    if (reg >= 0x80) {
        g_cpu_regs.status.negative = 1;
    }

    if (reg >= m) {
        g_cpu_regs.status.carry = 1;
        g_cpu_regs.status.zero = reg == m ? 1 : 0;
    } else {
        g_cpu_regs.status.carry = 0;
    }
}

void issue_interrupt(const InterruptType *type) {
        // check if the interrupt should be masked
        if (type->maskable && g_cpu_regs.status.interrupt_disable) {
            return;
        }

        // push PC and P
        if (type->push_pc) {
            stack_push(g_cpu_regs.pc >> 8);      // push MSB
            stack_push(g_cpu_regs.pc & 0xFF);    // push LSB

            uint8_t status_serial;
            memcpy(&status_serial, &g_cpu_regs.status, 1);

            stack_push(status_serial);;
        }

        // set B flag
        if (type->set_b) {
            g_cpu_regs.status.break_command = 1;
        }

        // set I flag
        if (type->set_i) {
            g_cpu_regs.status.interrupt_disable = 1;
        }

        // little-Endian, so the LSB comes first
        uint16_t vector = memory_read(type->vector_loc) | ((memory_read(type->vector_loc + 1)) << 8);

        // set the PC
        g_cpu_regs.pc = vector;
    }

void _exec_instr(const Instruction *instr, InstructionParameter param) {
    uint16_t m = param.value;
    uint16_t addr = param.src_addr;

    switch (instr->mnemonic) {
        // storage
        case LDA:
            g_cpu_regs.acc = m;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case LDX:
            g_cpu_regs.x = m;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case LDY:
            g_cpu_regs.y = m;

            _set_alu_flags(g_cpu_regs.y);

            break;
        case STA:
            memory_write(addr, g_cpu_regs.acc);
            break;
        case STX:
            memory_write(addr, g_cpu_regs.x);
            break;
        case STY:
            memory_write(addr, g_cpu_regs.y);
            break;
        case TAX:
            g_cpu_regs.x = g_cpu_regs.acc;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case TAY:
            g_cpu_regs.y = g_cpu_regs.acc;

            _set_alu_flags(g_cpu_regs.y);

            break;
        case TSX:
            g_cpu_regs.x = g_cpu_regs.sp;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case TXA:
            g_cpu_regs.acc = g_cpu_regs.x;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case TYA:
            g_cpu_regs.acc = g_cpu_regs.y;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case TXS:
            g_cpu_regs.sp = g_cpu_regs.x;
            break;
        // math
        case ADC: {
            uint8_t acc0 = g_cpu_regs.acc;

            g_cpu_regs.acc = (acc0 + m + g_cpu_regs.status.carry);

            _set_alu_flags(g_cpu_regs.acc);

            uint8_t a7 = (acc0 >> 7);
            uint8_t m7 = (m >> 7);

            // unsigned overflow will occur if at least two among the most significant operand bits and the carry bit are set
            g_cpu_regs.status.carry = ((a7 & m7) | (a7 & g_cpu_regs.status.carry) | (m7 & g_cpu_regs.status.carry));

            // signed overflow will occur if the sign of both inputs if different from the sign of the result
            g_cpu_regs.status.overflow = ((acc0 ^ g_cpu_regs.acc) & (m ^ g_cpu_regs.acc) & 0x80) ? 1 : 0;

            break;
        }
        case SBC: {
            uint8_t acc0 = g_cpu_regs.acc;

            g_cpu_regs.acc = (acc0 - m - !g_cpu_regs.status.carry);

            _set_alu_flags(g_cpu_regs.acc);

            g_cpu_regs.status.carry = g_cpu_regs.acc <= acc0;

            g_cpu_regs.status.overflow = ((acc0 ^ g_cpu_regs.acc) & ((0xFF - m) ^ g_cpu_regs.acc) & 0x80) ? 1 : 0;

            break;
        }
        case DEC: {
            uint16_t decRes = m - 1;

            memory_write(addr, m - 1);

            _set_alu_flags(decRes);

            break;
        }
        case DEX:
            g_cpu_regs.x--;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case DEY:
            g_cpu_regs.y--;

            _set_alu_flags(g_cpu_regs.y);

            break;
        case INC: {
            uint16_t incRes = m + 1;

            memory_write(addr, incRes);

            _set_alu_flags(incRes);

            break;
        }
        case INX:
            g_cpu_regs.x++;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case INY:
            g_cpu_regs.y++;

            _set_alu_flags(g_cpu_regs.y);

            break;
        // logic
        case AND:
            g_cpu_regs.acc = g_cpu_regs.acc & m;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case ASL:
            _do_shift(m, addr, instr->addr_mode == IMP, false, false);
            break;
        case LSR:
            _do_shift(m, addr, instr->addr_mode == IMP, true, false);
            break;
        case EOR:
            g_cpu_regs.acc = g_cpu_regs.acc ^ m;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case ORA:
            g_cpu_regs.acc = g_cpu_regs.acc | m;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case ROL:
            _do_shift(m, addr, instr->addr_mode == IMP, false, true);

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case ROR:
            _do_shift(m, addr, instr->addr_mode == IMP, true, true);

            _set_alu_flags(g_cpu_regs.acc);

            break;
        // branching
        case BCC:
            if (!g_cpu_regs.status.carry) {
                _do_branch(m);
            }
            break;
        case BCS:
            if (g_cpu_regs.status.carry) {
                _do_branch(m);
            }
            break;
        case BNE:
            if (!g_cpu_regs.status.zero) {
                _do_branch(m);
            }
            break;
        case BEQ:
            if (g_cpu_regs.status.zero) {
                _do_branch(m);
            }
            break;
        case BPL:
            if (!g_cpu_regs.status.negative) {
                _do_branch(m);
            }
            break;
        case BMI:
            if (g_cpu_regs.status.negative) {
                _do_branch(m);
            }
            break;
        case BVC:
            if (!g_cpu_regs.status.overflow) {
                _do_branch(m);
            }
            break;
        case BVS:
            if (g_cpu_regs.status.overflow) {
                _do_branch(m);
            }
            break;
        case JMP:
            g_cpu_regs.pc = addr;
            break;
        case JSR: {
            // on a real 6502, the PC gets pushed before it is moved past the
            // last byte of the JSR instruction
            uint16_t pc = g_cpu_regs.pc - 1;
            stack_push((pc >> 8) & 0xFF); // push MSB of PC
            stack_push(pc & 0xFF);        // push LSB of PC

            g_cpu_regs.pc = addr;

            break;
        }
        case RTS: {
            uint8_t pcl = stack_pop(); // pop LSB of PC
            uint8_t pcm = stack_pop(); // pop MSB of PC

            // we account for the PC pointing to the last byte of the JSR
            // instruction
            g_cpu_regs.pc = ((pcm << 8) | pcl) + 1;

            break;
        }
        // registers
        case CLC:
            g_cpu_regs.status.carry = 0;
            break;
        case CLD:
            g_cpu_regs.status.decimal = 0;
            break;
        case CLI:
            g_cpu_regs.status.interrupt_disable = 0;
            break;
        case CLV:
            g_cpu_regs.status.overflow = 0;
            break;
        case CMP:
            _do_cmp(g_cpu_regs.acc, m);
            break;
        case CPX:
            _do_cmp(g_cpu_regs.x, m);
            break;
        case CPY:
            _do_cmp(g_cpu_regs.y, m);
            break;
        case SEC:
            g_cpu_regs.status.carry = 1;
            break;
        case SED:
            g_cpu_regs.status.decimal = 1;
            break;
        case SEI:
            g_cpu_regs.status.interrupt_disable = 1;
            break;
        // stack
        case PHA:
            stack_push(g_cpu_regs.acc);
            break;
        case PHP: {
            uint8_t serial;
            memcpy(&serial, &g_cpu_regs.status, 1);
            stack_push(serial);
            break;
        }
        case PLA:
            g_cpu_regs.acc = stack_pop();

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case PLP: {
            uint8_t serial = stack_pop();
            memcpy(&g_cpu_regs.status, &serial, 1);
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
            memcpy(&g_cpu_regs.status, &status_serial, 1);

            // ORDER IS IMPORTANT
            g_cpu_regs.pc = stack_pop() | (stack_pop() << 8);

            break;
        }
        case NOP:
            // no-op
            break;
        case KIL:
            printf("Encountered KIL instruction @ $%x", g_cpu_regs.pc - 1);
            exit(-1);
        default:
            //TODO
            // no-op
            break;
        //default:
        //    throw new UnsupportedOperationException("Unsupported instruction " + instr.getOpcode().name());
    }

    if (g_cpu_regs.pc - 0x8000 >= g_prg_rom.size) {
        printf("Execution address exceeded PRG-ROM bounds (pc=$%x)\n", g_cpu_regs.pc);
        exit(-1);
    }
}

void _exec_next_instr(void) {
    const Instruction *instr = decode_instr(_next_prg_byte());

    InstructionParameter param = _get_next_m(instr->addr_mode);

    //printf("Decoded instruction %s:%s with computed param $%02x (src addr $%02x) @ $%04x\n",
    //        mnemonic_to_str(instr->mnemonic), addr_mode_to_str(instr->addr_mode), param.value, param.src_addr, g_cpu_regs.pc - get_instr_len(instr));

    g_burn_cycles = get_instr_cycles(instr, &param, &g_cpu_regs);

    _exec_instr(instr, param);
}

void cycle_cpu(void) {
    if (g_burn_cycles > 0) {
        g_burn_cycles--;
        return;
    }
    _exec_next_instr();
}
