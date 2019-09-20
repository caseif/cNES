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
#include "cpu/instrs.h"
#include "input/input_device.h"
#include "input/standard/sc_driver.h"
#include "input/standard/standard_controller.h"
#include "ppu.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define FRAMES_PER_SECOND 60.0988
#define CYCLES_PER_FRAME 89342
#define CYCLES_PER_SECOND (FRAMES_PER_SECOND * CYCLES_PER_FRAME)

#define SLEEP_INTERVAL 10 // milliseconds

#define SRAM_FILE_NAME "sram.bin"

#define PRINT_SYS_MEMORY_ACCESS 0
#define PRINT_PPU_MEMORY_ACCESS 0

#define PRINT_INSTRS 0

bool halted = false;
bool stepping = false;
bool dead = false;

unsigned char g_system_ram[SYSTEM_MEMORY_SIZE];
unsigned char *g_prg_ram;
size_t g_prg_ram_size;
unsigned char *g_chr_ram;
size_t g_chr_ram_size;

static Cartridge *g_cart;

static uint8_t g_bus_val; // the value on the data bus

static uint8_t g_cycle_index = 0;
static uint64_t g_total_cpu_cycles = 0;

static bool g_dma_in_progress;
static uint8_t g_dma_page;
static unsigned int g_dma_step;

#if PRINT_INSTRS
// snapshots for logging
static CpuRegisters g_last_reg_snapshot; // the state of the registers when the last instruction started execution
static unsigned int g_total_cycles_snapshot;
static uint16_t g_ppu_scanline_snapshot;
static uint16_t g_ppu_scanline_tick_snapshot;
#endif

static void _init_controllers() {
    init_controllers();

    controller_connect(create_standard_controller(0));
    controller_connect(create_standard_controller(1));

    sc_attach_driver(sc_init, sc_poll_input);
}

static void _write_prg_nvram(Cartridge *cart) {
    printf("Saving SRAM to disk\n");
    write_game_data(g_cart->title, SRAM_FILE_NAME, g_prg_ram, cart->prg_nvram_size);
}

#if PRINT_INSTRS
static void _print_last_instr(char *instr_str) {
    printf("%04X  %s  (a=%02X,x=%02X,y=%02X,sp=%02X,p=%02X,cyc=%d,ppu=%03d,%03d)\n",
            g_last_reg_snapshot.pc,
            instr_str,
            g_last_reg_snapshot.acc,
            g_last_reg_snapshot.x,
            g_last_reg_snapshot.y,
            g_last_reg_snapshot.sp,
            g_last_reg_snapshot.status.serial,
            g_total_cycles_snapshot,
            g_ppu_scanline_snapshot,
            g_ppu_scanline_tick_snapshot);
}

static void _log_callback(char *instr_str) {
    _print_last_instr(instr_str);
    
    // store snapshots for logging
    g_last_reg_snapshot = cpu_get_registers();
    g_total_cycles_snapshot = g_total_cpu_cycles;
    g_ppu_scanline_snapshot = ppu_get_scanline();
    g_ppu_scanline_tick_snapshot = ppu_get_scanline_tick();
}
#endif

static void _handle_dma(void) {
    uint8_t index = ppu_get_internal_regs()->s;
    if (g_dma_step == 0) {
        // dummy read
        system_memory_read((g_dma_page << 8) | index);
    } else {
        if (g_dma_step == 1) {
            g_dma_step++; // advance cycle count regardless of whether we skip or not
            // skip cycle if cycle count is odd
            if (g_total_cpu_cycles % 2) {
                return;
            }
        }

        if (g_dma_step % 2) {
            // write
            ppu_push_dma_byte(g_bus_val);
        } else {
            // read
            g_bus_val = system_memory_read((g_dma_page << 8) | index);
        }
    }

    if (++g_dma_step > 514) {
        g_dma_in_progress = false;
    }
}

void initialize_system(Cartridge *cart) {
    g_cart = cart;

    if (cart->prg_ram_size > 0) {
        g_prg_ram_size = cart->prg_ram_size;
    } else if (cart->prg_nvram_size > 0) {
        g_prg_ram_size = cart->prg_nvram_size;
    }
    if (g_prg_ram_size > 0) {
        g_prg_ram = (unsigned char*) malloc(g_prg_ram_size);
    }

    if (cart->chr_ram_size > 0) {
        g_chr_ram_size = cart->chr_ram_size;
    } else if (cart->chr_nvram_size > 0) {
        g_chr_ram_size = cart->chr_nvram_size;
    }
    if (g_chr_ram_size > 0) {
        g_chr_ram = (unsigned char*) malloc(g_chr_ram_size);
    }

    if (g_cart->has_nv_ram && g_cart->prg_nvram_size > 0) {
        unsigned char prg_ram_tmp[g_prg_ram_size];
        if (read_game_data(cart->title, SRAM_FILE_NAME, prg_ram_tmp, g_prg_ram_size, true)) {
            printf("Loading SRAM from disk\n");
            memcpy(g_prg_ram, prg_ram_tmp, g_prg_ram_size);
        }
    }

    initialize_cpu();
    initialize_ppu();
    ppu_set_mirroring_mode(g_cart->mirror_mode ? MIRROR_VERTICAL : MIRROR_HORIZONTAL);

    _init_controllers();

    g_dma_page = 0xFF;

    #if PRINT_INSTRS
    cpu_set_log_callback(_log_callback);
    #endif
}

uint8_t system_open_bus_read(void) {
    return g_bus_val;
}

void system_open_bus_write(uint8_t val) {
    g_bus_val = val;
}

unsigned char *system_get_ram(void) {
    return g_system_ram;
}

unsigned char *system_get_prg_ram(void) {
    return g_prg_ram;
}

uint8_t system_prg_ram_read(uint16_t addr) {
    if (g_prg_ram_size > 0) {
        return g_chr_ram[addr % g_prg_ram_size];
    } else {
        return system_open_bus_read();
    }
}

void system_prg_ram_write(uint16_t addr, uint8_t val) {
    if (g_prg_ram_size > 0) {
        g_prg_ram[addr % g_prg_ram_size] = val;
    }
}

unsigned char *system_get_chr_ram(void) {
    return g_chr_ram;
}

uint8_t system_chr_ram_read(uint16_t addr) {
    if (g_chr_ram_size > 0) {
        return g_chr_ram[addr % g_chr_ram_size];
    } else {
        return addr >> 8; // PPU open bus, typically the high byte
    }
}

void system_chr_ram_write(uint16_t addr, uint8_t val) {
    if (g_chr_ram_size > 0) {
        g_chr_ram[addr % g_chr_ram_size] = val;
    }
}

void system_ram_init(void) {
    srand(time(0));
    for (size_t i = 0; i < SYSTEM_MEMORY_SIZE; i++) {
        g_system_ram[i] = rand();
    }
}

uint8_t system_ram_read(uint16_t addr) {
    assert(addr < SYSTEM_MEMORY_SIZE);
    return g_system_ram[addr];
}

void system_ram_write(uint16_t addr, uint8_t val) {
    assert(addr < SYSTEM_MEMORY_SIZE);
    g_system_ram[addr] = val;
}

uint8_t system_memory_read(uint16_t addr) {
    uint8_t res = g_cart->mapper->ram_read_func(g_cart, addr);

    #if PRINT_SYS_MEMORY_ACCESS
    printf("$%04X -> %02X\n", addr, res);
    #endif

    g_bus_val = res;

    return res;
}

void system_memory_write(uint16_t addr, uint8_t val) {
    #if PRINT_SYS_MEMORY_ACCESS
    printf("$%04X <- %02X\n", addr, val);
    #endif

    g_cart->mapper->ram_write_func(g_cart, addr, val);

    g_bus_val = val;
}

uint8_t system_vram_read(uint16_t addr) {
    uint8_t res = g_cart->mapper->vram_read_func(g_cart, addr);

    #if PRINT_PPU_MEMORY_ACCESS
    printf("$%04X -> %02X\n", addr, res);
    #endif

    return res;
}

void system_vram_write(uint16_t addr, uint8_t val) {
    #if PRINT_PPU_MEMORY_ACCESS
    printf("$%04X <- %02X\n", addr, val);
    #endif

    g_cart->mapper->vram_write_func(g_cart, addr, val);
}

uint8_t system_lower_memory_read(uint16_t addr) {
    assert(addr < 0x8000);

    switch (addr) {
        case 0 ... 0x1FFF:
            return system_ram_read(addr % SYSTEM_MEMORY_SIZE);
        case 0x2000 ... 0x3FFF:
            return ppu_read_mmio((uint8_t) (addr % 8));
        case 0x4014:
            //TODO: DMA register
            return 0;
        case 0x4000 ... 0x4013:
        case 0x4015:
            //TODO: APU MMIO
            return 0;
        case 0x4016 ... 0x4017: {
            return 0x40 | controller_poll(addr - 0x4016);
        }
        default:
            return system_open_bus_read(); // open bus
    }
}

void system_lower_memory_write(uint16_t addr, uint8_t val) {
    assert(addr < 0x8000);

    switch (addr) {
        case 0 ... 0x1FFF: {
            system_ram_write(addr % 0x800, val);
            return;
        }
        case 0x2000 ... 0x3FFF: {
            ppu_write_mmio((uint8_t) (addr % 8), val);
            return;
        }
        case 0x4014: {
            system_start_oam_dma(val);
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

void system_start_oam_dma(uint8_t page) {
    g_dma_in_progress = true;
    g_dma_page = page;
    g_dma_step = 0;
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

            if (g_cart->mapper->tick_func != NULL) {
                g_cart->mapper->tick_func();
            }

            if (g_cycle_index++ == 2) {
                g_cycle_index = 0;

                if (g_dma_in_progress) {
                    _handle_dma();
                } else {
                    cycle_cpu();
                }

                g_total_cpu_cycles++;
            }

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
        _write_prg_nvram(g_cart);
    }
    dead = true;
}
