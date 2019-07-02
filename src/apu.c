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

#include "apu.h"

#include <SDL2/SDL_audio.h>

static ApuPulseRegisters g_regs_pulse_1;
static ApuPulseRegisters g_regs_pulse_2;
static ApuTriangleRegisters g_regs_triangle;
static ApuNoiseRegisters g_regs_noise;
static ApuDmcRegisters g_regs_dmc;
static ApuStatusRegister g_reg_status;
static ApuFrameCounterRegister g_reg_frame_counter;

static void set_pulse_register(ApuPulseRegisters regs, uint8_t index, uint8_t val) {
    switch (index) {
        case 0x0:
            regs.volume = val & 0x0F;
            regs.const_volume = (val >> 4) & 0x01;
            regs.length_counter = (val >> 5) & 0x01;
            regs.duty = (val >> 6) & 0x03;
            break;
        case 0x1:
            regs.sweep.shift_count = val & 0x07;
            regs.sweep.negative = (val >> 3) & 0x01;
            regs.sweep.period = (val >> 4) & 0x07;
            regs.sweep.enabled = (val >> 7) & 0x01;
            break;
        case 0x2:
            regs.timer_low = val;
            break;
        case 0x3:
            regs.timer_high = val & 0x07;
            regs.length_counter_load = (val >> 3) & 0x1F;
            break;
    }
}

void apu_regster_read(uint8_t reg, uint8_t val) {
    //TODO
}

void apu_register_write(uint8_t reg, uint8_t val) {
    assert(reg <= 0x13 || reg == 0x15 || reg == 0x17);

    switch (reg) {
        case 0x00 ... 0x03:
            set_pulse_register(g_regs_pulse_1, reg, val);
            break;
        case 0x04 ... 0x07:
            set_pulse_register(g_regs_pulse_2, reg % 0x04, val);
            break;
        case 0x08:
            g_regs_triangle.lin_counter_load = val & 0x7F;
            g_regs_triangle.lin_counter_control = (val >> 7) & 0x01;
            break;
        case 0x09:
            break; // unused
        case 0x0A:
            g_regs_triangle.timer_low = val;
            break;
        case 0x0B:
            g_regs_triangle.timer_high = val & 0x07;
            g_regs_triangle.length_counter_load = (val >> 3) & 0x1F;
            break;
        case 0x0C:
            g_regs_noise.volume = val & 0x0F;
            g_regs_noise.const_volume = (val >> 4) & 0x01;
            g_regs_noise.length_counter_halt = (val >> 5) & 0x01;
            // rest is unused
            break;
        case 0x0D:
            break; // unused
        case 0x0E:
            g_regs_noise.period = val & 0x0F;
            // middle is unused
            g_regs_noise.loop = (val >> 7) & 0x01;
            break;
        case 0x0F:
            // beginning is unused
            g_regs_noise.length_counter_load = (val >> 3) & 0x1F;
            break;
        case 0x10:
            g_regs_dmc.frequency = val & 0x0F;
            // middle is unused
            g_regs_dmc.loop = (val >> 6) & 0x01;
            g_regs_dmc.irq_enable = (val >> 7) & 0x01;
            break;
        case 0x11:
            g_regs_dmc.load_counter = val & 0x7F;
            // last bit is unused
            break;
        case 0x12:
            g_regs_dmc.sample_addr = val;
            break;
        case 0x13:
            g_regs_dmc.sample_length = val;
            break;
        case 0x15:
            //TODO
            break;
        case 0x17:
            // beginning is unused
            g_reg_frame_counter.irq_inhibit = (val >> 6) & 0x01;
            g_reg_frame_counter.mode = (val >> 7) & 0x01;
            break;
    }
}
