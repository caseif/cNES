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
#include "renderer.h"
#include "util.h"
#include "cpu/cpu.h"
#include "input/input_device.h"
#include "input/standard/sc_driver.h"
#include "input/standard/standard_controller.h"
#include "ppu/ppu.h"

#include <stdbool.h>
#include <time.h>

#define FRAMES_PER_SECOND 60.0988
#define CYCLES_PER_FRAME 29780.5
#define CYCLES_PER_SECOND (FRAMES_PER_SECOND * CYCLES_PER_FRAME)

#define SLEEP_INTERVAL 10 // milliseconds

static void _init_controllers() {
    init_controllers();

    connect_controller(0, create_standard_controller());

    sc_attach_driver(sc_poll_input);
}

void start_main_loop(Cartridge *cart) {
    initialize_cpu();
    initialize_ppu(cart, cart->mirror_mode);
    load_cartridge(cart);

    _init_controllers();

    initialize_renderer();

    unsigned int cycles_per_interval = CYCLES_PER_SECOND / 1000.0 * SLEEP_INTERVAL;

    unsigned int cycles_since_sleep = 0;

    time_t last_sleep = 0;

    while (true) {
        cycle_ppu();
        cycle_ppu();
        cycle_ppu();
        cycle_cpu();

        if (++cycles_since_sleep > cycles_per_interval) {
            sleep_cp(SLEEP_INTERVAL - (clock() - last_sleep) / 1000);
            cycles_since_sleep = 0;
            last_sleep = clock();
        }
    }
}
