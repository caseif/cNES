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

#include "test_assert.h"
#include "cpu/cpu_tester.h"

#include "cpu/cpu.h"

extern CpuRegisters g_cpu_regs;

bool test_store_load(void) {
    load_cpu_test("store_load.bin");

    // ACCUMULATOR TESTS
    // test zero-page addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x10));
    ASSERT_EQ(1, memory_read(0x90));
    ASSERT_EQ(1, memory_read(0xFF));

    // test zero-page loading
    pump_cpu();
    ASSERT_EQ(1, g_cpu_regs.acc);
    ASSERT_EQ(1, g_cpu_regs.x);
    ASSERT_EQ(1, g_cpu_regs.y);

    // test zero-page (x-indexed) addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x12));
    ASSERT_EQ(1, memory_read(0x92));
    ASSERT_EQ(1, memory_read(0x01));
    ASSERT_EQ(1, memory_read(0xA1));
    ASSERT_EQ(1, memory_read(0x02));
    ASSERT_EQ(1, memory_read(0x11));

    // test zero-page (x-indexed) loading
    pump_cpu();
    ASSERT_EQ(6, g_cpu_regs.acc);

    // test absolute addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x0023));
    ASSERT_EQ(1, memory_read(0x0303));
    ASSERT_EQ(1, memory_read(0x0103));
    ASSERT_EQ(1, memory_read(0x0203));
    ASSERT_EQ(1, memory_read(0x0303));

    // test absolute (x-indexed) addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x0025));
    ASSERT_EQ(1, memory_read(0x0305));
    ASSERT_EQ(1, memory_read(0x0105));
    ASSERT_EQ(1, memory_read(0x0205));
    ASSERT_EQ(1, memory_read(0x0305));
    ASSERT_EQ(1, memory_read(0x0005));

    // test absolute (y-indexed) addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x0026));
    ASSERT_EQ(1, memory_read(0x0006));

    // test indexed indirect addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x0213));
    ASSERT_EQ(1, memory_read(0x1302));
    ASSERT_EQ(1, memory_read(0x0302));

    // test indirect indexed addressing
    pump_cpu();
    ASSERT_EQ(1, memory_read(0x0215));
    ASSERT_EQ(1, memory_read(0x1304));

    // X REGISTER TESTS
    // test zero-page addressing
    pump_cpu();
    ASSERT_EQ(2, memory_read(0x10));
    ASSERT_EQ(2, memory_read(0x90));
    ASSERT_EQ(2, memory_read(0xFF));

    // test zero-page (y-indexed) addressing
    pump_cpu();
    ASSERT_EQ(2, memory_read(0x14));
    ASSERT_EQ(2, memory_read(0x94));
    ASSERT_EQ(2, memory_read(0x03));
    ASSERT_EQ(2, memory_read(0xA0));
    ASSERT_EQ(2, memory_read(0x00));
    ASSERT_EQ(2, memory_read(0x20));

    // Y REGISTER TESTS
    // test zero-page addressing
    pump_cpu();
    ASSERT_EQ(4, memory_read(0x10));
    ASSERT_EQ(4, memory_read(0x90));
    ASSERT_EQ(4, memory_read(0xFF));

    // transfer tests
    pump_cpu();
    ASSERT_EQ(0x42, memory_read(0x08));
    ASSERT_EQ(0x52, memory_read(0x09));

    return true;
}
