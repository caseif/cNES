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

#include <stdint.h>

typedef union {
    struct {
        unsigned int volume:4;
        unsigned int const_volume:1;
        unsigned int length_counter:1;
        unsigned int duty:2;
        struct {
            unsigned int shift_count:3;
            unsigned int negative:1;
            unsigned int period:3;
            unsigned int enabled:1;
        } sweep;
        union {
            struct {
                unsigned int timer_low:8;
                unsigned int timer_high:3;
            };
            unsigned int timer:11;
        };
        unsigned int length_counter_load:5;
    };
    unsigned int serial:32;
} ApuPulseRegisters;

typedef union {
    struct {
        unsigned int lin_counter_load:7;
        unsigned int lin_counter_control:1;
        unsigned int :8; // unused
        unsigned int timer_low:8;
        unsigned int timer_high:3;
        unsigned int length_counter_load:5;
    };
    uint32_t serial;
} ApuTriangleRegisters;

typedef union {
    struct {
        unsigned int volume:4;
        unsigned int const_volume:1;
        unsigned int length_counter_halt:1;
        unsigned int :10; // unused
        unsigned int period:4;
        unsigned int :3; // unused
        unsigned int loop:1;
        unsigned int :3; // unused
        unsigned int length_counter_load:3;
    };
    uint32_t serial;
} ApuNoiseRegisters;

typedef union {
    struct {
        unsigned int frequency:4;
        unsigned int :2; // unused
        unsigned int loop:1;
        unsigned int irq_enable:1;
        unsigned int load_counter:7;
        unsigned int sample_addr:8;
        unsigned int sample_length:8;
    };
    uint32_t serial;
} ApuDmcRegisters;

typedef union {
    struct {
        unsigned int pulse_1:1;
        unsigned int pulse_2:1;
        unsigned int triangle:1;
        unsigned int noise:1;
        unsigned int dmc:1;
        unsigned int :1; // unused
        unsigned int frame_interrupt:1;
        unsigned int dmc_interrurpt:1;
    };
    uint8_t serial;
} ApuStatusRegister;

typedef union {
    struct {
        unsigned int :6; // unused
        unsigned int irq_inhibit:1;
        unsigned int mode:1;
    };
    uint8_t serial;
} ApuFrameCounterRegister;

void apu_set_register(uint8_t reg, uint8_t val);
