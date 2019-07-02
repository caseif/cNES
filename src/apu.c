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

#include <stdbool.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mixer.h>

static ApuPulseRegisters g_regs_pulse_1;
static ApuPulseRegisters g_regs_pulse_2;
static ApuTriangleRegisters g_regs_triangle;
static ApuNoiseRegisters g_regs_noise;
static ApuDmcRegisters g_regs_dmc;
static ApuStatusRegister g_reg_status;
static ApuFrameCounterRegister g_reg_frame_counter;

static unsigned int g_waveform_pulse_0[] = { 0,  0,  0,  0,  0,  0,  0, 15};
static unsigned int g_waveform_pulse_1[] = { 0,  0,  0,  0,  0,  0, 15, 15};
static unsigned int g_waveform_pulse_2[] = { 0,  0,  0,  0, 15, 15, 15, 15};
static unsigned int g_waveform_pulse_3[] = {15, 15, 15, 15, 15, 15,  0,  0};

static unsigned int g_waveform_triangle[] = {15, 14, 13, 12, 11, 10,  9,  8,
                                             7,  6,  5,  4,  3,  2,  1,  0,
                                             0,  1,  2,  3,  4,  5,  6,  7,
                                             8,  9, 10, 11, 12, 13, 14, 15};

static unsigned int g_timer_pulse_1 = 0;

static unsigned int g_index_pulse_1 = 0;

static void init_audio(void) {
    SDL_AudioSpec requested = {};
    requested.freq = 
    requested.format = AUDIO_U8;
}

static void handle_pulse_1(void) {
    if (g_regs_pulse_1.timer < 8) {
        return;
    }

    if (g_regs_pulse_1.length_counter && g_regs_pulse_1.length_counter_load == 0) {
        return;
    }

    unsigned int *waveform;
    switch (g_regs_pulse_1.duty) {
        case 0:
            waveform = g_waveform_pulse_0;
            break;
        case 1:
            waveform = g_waveform_pulse_1;
            break;
        case 2:
            waveform = g_waveform_pulse_2;
            break;
        case 3:
            waveform = g_waveform_pulse_3;
            break;
    }

    //TODO

    if (--g_timer_pulse_1 == 0) {
        // oddly, the index counts downward
        g_index_pulse_1 = (g_index_pulse_1 - 1) % sizeof(waveform);
    }
}

void apu_tick(void) {
    //TODO
}

uint8_t apu_regster_read(uint8_t reg, uint8_t val) {
    assert(reg <= 0x13 || reg == 0x15 || reg == 0x17);

    switch (reg) {
        case 0x00 ... 0x03:
            return (g_regs_pulse_1.serial >> reg) & 0xFF;
        case 0x04 ... 0x07:
            return (g_regs_pulse_2.serial >> (reg % 4)) & 0xFF;
        case 0x08 ... 0x0B:
            return (g_regs_triangle.serial >> (reg % 4)) & 0xFF;
        case 0x0C ... 0x0F:
            return (g_regs_noise.serial >> (reg % 4)) & 0xFF;
        case 0x10 ... 0x13:
            return (g_regs_dmc.serial >> (reg % 4)) & 0xFF;
        case 0x15:
            //TODO: work to be done here
            return (g_reg_status.serial >> (reg % 4)) & 0xFF;
        case 0x17:
            return (g_reg_frame_counter.serial >> (reg % 4)) & 0xFF;
    }
}

void apu_register_write(uint8_t reg, uint8_t val) {
    assert(reg <= 0x13 || reg == 0x15 || reg == 0x17);

    switch (reg) {
        case 0x00 ... 0x03:
            g_regs_pulse_1.serial &= ~(0xFF << (reg * 8));
            g_regs_pulse_1.serial |= val << (reg * 8);
            break;
        case 0x04 ... 0x07:
            g_regs_pulse_2.serial &= ~(0xFF << ((reg % 4) * 8));
            g_regs_pulse_2.serial |= val << ((reg % 4) * 8);
            break;
        case 0x08 ... 0x0B:
            g_regs_triangle.serial &= (0xFF << ((reg % 4) * 8));
            g_regs_triangle.serial |= val << ((reg % 4) * 8);
            break;
        case 0x0C ... 0x0F:
            g_regs_noise.serial &= (0xFF << ((reg % 4) * 8));
            g_regs_noise.serial |= val << ((reg % 4) * 8);
            break;
        case 0x10 ... 0x13:
            g_regs_dmc.serial &= (0xFF << ((reg % 4) * 8));
            g_regs_dmc.serial |= val << ((reg % 4) * 8);
            break;
        case 0x15:
            g_reg_status.serial = val;
            break;
        case 0x17:
            g_reg_frame_counter.serial = val;
            break;
    }
}
