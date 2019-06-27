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
#include "fs.h"
#include "renderer.h"
#include "system.h"
#include "util.h"
#include "cpu/cpu.h"
#include "input/input_device.h"
#include "input/standard/sc_driver.h"
#include "input/standard/standard_controller.h"
#include "ppu/ppu.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define FRAMES_PER_SECOND 60.0988
#define CYCLES_PER_FRAME 29780.5
#define CYCLES_PER_SECOND (FRAMES_PER_SECOND * CYCLES_PER_FRAME)

#define SLEEP_INTERVAL 10 // milliseconds

#define SRAM_FILE_NAME "sram.bin"

bool halted = false;
bool stepping = false;
bool dead = false;

static Cartridge *g_cart;

static void _init_controllers() {
    init_controllers();

    controller_connect(0, create_standard_controller());

    sc_attach_driver(sc_init, sc_poll_input);
}

static void _write_prg_ram(void) {
    printf("Saving SRAM to disk\n");
    write_game_data(g_cart->title, SRAM_FILE_NAME, g_prg_ram, sizeof(g_prg_ram));
}

void initialize_system(Cartridge *cart) {
    g_cart = cart;

    if (g_cart->has_nv_ram) {
        unsigned char prg_ram_tmp[sizeof(g_prg_ram)];
        if (read_game_data(cart->title, SRAM_FILE_NAME, prg_ram_tmp, sizeof(prg_ram_tmp), true)) {
            printf("Loading SRAM from disk\n");
            memcpy(g_prg_ram, prg_ram_tmp, sizeof(prg_ram_tmp));
        }
    }

    initialize_cpu();
    initialize_ppu();
    ppu_set_mirroring_mode(g_cart->mirror_mode ? MIRROR_VERTICAL : MIRROR_HORIZONTAL);
    cpu_init_pc(system_ram_read(0xFFFC) | (system_ram_read(0xFFFD) << 8));

    _init_controllers();
}

uint8_t system_ram_read(uint16_t addr) {
    return g_cart->mapper->ram_read_func(g_cart, addr);
}

void system_ram_write(uint16_t addr, uint8_t val) {
    g_cart->mapper->ram_write_func(g_cart, addr, val);
}

uint8_t system_vram_read(uint16_t addr) {
    return g_cart->mapper->vram_read_func(g_cart, addr);
}

void system_vram_write(uint16_t addr, uint8_t val) {
    g_cart->mapper->vram_write_func(g_cart, addr, val);
}

uint8_t system_lower_memory_read(uint16_t addr) {
    assert(addr < 0x8000);

    switch (addr) {
        case 0 ... 0x1FFF:
            return cpu_ram_read(addr % 0x800);
        case 0x2000 ... 0x3FFF:
            return ppu_read_mmio((uint8_t) (addr % 8));
        case 0x4014:
            //TODO: DMA register
            return 0;
        case 0x4000 ... 0x4013:
        case 0x4015:
            //TODO: APU MMIO
            return 0;
        case 0x4016 ... 0x4017:
            return controller_poll(addr - 0x4016);
        default:
            return 0; // open bus
    }
}

void system_lower_memory_write(uint16_t addr, uint8_t val) {
    assert(addr < 0x8000);

    switch (addr) {
        case 0 ... 0x1FFF: {
            cpu_ram_write(addr % 0x800, val);
            return;
        }
        case 0x2000 ... 0x3FFF: {
            ppu_write_mmio((uint8_t) (addr % 8), val);
            return;
        }
        case 0x4014: {
            cpu_start_oam_dma(val);
            return;
        }
        case 0x4000 ... 0x4013:
        case 0x4015: {
            //TODO: APU MMIO
            return;
        }
        case 0x4016 ... 0x4017: {
            controller_push(addr - 0x4016, val);
            return;
        }
        default:
            return; // do nothing
    }
}

void do_system_loop(void) {
    unsigned int cycles_per_interval = CYCLES_PER_SECOND / 1000.0 * SLEEP_INTERVAL;

    unsigned int cycles_since_sleep = 0;

    time_t last_sleep = 0;

    while (true) {
        if (dead) {
            break;
        }

        if (!halted) {
            cycle_ppu();
            cycle_ppu();
            cycle_ppu();

            if (g_cart->mapper->tick_func != NULL) {
                g_cart->mapper->tick_func();
            }

            cycle_cpu();

            if (stepping) {
                halted = true;
                stepping = false;
            }
        }

        if (++cycles_since_sleep > cycles_per_interval) {
            sleep_cp(SLEEP_INTERVAL - (clock() - last_sleep) / 1000);
            cycles_since_sleep = 0;
            last_sleep = clock();
        }
    }
}

void break_execution(void) {
    halted = true;
}

void continue_execution(void) {
    halted = false;
}

void step_execution(void) {
    halted = false;
    stepping = true;
}

bool is_execution_halted(void) {
    return halted;
}

void kill_execution(void) {
    if (g_cart->has_nv_ram) {
        _write_prg_ram();
    }
    dead = true;
}
