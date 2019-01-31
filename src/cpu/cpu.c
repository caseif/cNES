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
#define BASE_SP 0xFF
#define DEFAULT_STATUS 0x24 // interrupt-disable and unused flag are set by default

#define PRINT_INSTRS 0

const InterruptType *INT_NMI   = &(InterruptType) {0xFFFA, false, true, false, false};
const InterruptType *INT_RESET = &(InterruptType) {0xFFFC, false, true,  false, false};
const InterruptType *INT_IRQ   = &(InterruptType) {0xFFFE, true,  true,  false, true};
const InterruptType *INT_BRK   = &(InterruptType) {0xFFFE, false, true,  true,  true};

static Cartridge *g_cartridge;

static DataBlob g_prg_rom;

unsigned char g_sys_memory[SYSTEM_MEMORY];
CpuRegisters g_cpu_regs;

uint16_t base_pc;

uint8_t g_burn_cycles = 0;

unsigned int g_total_cycles = 7;

void initialize_cpu(void) {
    g_cpu_regs.sp = BASE_SP;

    memset(&g_cpu_regs.status, DEFAULT_STATUS, 1);
    memset(g_sys_memory, 0xFF, SYSTEM_MEMORY);
}

void load_cartridge(Cartridge *cartridge) {
    g_cartridge = cartridge;
    load_program((DataBlob) {cartridge->prg_rom, cartridge->prg_size});

    base_pc = memory_read(0xFFFC) | (memory_read(0xFFFD) << 8);
    g_cpu_regs.pc = base_pc;
}

void load_program(DataBlob program_blob) {
    g_prg_rom = program_blob;
}

uint8_t memory_read(uint16_t addr) {
    switch (addr) {
        case 0 ... 0x1FFF: {
            return g_sys_memory[addr % 0x800];
        }
        case 0x2000 ... 0x3FFF: {
            return read_ppu_mmio((uint8_t) (addr % 8));
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
        case 0x2000 ... 0x3FFF: {
            write_ppu_mmio((uint8_t) (addr % 8), val);
            return;
        }
        case 0x4014: {
            initiate_oam_dma(val);
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
static InstructionParameter _get_next_m(uint8_t opcode, const Instruction *instr) {
    uint16_t raw_operand;
    uint16_t adj_operand;

    bool crosses_boundary = false;

    switch (instr->addr_mode) {
        case IMP: {
            return (InstructionParameter) {0, 0, 0};
        }
        case IMM: {
            uint8_t val = _next_prg_byte();
            return (InstructionParameter) {val, val, 0};
        }
        case REL: {
            uint8_t val = _next_prg_byte();
            return (InstructionParameter) {val, val, 0};
        }
        case ZRP: {
            raw_operand = _next_prg_byte();
            adj_operand = raw_operand;
            break;
        }
        case ZPX: {
            raw_operand = _next_prg_byte();
            // we have to reinterpret the result as a single byte since we're working with the zero-page
            adj_operand = (uint8_t) (raw_operand + g_cpu_regs.x);
            break;
        }
        case ZPY: {
            raw_operand = _next_prg_byte();
            // again, we have to reinterpret as a single byte
            adj_operand = (uint8_t) (raw_operand + g_cpu_regs.y);
            break;
        }
        case ABS: {
            uint16_t raw_operand = _next_prg_short();
            adj_operand = raw_operand;
            break;
        }
        case ABX: {
            raw_operand = _next_prg_short();
            adj_operand = g_cpu_regs.x + raw_operand;

            crosses_boundary = (adj_operand & 0xFF) < (raw_operand & 0xFF);

            break;
        }
        case ABY: {
            raw_operand = _next_prg_short();
            adj_operand = g_cpu_regs.y + raw_operand;

            crosses_boundary = (uint8_t) (adj_operand & 0xFF) < (uint8_t) (raw_operand & 0xFF);

            break;
        }
        case IND: {
            raw_operand = _next_prg_short();

            uint8_t addr_low = memory_read(raw_operand);
            // if the indirect target is the last byte of a page, the target
            // high byte will incorrectly be read from the first byte of the
            // same page
            uint16_t high_target = ((raw_operand & 0xFF) == 0xFF) ? (raw_operand - 0xFF) : (raw_operand + 1);
            uint8_t addr_high = memory_read(high_target);

            adj_operand = (addr_low | (addr_high << 8));

            break;
        }
        case IZX: {
            raw_operand = _next_prg_byte();

            uint16_t orig_addr = g_cpu_regs.x + raw_operand;
            // this mode wraps around to the same page
            uint8_t addr_low = memory_read(orig_addr % 0x100);
            // again, we have to handle wrapping for the second byte
            uint8_t addr_high = memory_read((orig_addr + 1) % 0x100);

            adj_operand = (addr_low | (addr_high << 8));

            break;
        }
        case IZY: {
            raw_operand = _next_prg_byte();

            uint8_t addr_low = memory_read(raw_operand);
            // this mode wraps around for the second address
            uint8_t addr_high = memory_read((raw_operand + 1) % 0x100);
            uint16_t base_addr = addr_low | (addr_high << 8);

            adj_operand = (g_cpu_regs.y + (addr_low | (addr_high << 8)));

            crosses_boundary = (adj_operand & 0xFF) < (base_addr & 0xFF);

            break;
        }
        default: {
            printf("Unhandled addressing mode %s", addr_mode_to_str(instr->addr_mode));
            exit(-1);
        }
    }

    uint8_t val = 0;
    // don't do a read for write-only instructions
    // this is especially important because erroneous reads can mess with MMIO
    if (get_instr_type(instr->mnemonic) != INS_W) {
        val = memory_read(adj_operand);
    }

    if (crosses_boundary && can_incur_page_boundary_penalty(opcode)) {
        g_burn_cycles++; // crossing a page boundary for certain instructions incurs a 1-cycle penalty
    }

    return (InstructionParameter) {val, raw_operand, adj_operand};
}

static void _do_branch(int8_t offset) {
    g_cpu_regs.pc += offset;
    g_burn_cycles++; // taking a branch incurs a 1-cycle penalty
}

static void _set_alu_flags(uint16_t val) {
    g_cpu_regs.status.zero = val ? 0 : 1;
    g_cpu_regs.status.negative = (val & 0x80) ? 1 : 0;
}

static void _do_shift(uint8_t m, uint16_t src_addr, bool implicit, bool right, bool rot) {
    if (implicit) {
        m = g_cpu_regs.acc;
    }

    uint8_t res = right ? m >> 1 : m << 1;

    if (rot) {
        if (right) {
            res |= g_cpu_regs.status.carry << 7;
        } else {
            res |= g_cpu_regs.status.carry;
        }
    }

    if (right) {
        g_cpu_regs.status.carry = m & 1;
    } else {
        g_cpu_regs.status.carry = (m & 0x80) >> 7;
    }

    if (implicit) {
        g_cpu_regs.acc = res;
    } else {
        memory_write(src_addr, res);
    }

    _set_alu_flags(res);
}

static void _do_cmp(uint8_t reg, uint16_t m) {
    g_cpu_regs.status.carry = reg >= m;
    g_cpu_regs.status.zero = reg == m;
    g_cpu_regs.status.negative = ((uint8_t) (reg - m)) >> 7;
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
    uint16_t addr = param.adj_operand;

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

            memory_write(addr, decRes);

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
            uint8_t incRes = m + 1;

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
            break;
        case ROR:
            _do_shift(m, addr, instr->addr_mode == IMP, true, true);
            break;
        case BIT:
            // set negative and overflow flags from memory
            g_cpu_regs.status.negative = m >> 7;
            g_cpu_regs.status.overflow = (m >> 6) & 1;

            // mask accumulator with value and set zero flag appropriately
            g_cpu_regs.status.zero = (g_cpu_regs.acc & m) == 0;
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
            serial &= ~(1 << 4); // unset bit 4
            serial |= (1 << 5); // set bit 5
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
        default:
        case KIL:
            printf("Encountered %s instruction @ $%x\n", mnemonic_to_str(instr->mnemonic), g_cpu_regs.pc - 1);
            printf("Error codes: %02x %02x\n", memory_read(0x02), memory_read(0x03));
            for (unsigned int i = 0x200; i < 0x400; i++) {

            }
            exit(-1);
        //default:
            //TODO
            // no-op
            //break;
    }

    if (g_cpu_regs.pc >= g_prg_rom.size + base_pc) {
        printf("Execution address exceeded PRG-ROM bounds (pc=$%x)\n", g_cpu_regs.pc);
        exit(-1);
    }
}

void _exec_next_instr(void) {
    // reset burn cycles
    g_burn_cycles = 0;

    uint8_t opcode = _next_prg_byte();
    const Instruction *instr = decode_instr(opcode);

    InstructionParameter param = _get_next_m(opcode, instr);

    #if PRINT_INSTRS
    extern unsigned int g_scanline;
    extern unsigned int g_scanline_tick;
    uint8_t status;
    memcpy(&status, &g_cpu_regs.status, 1);
    printf("%04x  %s:%s $%04x (a=%02x,x=%02x,y=%02x,sp=%02x,p=%02x,ppu=%d,cyc=%d)\n",
            g_cpu_regs.pc - get_instr_len(instr), mnemonic_to_str(instr->mnemonic), addr_mode_to_str(instr->addr_mode),
            param.raw_operand, g_cpu_regs.acc, g_cpu_regs.x, g_cpu_regs.y, g_cpu_regs.sp, status,
            g_scanline_tick, g_total_cycles);
    #endif

    // we increment, since decoding the instruction can modify the value
    g_burn_cycles += get_instr_cycles(opcode, &param, &g_cpu_regs) - 1;

    _exec_instr(instr, param);
}

void cycle_cpu(void) {
    if (g_burn_cycles > 0) {
        g_burn_cycles--;
    } else {
        _exec_next_instr();
    }
    g_total_cycles++;
}
