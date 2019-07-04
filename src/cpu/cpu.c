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
#include "system.h"
#include "cpu/cpu.h"
#include "cpu/instrs.h"
#include "input/input_device.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSTEM_MEMORY 0x800
#define STACK_BOTTOM_ADDR 0x100
#define BASE_SP 0xFF
#define DEFAULT_STATUS 0x24 // interrupt-disable and unused flag are set by default

#define PRINT_INSTRS 0

#define ASSERT_CYCLE(l, h)  assert(g_instr_cycle >= l); \
                            assert(g_instr_cycle <= h)

const InterruptType INT_NMI   = (InterruptType) {0xFFFA, false, true, false, false};
const InterruptType INT_RESET = (InterruptType) {0xFFFC, false, true,  false, false};
const InterruptType INT_IRQ   = (InterruptType) {0xFFFE, true,  true,  false, true};
const InterruptType INT_BRK   = (InterruptType) {0xFFFE, false, true,  true,  true};

unsigned char g_sys_memory[SYSTEM_MEMORY];

CpuRegisters g_cpu_regs;

unsigned char g_debug_buffer[0x1000];

uint16_t base_pc;

uint16_t g_burn_cycles = 0;

unsigned int g_total_cycles = 0;

static bool g_nmi_line = false;
static bool g_irq_line = false;

// state for implementing cycle-accuracy
uint8_t g_instr_cycle = 1; // this is 1-indexed to match blargg's doc

Instruction *g_cur_instr; // the instruction currently being executed
static uint8_t g_last_opcode; // the last opcode decoded

#if PRINT_INSTRS
// snapshots for logging
static CpuRegisters g_last_reg_snapshot; // the state of the registers when the last instruction started execution
static unsigned int g_total_cycles_snapshot;
static uint16_t g_ppu_scanline_snapshot;
static uint16_t g_ppu_scanline_tick_snapshot;
#endif

static uint16_t g_cur_operand; // the operand directly read from PRG
static uint16_t g_eff_operand; // the effective operand (after being offset)

static uint8_t g_latched_val; // typically the value to be read from or written to memory

static const InterruptType *g_cur_interrupt; // the interrupt type currently being executed
static const InterruptType *g_queued_interrupt; // the interrupt type currently queued
static bool g_nmi_hijack; // set when an NMI "hijacks" a software interrupt

void initialize_cpu(void) {
    g_cpu_regs.sp = BASE_SP;

    memset(&g_cpu_regs.status, DEFAULT_STATUS, 1);
    memset(g_sys_memory, 0x00, SYSTEM_MEMORY);

    g_queued_interrupt = &INT_RESET;
}

void cpu_init_pc(uint16_t addr) {
    g_cpu_regs.pc = addr;

    printf("Initialized PC to $%04x\n", g_cpu_regs.pc);
}

uint8_t cpu_ram_read(uint16_t addr) {
    assert(addr < 0x800);
    return g_sys_memory[addr];
}

void cpu_ram_write(uint16_t addr, uint8_t val) {
    assert(addr < 0x800);
    g_sys_memory[addr] = val;
}

void cpu_start_oam_dma(uint8_t page) {
    ppu_start_oam_dma(page);
    g_burn_cycles += 514;
}

void stack_push(char val) {
    g_sys_memory[STACK_BOTTOM_ADDR + g_cpu_regs.sp--] = val;
}

unsigned char stack_pop(void) {
    return g_sys_memory[STACK_BOTTOM_ADDR + ++g_cpu_regs.sp];
}

static unsigned char _next_prg_byte(void) {
    return system_ram_read(g_cpu_regs.pc);
}

static void _set_alu_flags(uint8_t val) {
    g_cpu_regs.status.zero = val ? 0 : 1;
    g_cpu_regs.status.negative = (val & 0x80) ? 1 : 0;
}

static void _do_shift(bool right, bool rot) {
    uint8_t res = right ? g_latched_val >> 1 : g_latched_val << 1;

    if (rot) {
        if (right) {
            res |= g_cpu_regs.status.carry << 7;
        } else {
            res |= g_cpu_regs.status.carry;
        }
    }

    if (right) {
        g_cpu_regs.status.carry = g_latched_val & 1;
    } else {
        g_cpu_regs.status.carry = (g_latched_val & 0x80) >> 7;
    }

    _set_alu_flags(res);

    g_latched_val = res;
}

static void _do_cmp(uint8_t reg, uint8_t m) {
    g_cpu_regs.status.carry = reg >= m;
    g_cpu_regs.status.zero = reg == m;
    g_cpu_regs.status.negative = ((uint8_t) (reg - m)) >> 7;
}

static void _do_adc(uint8_t m) {
    uint8_t acc0 = g_cpu_regs.acc;

    g_cpu_regs.acc = (acc0 + m + g_cpu_regs.status.carry);

    _set_alu_flags(g_cpu_regs.acc);

    // unsigned overflow will occur if at least two among the most significant operand bits and the carry bit are set
    g_cpu_regs.status.carry = ((acc0 + m + g_cpu_regs.status.carry) & 0x100) ? 1 : 0;

    // signed overflow will occur if the sign of both inputs if different from the sign of the result
    g_cpu_regs.status.overflow = ((acc0 ^ g_cpu_regs.acc) & (m ^ g_cpu_regs.acc) & 0x80) ? 1 : 0;
}

static void _do_sbc(uint8_t m) {
    return _do_adc(~m);
}

void cpu_raise_nmi_line(void) {
    g_nmi_line = true;
}

void cpu_clear_nmi_line(void) {
    g_nmi_line = false;
}

void cpu_raise_irq_line(void) {
    g_irq_line = true;
}

void cpu_clear_irq_line(void) {
    g_irq_line = false;
}

void _do_instr_operation() {
    switch (g_cur_instr->mnemonic) {
        // storage
        case LDA:
            g_cpu_regs.acc = g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case LDX:
            g_cpu_regs.x = g_latched_val;

            _set_alu_flags(g_cpu_regs.x);

            break;
        case LDY:
            g_cpu_regs.y = g_latched_val;

            _set_alu_flags(g_cpu_regs.y);

            break;
        case LAX: // unofficial
            g_cpu_regs.acc = g_latched_val;
            g_cpu_regs.x = g_latched_val;

            _set_alu_flags(g_latched_val);

            break;
        case STA:
            g_latched_val = g_cpu_regs.acc;
            break;
        case STX:
            g_latched_val = g_cpu_regs.x;
            break;
        case STY:
            g_latched_val = g_cpu_regs.y;
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
            _do_adc(g_latched_val);

            break;
        }
        case SBC: {
            _do_sbc(g_latched_val);

            break;
        }
        case DEC: {
            g_latched_val--;

            _set_alu_flags(g_latched_val);

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
            g_latched_val++;

            _set_alu_flags(g_latched_val);

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
        case ISC: // unofficial
            g_latched_val++;
            _do_sbc(g_latched_val);

            break;
        case DCP: // unofficial
            g_latched_val--;
            _do_cmp(g_cpu_regs.acc, g_latched_val);

            break;
        // logic
        case AND:
            g_cpu_regs.acc &= g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case SAX: { // unofficial
            uint8_t res = g_cpu_regs.acc & g_cpu_regs.x;
            g_latched_val = res;

            break;
        }
        case ANC: { // unofficial
            g_cpu_regs.acc &= g_latched_val;
            g_cpu_regs.status.carry = g_cpu_regs.acc >> 7;
            break;
        }
        case ASL:
            _do_shift(false, false);
            break;
        case LSR:
            _do_shift(true, false);
            break;
        case ROL:
            _do_shift(false, true);
            break;
        case ROR:
            _do_shift(true, true);
            break;
        case ALR: // unofficial
            _do_shift(true, false);
            break;
        case SLO: { // unofficial
            _do_shift(false, false);
            g_cpu_regs.acc |= g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        }
        case RLA: { // unofficial
            // I think this performs two r/w cycles too
            _do_shift(false, true);

            g_cpu_regs.acc &= g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        }
        case ARR: // unofficial
            g_cpu_regs.acc &= g_latched_val;

            _do_shift(true, true);

            _set_alu_flags(g_cpu_regs.acc);

            g_cpu_regs.status.overflow = (g_cpu_regs.acc >> 5) & 1;
            g_cpu_regs.status.carry = !((g_cpu_regs.acc >> 6) & 1);
            
            break;
        case SRE: { // unofficial
            _do_shift(true, false);

            g_cpu_regs.acc ^= g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        }
        case RRA: { // unofficial
            _do_shift(true, true);

            _do_adc(g_latched_val);

            break;
        }
        case AXS: { // unofficial
            g_cpu_regs.x &= g_cpu_regs.acc;
            uint8_t res = g_cpu_regs.x - g_latched_val;

            g_cpu_regs.status.carry = res > g_latched_val;

            g_cpu_regs.x = res;

            _set_alu_flags(g_cpu_regs.x);

            break;
        }
        case EOR:
            g_cpu_regs.acc = g_cpu_regs.acc ^ g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case ORA:
            g_cpu_regs.acc = g_cpu_regs.acc | g_latched_val;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        case BIT:
            // set negative and overflow flags from memory
            g_cpu_regs.status.negative = g_latched_val >> 7;
            g_cpu_regs.status.overflow = (g_latched_val >> 6) & 1;

            // mask accumulator with value and set zero flag appropriately
            g_cpu_regs.status.zero = (g_cpu_regs.acc & g_latched_val) == 0;
            break;
        case TAS: { // unofficial
            // this some fkn voodo right here
            g_cpu_regs.sp = g_cpu_regs.acc & g_cpu_regs.x;
            g_latched_val = g_cpu_regs.sp & ((g_cur_operand >> 8) + 1);

            break;
        }
        case LAS: { // unofficial
            g_cpu_regs.acc = g_latched_val & g_cpu_regs.sp;
            g_cpu_regs.x = g_cpu_regs.acc;
            g_cpu_regs.sp = g_cpu_regs.acc;

            _set_alu_flags(g_cpu_regs.acc);

            break;
        }
        case SHX: { // unofficial
            g_latched_val = g_cpu_regs.x & ((g_cur_operand >> 8) + 1);
            break;
        }
        case SHY: { // unofficial
            g_latched_val = g_cpu_regs.y & ((g_cur_operand >> 8) + 1);
            break;
        }
        case AHX: { // unofficial
            g_latched_val = (g_cpu_regs.acc & g_cpu_regs.x) & 7;
            break;
        }
        case ATX: { // unofficial
            g_cpu_regs.x = g_cpu_regs.acc & g_latched_val;
            _set_alu_flags(g_cpu_regs.x);

            break;
        }
        case XAA: { // unofficial
            // even more voodoo
            g_cpu_regs.acc = (g_cpu_regs.x & 0xEE) | ((g_cpu_regs.x & g_cpu_regs.acc) & 0x11);
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
            _do_cmp(g_cpu_regs.acc, g_latched_val);
            break;
        case CPX:
            _do_cmp(g_cpu_regs.x, g_latched_val);
            break;
        case CPY:
            _do_cmp(g_cpu_regs.y, g_latched_val);
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
        // misc
        case NOP:
            // no-op
            break;
        default:
        case KIL:
            printf("Encountered %s instruction @ $%x\n", mnemonic_to_str(g_cur_instr->mnemonic), g_cpu_regs.pc - 1);
            exit(-1);
    }
}

static void _reset_instr_state(void) {
    g_cur_operand = 0; // reset current operand
    g_eff_operand = 0; // reset effective operand
    g_latched_val = 0; // reset data value

    g_instr_cycle = 1; // skip opcode fetching
}

static void _poll_interrupts() {
    if (g_nmi_line) {
        g_queued_interrupt = &INT_NMI;
    } else if (g_irq_line && !g_cpu_regs.status.interrupt_disable) {
        g_queued_interrupt = &INT_IRQ;
    }
}

static void _execute_interrupt() {
    ASSERT_CYCLE(1, 7);

    switch (g_instr_cycle) {
        case 1:
            _next_prg_byte(); // garbage read
            g_last_opcode = 0; // BRK

            if (g_cur_interrupt == &INT_NMI) {
                cpu_clear_nmi_line();
            } else if (g_cur_interrupt == &INT_IRQ) {
                cpu_clear_irq_line();
            } else if (g_cur_interrupt == &INT_BRK && g_nmi_line) {
                g_nmi_hijack = true;
            }

            break;
        case 2:
            _next_prg_byte(); // garbage read
            if (g_cur_interrupt == &INT_BRK) {
                g_cpu_regs.pc++; // increment PC anyway for software interrupts
            }

            if (g_cur_interrupt == &INT_BRK && g_nmi_line) {
                g_nmi_hijack = true;
            }

            break;
        case 3:
            if (g_cur_interrupt->push_pc) {
                // push PC high, decrement S
                system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, g_cpu_regs.pc >> 8);
            }
            g_cpu_regs.sp--;

            if (g_cur_interrupt == &INT_BRK && g_nmi_line) {
                g_nmi_hijack = true;
            }

            break;
        case 4:
            if (g_cur_interrupt->push_pc) {
                // push PC low, decrement S
                system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, g_cpu_regs.pc & 0xFF);
            }
            g_cpu_regs.sp--;

            if (g_cur_interrupt == &INT_BRK && g_nmi_line) {
                g_nmi_hijack = true;
            }

            break;
        case 5:
            if (g_nmi_hijack) {
                g_cur_interrupt = &INT_NMI;
                g_nmi_hijack = false;
            }

            if (g_cur_interrupt->push_pc) {
                // push P, decrement S, set/clear B
                g_cpu_regs.status.break_command = g_cur_interrupt->set_b;

                uint8_t val = g_cpu_regs.status.serial;
                if (g_cur_interrupt == &INT_BRK) {
                    val |= 0x30;
                }
                
                system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, val);
            }
            g_cpu_regs.sp--;
            break;
        case 6:
            // clear PC high and set to vector value
            g_latched_val = system_ram_read(g_cur_interrupt->vector_loc);

            if (g_cur_interrupt->set_i) {
                g_cpu_regs.status.interrupt_disable = 1;
            }
            break;
        case 7: {
            // clear PC high and set to vector value
            uint8_t pch = system_ram_read(g_cur_interrupt->vector_loc + 1);
            g_cpu_regs.pc = (pch << 8) | g_latched_val;
            g_instr_cycle = 0; // reset for next instruction
            g_cur_interrupt = NULL;
            break;
        }
    }
}

static void _handle_rti(void) {
    ASSERT_CYCLE(2, 6);
    
    switch (g_instr_cycle) {
        case 2:
            _next_prg_byte(); // garbage read
            break;
        case 3:
            // increment S
            g_cpu_regs.sp++;
            break;
        case 4:
            // pull P, increment S
            g_cpu_regs.status.serial = system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp);
            g_cpu_regs.sp++;
            break;
        case 5:
            // clear PC low and set to stack value, increment S
            g_cpu_regs.pc &= ~0xFF;
            g_cpu_regs.pc |= system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp);
            g_cpu_regs.sp++;

            _poll_interrupts();

            break;
        case 6:
            // clear PC high and set to stack value
            g_cpu_regs.pc &= ~0xFF00;
            g_cpu_regs.pc |= system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp) << 8;

            g_instr_cycle = 0; // reset for next instruction
            break;
    }
}

static void _handle_rts(void) {
    ASSERT_CYCLE(2, 6);
    
    switch (g_instr_cycle) {
        case 2:
            system_ram_read(g_cpu_regs.pc); // garbage read
            break;
        case 3:
            // increment S
            g_cpu_regs.sp++;
            break;
        case 4:
            // clear PC low and set to stack value, increment S
            g_cpu_regs.pc &= ~0xFF;
            g_cpu_regs.pc |= system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp);
            g_cpu_regs.sp++;
            break;
        case 5:
            // clear PC high and set to stack value
            g_cpu_regs.pc &= ~0xFF00;
            g_cpu_regs.pc |= system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp) << 8;

            _poll_interrupts();

            break;
        case 6:
            // increment PC
            g_cpu_regs.pc++;

            g_instr_cycle = 0; // reset for next instruction
    }
}

static void _handle_stack_push(void) {
    ASSERT_CYCLE(2, 3);

    switch (g_instr_cycle) {
        case 2:
            _next_prg_byte(); // garbage read

            _poll_interrupts();

            break;
        case 3: {
            // push register, decrement S
            uint8_t val;
            if (g_cur_instr->mnemonic == PHA) {
                val = g_cpu_regs.acc;
            } else {
                val = g_cpu_regs.status.serial;
                val |= 0x30;
            }
            system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, val);
            g_cpu_regs.sp--;

            g_instr_cycle = 0; // reset for next instruction
            break;
        }
    }
}

static void _handle_stack_pull(void) {
    ASSERT_CYCLE(2, 4);

    switch (g_instr_cycle) {
        case 2:
            _next_prg_byte(); // garbage read
            break;
        case 3:
            // increment S
            g_cpu_regs.sp++;

            _poll_interrupts();

            break;
        case 4: {
            // pull register
            uint8_t val = system_ram_read(STACK_BOTTOM_ADDR + g_cpu_regs.sp);
            if (g_cur_instr->mnemonic == PLA) {
                g_cpu_regs.acc = val;
            } else {
                g_cpu_regs.status.serial = val;
            }

            if (g_cur_instr->mnemonic == PLA) {
                _set_alu_flags(val);
            }

            g_instr_cycle = 0; // reset for next instruction

            break;
        }
    }
}

static void _handle_jsr(void) {
    ASSERT_CYCLE(3, 6);

    switch (g_instr_cycle) {
        case 3:
            // unsure of what happens here
            break;
        case 4:
            // push PC high, decrement S
            system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, g_cpu_regs.pc >> 8);
            g_cpu_regs.sp--;
            break;
        case 5:
            // push PC low, decrement S
            system_ram_write(STACK_BOTTOM_ADDR + g_cpu_regs.sp, g_cpu_regs.pc & 0xFF);
            g_cpu_regs.sp--;

            _poll_interrupts();

            break;
        case 6: {
            // copy low byte to PC, fetch high byte to PC (but don't increment PC)
            uint8_t pch = system_ram_read(g_cpu_regs.pc);
            g_cur_operand |= pch << 8;
            g_eff_operand = g_cur_operand;

            g_cpu_regs.pc = g_cur_operand;

            g_instr_cycle = 0; // reset for next instruction
            break;
        }
    }
}

static bool _handle_stack_instr(void) {
    switch (g_cur_instr->mnemonic) {
        case RTI:
            _handle_rti();
            return true;
        case RTS:
            _handle_rts();
            return true;
        case PHA:
        case PHP:
            _handle_stack_push();
            return true;
        case PLA:
        case PLP:
            _handle_stack_pull();
            return true;
        case JSR:
            _handle_jsr();
            return true;
        default:
            return false;
    }
}

static void _handle_instr_rw(uint8_t offset) {
    switch (get_instr_type(g_cur_instr->mnemonic)) {
        case INS_R:
            ASSERT_CYCLE(offset, offset);

            g_latched_val = system_ram_read(g_eff_operand);
            _do_instr_operation();

            g_instr_cycle = 0;

            break;
        case INS_W:
            ASSERT_CYCLE(offset, offset);

            _do_instr_operation();
            system_ram_write(g_eff_operand, g_latched_val);

            g_instr_cycle = 0;

            break;
        case INS_RW:
            ASSERT_CYCLE(offset, offset + 2);

            switch (g_instr_cycle - offset) {
                case 0:
                    g_latched_val = system_ram_read(g_eff_operand);
                    break;
                case 1:
                    system_ram_write(g_eff_operand, g_latched_val);
                    _do_instr_operation();

                    _poll_interrupts();

                    break;
                case 2:
                    system_ram_write(g_eff_operand, g_latched_val);
                    g_instr_cycle = 0;
                    break;
            }

            break;
        default:
            printf("Unhandled instr %s with type %d\n", mnemonic_to_str(g_cur_instr->mnemonic),
                    get_instr_type(g_cur_instr->mnemonic));
            fflush(stdout);
            assert(false);
    }
}

static void _handle_instr_zrp(void) {
    g_eff_operand = g_cur_operand;
    _handle_instr_rw(3);
}

static void _handle_instr_zpi(void) {
    ASSERT_CYCLE(3, 6);

    if (g_instr_cycle == 3) {
        g_latched_val = system_ram_read(g_cur_operand);
        g_eff_operand = (g_cur_operand + (g_cur_instr->addr_mode == ZPX ? g_cpu_regs.x : g_cpu_regs.y)) & 0xFF;

        if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
            _poll_interrupts();
        }
    } else {
        _handle_instr_rw(4);
    }
}

static void _handle_instr_abs(void) {
    ASSERT_CYCLE(3, 6);

    if (g_instr_cycle == 3) {
        g_cur_operand |= (_next_prg_byte() << 8); // fetch high byte of operand
        g_cpu_regs.pc++; // increment PC

        if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
            _poll_interrupts();
        }
    } else {
        g_eff_operand = g_cur_operand;
        _handle_instr_rw(4);
    }
}

static void _handle_instr_abi(void) {
    ASSERT_CYCLE(3, 8);

    switch (g_instr_cycle) {
        case 3:
            g_cur_operand |= (_next_prg_byte() << 8); // fetch high byte of operand
            g_eff_operand = (g_cur_operand & 0xFF00)
                    | ((g_cur_operand + (g_cur_instr->addr_mode == ABX ? g_cpu_regs.x : g_cpu_regs.y)) & 0xFF);
            g_cpu_regs.pc++; // increment PC

            // if this will be the next-last cycle, poll interrupts
            if (get_instr_type(g_cur_instr->mnemonic) != INS_RW
                    && !((g_cur_operand & 0xFF) + (g_cur_instr->addr_mode == ABX ? g_cpu_regs.x : g_cpu_regs.y) >= 0x100)) {
                _poll_interrupts();
            }

            break;
        case 4:
            g_latched_val = system_ram_read(g_eff_operand);
            // fix effective address
            if ((g_cur_operand & 0xFF) + (g_cur_instr->addr_mode == ABX ? g_cpu_regs.x : g_cpu_regs.y) >= 0x100) {
                g_eff_operand += 0x100;
                if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
                    _poll_interrupts();
                }
            } else if (get_instr_type(g_cur_instr->mnemonic) == INS_R) {
                // we're finished if the high byte was correct
                _do_instr_operation();

                g_instr_cycle = 0;
            }
            break;
        default:
            _handle_instr_rw(5);
            break;
    }
}

static void _handle_instr_izx(void) {
    ASSERT_CYCLE(3, 8);

    switch (g_instr_cycle) {
        case 3:
            system_ram_read(g_cur_operand);
            g_cur_operand = (g_cur_operand & 0xFF00) | ((g_cur_operand + g_cpu_regs.x) & 0xFF);
            break;
        case 4:
            g_eff_operand = 0;
            g_eff_operand |= system_ram_read(g_cur_operand);
            break;
        case 5:
            g_eff_operand |= system_ram_read((g_cur_operand & 0xFF00) | ((g_cur_operand + 1) & 0xFF)) << 8;

            if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
                _poll_interrupts();
            }

            break;
        default:
            _handle_instr_rw(6);
            break;
    }
}

static void _handle_instr_izy(void) {
    ASSERT_CYCLE(3, 8);

    switch (g_instr_cycle) {
        case 3:
            g_eff_operand = 0;
            g_eff_operand |= system_ram_read(g_cur_operand);
            g_latched_val = g_eff_operand & 0xFF;
            break;
        case 4:
            g_eff_operand |= system_ram_read((g_cur_operand & 0xFF00) | ((g_cur_operand + 1) & 0xFF)) << 8;
            g_eff_operand = (g_eff_operand & 0xFF00) | ((g_eff_operand + g_cpu_regs.y) & 0xFF);
            break;
        case 5: {
            uint8_t tmp = system_ram_read(g_eff_operand);
            if (g_latched_val + g_cpu_regs.y >= 0x100) {
                g_eff_operand += 0x100;
            } else if (get_instr_type(g_cur_instr->mnemonic) == INS_R) {
                // we're finished if the high byte was correct
                g_latched_val = tmp;

                _do_instr_operation();

                g_instr_cycle = 0;
            } else {
                g_latched_val = tmp;
            }

            if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
                _poll_interrupts();
            }

            break;
        }
        default:
            _handle_instr_rw(6);
            break;
    }
}

static void _handle_jmp(void) {
    switch (g_cur_instr->addr_mode) {
        case ABS:
            ASSERT_CYCLE(3, 3);
            
            uint8_t pch = system_ram_read(g_cpu_regs.pc);
            g_cpu_regs.pc++;

            g_cpu_regs.pc = (pch << 8) | (g_cur_operand & 0xFF);

            g_instr_cycle = 0;
            
            break;
        case IND:
            ASSERT_CYCLE(3, 5);
            switch (g_instr_cycle) {
                case 3:
                    g_cur_operand |= _next_prg_byte() << 8; // fetch high byte of operand
                    g_cpu_regs.pc++; // increment PC
                    break;
                case 4:
                    g_latched_val = system_ram_read(g_cur_operand); // fetch target low

                    if (get_instr_type(g_cur_instr->mnemonic) != INS_RW) {
                        _poll_interrupts();
                    }

                    break;
                case 5:
                    g_cpu_regs.pc = 0; // clear PC (technically not accurate, but it has no practical consequence)
                    // fetch target high to PC
                    // we technically don't do this properly, but sub-cycle accuracy is not necessarily a goal
                    // page boundary crossing is not handled correctly - we emulate this bug here
                    g_eff_operand = (system_ram_read((g_cur_operand & 0xFF00) | ((g_cur_operand + 1) & 0xFF)) << 8) | g_latched_val;
                    // copy target to PC
                    g_cpu_regs.pc = g_eff_operand;

                    g_instr_cycle = 0;

                    break;
            }
            break;
        default:
            assert(false);
    }
}

// forward declaration for branch handling
static void _do_instr_cycle(void);

static void _handle_branch(void) {
    ASSERT_CYCLE(3, 4);

    switch (g_instr_cycle) {
        case 3:
            g_latched_val = system_ram_read(g_cpu_regs.pc);

            g_eff_operand = g_cpu_regs.pc + (int8_t) g_cur_operand;

            bool should_take;
            switch (g_cur_instr->mnemonic) {
                case BCC:
                    should_take = !g_cpu_regs.status.carry;
                    break;
                case BCS:
                    should_take = g_cpu_regs.status.carry;
                    break;
                case BNE:
                    should_take = !g_cpu_regs.status.zero;
                    break;
                case BEQ:
                    should_take = g_cpu_regs.status.zero;
                    break;
                case BPL:
                    should_take = !g_cpu_regs.status.negative;
                    break;
                case BMI:
                    should_take = g_cpu_regs.status.negative;
                    break;
                case BVC:
                    should_take = !g_cpu_regs.status.overflow;
                    break;
                case BVS:
                    should_take = g_cpu_regs.status.overflow;
                    break;
                default:
                    assert(false);
            }

            if (should_take) {
                g_latched_val = g_cpu_regs.pc & 0xFF;
                g_cpu_regs.pc = (g_cpu_regs.pc & 0xFF00) | ((g_cpu_regs.pc + (int8_t) g_cur_operand) & 0xFF);
                return;
            } else {
                // recursive call to fetch the next opcode
                g_instr_cycle = 1;
                _do_instr_cycle();
                return;
            }
        case 4: {
            uint8_t old_pcl = g_latched_val;

            g_latched_val = system_ram_read(g_cpu_regs.pc);

            if ((int8_t) g_cur_operand < 0 && -(int8_t) g_cur_operand > old_pcl) {
                g_cpu_regs.pc -= 0x100;
            } else if ((int8_t) g_cur_operand > 0 && g_cur_operand + old_pcl >= 0x100) {
                g_cpu_regs.pc += 0x100;
            } else {
                // recursive call to fetch the next opcode
                g_instr_cycle = 1;
                _do_instr_cycle();
                return;
            }

            g_instr_cycle = 0;

            break;
        }
        default:
            assert(false);
    }
}

#if PRINT_INSTRS
// forward declaration
static void _print_last_instr(void);
#endif

static void _do_instr_cycle(void) {
    if (g_cur_interrupt) {
        _execute_interrupt();
    } else if (g_instr_cycle == 1) {
        #if PRINT_INSTRS
        if (g_cur_instr != NULL) {
            _print_last_instr();
        }
        #endif

        if (g_queued_interrupt) {
            g_cur_interrupt = g_queued_interrupt;
            g_queued_interrupt = NULL;
            _execute_interrupt();
        } else {
            g_last_opcode = _next_prg_byte(); // store last opcode
            g_cur_instr = decode_instr(g_last_opcode); // fetch and decode opcode

            _reset_instr_state();

            #if PRINT_INSTRS
            // store snapshots for logging
            g_last_reg_snapshot = g_cpu_regs;
            g_total_cycles_snapshot = g_total_cycles;
            g_ppu_scanline_snapshot = ppu_get_scanline();
            g_ppu_scanline_tick_snapshot = ppu_get_scanline_tick();
            #endif

            g_cpu_regs.pc++; // increment PC

            if (g_cur_instr->addr_mode == IMP || g_cur_instr->addr_mode == IMM) {
                _poll_interrupts();
            }
        }

        return;
    } else if (g_cur_instr->mnemonic == BRK) {
        g_cur_interrupt = &INT_BRK;
        _execute_interrupt();
    } else if (g_instr_cycle == 2 && g_cur_instr->addr_mode != IMP && g_cur_instr->addr_mode != IMM) {
        if ((g_cur_instr->mnemonic == JMP && g_cur_instr->addr_mode == ABS)
                || get_instr_type(g_cur_instr->mnemonic) == INS_BRANCH) {
            _poll_interrupts();
        }

        // this doesn't execute for implicit/immediate instructions because they have additional steps beyond fetching
        // on this cycle
        g_cur_operand |= _next_prg_byte(); // fetch low byte of operand
        g_cpu_regs.pc++; // increment PC
        return;
    } else if (_handle_stack_instr()) {
        return;
    } else {
        InstructionType type = get_instr_type(g_cur_instr->mnemonic);
        if (type == INS_JUMP) {
            _handle_jmp();
            return;
        } else if (type == INS_BRANCH) {
            _handle_branch();
            return;
        }

        switch (g_cur_instr->addr_mode) {
            case IMP:
                ASSERT_CYCLE(2, 2);

                switch (get_instr_type(g_cur_instr->mnemonic)) {
                    case INS_R:
                        g_latched_val = g_cpu_regs.acc;
                        _do_instr_operation();
                        break;
                    case INS_W:
                        _do_instr_operation();
                        g_cpu_regs.acc = g_latched_val;
                        break;
                    case INS_RW:
                        g_latched_val = g_cpu_regs.acc;
                        _do_instr_operation();
                        g_cpu_regs.acc = g_latched_val;
                        break;
                    case INS_NONE:
                        _do_instr_operation();
                        break;
                    default:
                        assert(false);
                }
                g_instr_cycle = 0; // reset for next instruction
                break;
            case IMM:
                ASSERT_CYCLE(2, 2);
        
                g_cur_operand |= _next_prg_byte(); // fetch immediate byte
                g_cpu_regs.pc++; // increment PC

                g_latched_val = g_cur_operand & 0xFF;
                _do_instr_operation();

                g_instr_cycle = 0; // reset for next instruction
                break;
            case ZRP:
                _handle_instr_zrp();
                break;
            case ZPX:
            case ZPY:
                _handle_instr_zpi();
                break;
            case ABS:
                _handle_instr_abs();
                break;
            case ABX:
            case ABY:
                _handle_instr_abi();
                break;
            case IZX:
                _handle_instr_izx();
                break;
            case IZY:
                _handle_instr_izy();
                break;
            default:
                assert(false);
        }
    }
}

void cycle_cpu(void) {
    if (g_burn_cycles > 0) {
        g_burn_cycles--;
    } else {
        _do_instr_cycle();
        g_instr_cycle++;
    }

    g_total_cycles++;
}

void dump_ram(void) {
    FILE *out_file = fopen("ram.bin", "w+");

    if (!out_file) {
        printf("Failed to dump RAM (couldn't open file: %s)\n", strerror(errno));
        return;
    }

    fwrite(g_sys_memory, SYSTEM_MEMORY, 1, out_file);

    fclose(out_file);
}

#if PRINT_INSTRS
static void _print_last_instr(void) {
    char str_machine_code[9];
    switch (get_instr_len(g_cur_instr)) {
        case 1:
            sprintf(str_machine_code, "%02X      ", g_last_opcode);
            break;
        case 2:
            sprintf(str_machine_code, "%02X %02X   ", g_last_opcode, g_cur_operand);
            break;
        case 3:
            sprintf(str_machine_code, "%02X %02X %02X", g_last_opcode, g_cur_operand & 0xFF,g_cur_operand >> 8);
            break;
    }

    char str_param[32];
    InstructionType instr_type = get_instr_type(g_cur_instr->mnemonic);
    switch (g_cur_instr->addr_mode) {
        case IMM:
            sprintf(str_param, "#$%02X                   ", g_cur_operand);
            break;
        case ZRP:
            switch (instr_type) {
                case INS_R:
                case INS_RW:
                    sprintf(str_param, "$%02X     -> $%02X         ", g_cur_operand, g_latched_val);
                    break;
                default:
                    sprintf(str_param, "$%02X                    ", g_cur_operand);
                    break;
            }
            break;
        case ZPX:
        case ZPY:
            switch (instr_type) {
                case INS_R:
                case INS_RW:
                    sprintf(str_param, "$%02X,%c   -> $%02X   -> $%02X",
                            g_cur_operand, g_cur_instr->addr_mode == ZPX ? 'X' : 'Y', g_eff_operand, g_latched_val);
                    break;
                default:
                    sprintf(str_param, "$%02X,%c   -> $%02X         ",
                            g_cur_operand, g_cur_instr->addr_mode == ZPX ? 'X' : 'Y', g_latched_val);
                    break;
            }
            break;
        case ABS:
            switch (instr_type) {
                case INS_R:
                case INS_RW:
                    sprintf(str_param, "$%04X   -> $%02X         ", g_cur_operand, g_latched_val);
                    break;
                default:
                    sprintf(str_param, "$%04X                  ", g_cur_operand);
                    break;
            }
            break;
        case ABX:
        case ABY:
            switch (instr_type) {
                case INS_R:
                case INS_RW:
                    sprintf(str_param, "$%04X,%c -> $%04X -> $%02X",
                            g_cur_operand, g_cur_instr->addr_mode == ABX ? 'X' : 'Y', g_eff_operand, g_latched_val);
                    break;
                default:
                    sprintf(str_param, "$%04X,%c -> $%02X         ",
                            g_cur_operand, g_cur_instr->addr_mode == ABX ? 'X' : 'Y', g_latched_val);
                    break;
            }
            break;
        case REL:
            sprintf(str_param, "#$%02X    -> $%04X       ",
                    g_cur_operand, g_eff_operand);
            break;
        case IND:
            sprintf(str_param, "($%04X) -> $%04X       ", g_cur_operand, g_eff_operand);
            break;
        case IZX:
            sprintf(str_param, "($%02X,X) -> $%04X -> $%02X", g_cur_operand, g_eff_operand, g_latched_val);
            break;
        case IZY:
            sprintf(str_param, "($%02X),Y -> $%04X -> $%02X", g_cur_operand, g_eff_operand, g_latched_val);
            break;
        case IMP:
            sprintf(str_param, "                       ");
            break;
    }

    printf("%04X  %s  %s %s  (a=%02X,x=%02X,y=%02X,sp=%02X,p=%02X,cyc=%d,ppu=%03d,%03d)\n",
            g_last_reg_snapshot.pc,
            str_machine_code,
            mnemonic_to_str(g_cur_instr->mnemonic),
            str_param,
            g_last_reg_snapshot.acc,
            g_last_reg_snapshot.x,
            g_last_reg_snapshot.y,
            g_last_reg_snapshot.sp,
            g_last_reg_snapshot.status.serial,
            g_total_cycles_snapshot,
            g_ppu_scanline_snapshot,
            g_ppu_scanline_tick_snapshot);
}
#endif
