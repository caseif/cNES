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
#include "ppu.h"
#include "renderer.h"
#include "system.h"
#include "util.h"
#include "input/input_device.h"
#include "input/standard/sc_driver.h"
#include "input/standard/standard_controller.h"

#include "c6502/cpu.h"
#include "c6502/instrs.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX(a, b) (a > b ? a : b)

#define LOG_PERFORMANCE 0
#define PRINT_SYS_MEMORY_ACCESS 0
#define PRINT_PPU_MEMORY_ACCESS 0
#define PRINT_INSTRS 0

#define FRAMES_PER_SECOND_NTSC 60.0988
#define MASTER_CLOCK_SPEED_NTSC 21477272
#define CPU_CLOCK_DIVIDER_NTSC 12
#define PPU_CLOCK_DIVIDER_NTSC 4

#define FRAMES_PER_SECOND_PAL 50.0070
#define MASTER_CLOCK_SPEED_PAL 26601712
#define CPU_CLOCK_DIVIDER_PAL 16
#define PPU_CLOCK_DIVIDER_PAL 5

#define FRAMES_PER_SECOND_DENDY 50.0070
#define MASTER_CLOCK_SPEED_DENDY 26601712
#define CPU_CLOCK_DIVIDER_DENDY 15
#define PPU_CLOCK_DIVIDER_DENDY 5

#define THROTTLE_SPEED 1
#define SLEEP_INTERVAL 1000 // microseconds
#define SLEEP_OVERHEAD 70 // microseconds

#define SRAM_FILE_NAME "sram.bin"
#define CHIPRAM_FILE_NAME "chipram.bin"

static bool g_halted = false;
static bool g_stepping = false;
static bool g_dead = false;

static unsigned char g_system_ram[SYSTEM_MEMORY_SIZE];
static unsigned char *g_prg_ram;
static size_t g_prg_ram_size;
static unsigned char *g_chr_ram;
static size_t g_chr_ram_size;

static unsigned char *g_chip_ram = NULL;
static size_t g_chip_ram_size = 0;

static Cartridge *g_cart;

static TvSystem g_tv_system;
static uint64_t g_master_clock_speed;
static uint64_t g_cpu_clock_divider;
static uint64_t g_ppu_clock_divider;
static uint64_t g_clock_divider_cd;

static uint8_t g_bus_val; // the value on the data bus

static uint8_t g_cycle_index = 0;
static uint64_t g_total_cpu_cycles = 7; // the initial reset's cycles aren't counted automatically

static bool g_dma_in_progress;
static uint8_t g_dma_page;
static unsigned int g_dma_step;

static unsigned int (*g_nmi_line_callback)(void);
static unsigned int (*g_irq_line_callback)(void);
static unsigned int (*g_rst_line_callback)(void);

static int g_rst_cycles = 0;

#if PRINT_INSTRS
// snapshots for logging
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
    if (g_chip_ram_size != 0) {
        write_game_data(g_cart->title, CHIPRAM_FILE_NAME, g_chip_ram, g_chip_ram_size);
    }
}

#if PRINT_INSTRS
static void _print_last_instr(char *instr_str, CpuRegisters *regs_snapshot) {
    printf("%04X  %s  (a=%02X,x=%02X,y=%02X,sp=%02X,p=%02X,cyc=%d,ppu=%03d,%03d)\n",
            regs_snapshot->pc,
            instr_str,
            regs_snapshot->acc,
            regs_snapshot->x,
            regs_snapshot->y,
            regs_snapshot->sp,
            regs_snapshot->status.serial,
            g_total_cycles_snapshot,
            g_ppu_scanline_snapshot,
            g_ppu_scanline_tick_snapshot);
}

static void _log_callback(char *instr_str, CpuRegisters regs_snapshot) {
    _print_last_instr(instr_str, &regs_snapshot);
    
    // store snapshots for logging
    g_total_cycles_snapshot = g_total_cpu_cycles;
    g_ppu_scanline_snapshot = ppu_get_scanline();
    g_ppu_scanline_tick_snapshot = ppu_get_scanline_tick();
}
#endif

static uint64_t now_us() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    return now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

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

static unsigned int _internal_rst_connection(void) {
    return g_rst_cycles > 0 ? 0 : 1;
}

void initialize_system(Cartridge *cart) {
    g_cart = cart;

    system_connect_rst_line(_internal_rst_connection);

    switch (cart->timing_mode) {
        case TIMING_MODE_NTSC:
        case TIMING_MODE_MULTI:
            printf("Using NTSC system timing\n");
            g_tv_system = TV_SYSTEM_NTSC;
            g_master_clock_speed = MASTER_CLOCK_SPEED_NTSC;
            g_cpu_clock_divider = CPU_CLOCK_DIVIDER_NTSC;
            g_ppu_clock_divider = PPU_CLOCK_DIVIDER_NTSC;
            break;
        case TIMING_MODE_PAL:
            printf("Using PAL system timing\n");
            g_tv_system = TV_SYSTEM_PAL;
            g_master_clock_speed = MASTER_CLOCK_SPEED_PAL;
            g_cpu_clock_divider = CPU_CLOCK_DIVIDER_PAL;
            g_ppu_clock_divider = PPU_CLOCK_DIVIDER_PAL;
            break;
        case TIMING_MODE_DENDY:
            printf("Using Dendy system timing\n");
            g_tv_system = TV_SYSTEM_DENDY;
            g_master_clock_speed = MASTER_CLOCK_SPEED_DENDY;
            g_cpu_clock_divider = CPU_CLOCK_DIVIDER_DENDY;
            g_ppu_clock_divider = PPU_CLOCK_DIVIDER_DENDY;
            break;
        default:
            printf("Unhandled case %d\n", cart->timing_mode);
            exit(1);
    }

    g_clock_divider_cd = g_cpu_clock_divider * g_ppu_clock_divider;

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
        unsigned char *prg_ram_tmp = malloc(g_prg_ram_size);
        if (read_game_data(cart->title, SRAM_FILE_NAME, prg_ram_tmp, g_prg_ram_size, true)) {
            printf("Loading SRAM from disk\n");
            memcpy(g_prg_ram, prg_ram_tmp, g_prg_ram_size);
        }
        free(prg_ram_tmp);
    }

    memset(g_system_ram, 0x00, SYSTEM_MEMORY_SIZE);

    initialize_cpu((CpuSystemInterface){
            system_memory_read,
            system_memory_write,
            system_bus_read,
            system_bus_write,
            system_read_nmi_line,
            system_read_irq_line,
            system_read_rst_line
    });
    initialize_ppu();
    ppu_set_mirroring_mode(g_cart->four_screen_mode
            ? MIRROR_FOUR_SCREEN
            : g_cart->mirror_mode
                ? MIRROR_VERTICAL
                : MIRROR_HORIZONTAL);

    _init_controllers();

    g_dma_page = 0xFF;

    #if PRINT_INSTRS
    cpu_set_log_callback(_log_callback);
    #endif
}

TvSystem system_get_tv_system(void) {
    return g_tv_system;
}

unsigned int system_read_nmi_line(void) {
    return g_nmi_line_callback != NULL ? g_nmi_line_callback() : 1;
}

unsigned int system_read_irq_line(void) {
    return g_irq_line_callback != NULL ? g_irq_line_callback() : 1;
}

unsigned int system_read_rst_line(void) {
    return g_rst_line_callback != NULL ? g_rst_line_callback() : 1;
}

uint8_t system_bus_read(void) {
    return g_bus_val;
}

void system_bus_write(uint8_t val) {
    g_bus_val = val;
}

uint8_t system_prg_ram_read(uint16_t addr) {
    return addr < g_prg_ram_size ? g_prg_ram[addr] : g_bus_val;
}

void system_prg_ram_write(uint16_t addr, uint8_t val) {
    if (addr < g_prg_ram_size) {
        g_prg_ram[addr] = val;
    }
    g_bus_val = val;
}

uint8_t system_chr_ram_read(uint16_t addr) {
    return addr < g_chr_ram_size ? g_chr_ram[addr] : g_bus_val;
}

void system_chr_ram_write(uint16_t addr, uint8_t val) {
    if (addr < g_chr_ram_size) {
        g_chr_ram[addr] = val;
    }
    g_bus_val = val;
}

void system_register_chip_ram(Cartridge *cart, unsigned char *ram, size_t size) {
    printf("Registering chip RAM for cartridge\n");
    g_chip_ram = ram;
    g_chip_ram_size = size;

    unsigned char *chip_ram_tmp = malloc(g_chip_ram_size);
    if (read_game_data(cart->title, CHIPRAM_FILE_NAME, chip_ram_tmp, g_chip_ram_size, true)) {
        printf("Loading chip RAM from disk\n");
        memcpy(g_chip_ram, chip_ram_tmp, g_chip_ram_size);
    }
    free(chip_ram_tmp);
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

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        return system_ram_read(addr % SYSTEM_MEMORY_SIZE);
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        return ppu_read_mmio((uint8_t) (addr % 8));
        } else if (addr == 0x4014) {
        //TODO: DMA register
        return 0;
        } else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015) {
        //TODO: APU MMIO
        return 0;
    } else if (addr >= 0x4016 && addr <= 0x4017) {
        return 0x40 | controller_poll(addr - 0x4016);
    } else {
        return system_bus_read(); // open bus
    }
}

void system_lower_memory_write(uint16_t addr, uint8_t val) {
    assert(addr < 0x8000);

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        system_ram_write(addr % 0x800, val);
        return;
    }
    else if (addr >= 0x2000 && addr <= 0x3FFF) {
        ppu_write_mmio((uint8_t) (addr % 8), val);
        return;
    } else if (addr == 0x4014) {
        system_start_oam_dma(val);
        return;
    }
    else if ((addr >= 0x4000 && addr <= 0x4013) || addr == 0x4015) {
        //TODO: APU MMIO
        return;
    }
    else if (addr >= 0x4016 && addr <= 0x4017) {
        controller_push(addr - 0x4016, val);
        return;
    } else {
        return; // do nothing
    }
}

void system_dump_ram(void) {
    FILE *out_file = fopen("ram.bin", "w+");

    if (!out_file) {
        printf("Failed to dump RAM (couldn't open file: %s)\n", strerror(errno));
        return;
    }

    fwrite(g_system_ram, SYSTEM_MEMORY_SIZE, 1, out_file);

    fclose(out_file);
}

void system_start_oam_dma(uint8_t page) {
    g_dma_in_progress = true;
    g_dma_page = page;
    g_dma_step = 0;
}

void do_system_loop(void) {
    unsigned int cycles_per_interval = g_master_clock_speed * SLEEP_INTERVAL / 1000000;

    unsigned int cycles_since_sleep = 0;

    uint64_t last_sleep = now_us();

    int cycles_since_log = 0;
    uint64_t last_log = now_us();

    while (true) {
        if (g_dead) {
            break;
        }

        if (!g_halted) {
            bool tick_cpu = (g_cycle_index % g_cpu_clock_divider) == 0;
            bool tick_ppu = (g_cycle_index % g_ppu_clock_divider) == 0;

            if (tick_ppu) {
                cycle_ppu();

                if (g_rst_cycles > 0) {
                    g_rst_cycles--;
                }
            }

            if (tick_cpu) {
                if (g_dma_in_progress) {
                    _handle_dma();
                } else {
                    cycle_cpu();
                }

                g_total_cpu_cycles++;
            }

            if (tick_ppu) {
                if (g_cart->mapper->tick_func != NULL) {
                    g_cart->mapper->tick_func();
                }
            }

            if (++g_cycle_index == g_clock_divider_cd) {
                g_cycle_index = 0;
            }

            if (g_stepping) {
                g_halted = true;
                g_stepping = false;
            }
        }

        #if THROTTLE_SPEED
        if (++cycles_since_sleep > cycles_per_interval) {
            uint64_t now = now_us();
            uint64_t delta_us = now - last_sleep;

            if (delta_us < SLEEP_INTERVAL) {
                uint64_t sleep_for_us = SLEEP_INTERVAL - delta_us;

                if (sleep_for_us > SLEEP_OVERHEAD) {
                    sleep_for_us -= SLEEP_OVERHEAD;
                    struct timespec sleep_for;
                    sleep_for.tv_sec = sleep_for_us / 1000000;
                    sleep_for.tv_nsec = (sleep_for_us % 1000000) * 1000;
                    
                    nanosleep(&sleep_for, &sleep_for);
                }
            }
            
            last_sleep = now_us();
            
            cycles_since_sleep = 0;
        }
        #endif

        #if LOG_PERFORMANCE
        if (cycles_since_log > g_master_clock_speed) {
            cycles_since_log = 0;

            uint64_t now = now_us();

            uint64_t delta_us = now - last_log;
            
            float fraction = 1000000.0 / delta_us;

            printf("Running at %.1f%% fullspeed\n", fraction * 100);

            last_log = now;
        } else {
            cycles_since_log++;
        }
        #endif
    }
}

void break_execution(void) {
    g_halted = true;
}

void continue_execution(void) {
    g_halted = false;
}

void step_execution(void) {
    g_halted = false;
    g_stepping = true;
}

bool is_execution_halted(void) {
    return g_halted;
}

void kill_execution(void) {
    if (g_cart->has_nv_ram) {
        _write_prg_nvram(g_cart);
    }
    g_dead = true;
}

void system_connect_nmi_line(unsigned int (*nmi_line_callback)(void)) {
    g_nmi_line_callback = nmi_line_callback;
}

void system_connect_irq_line(unsigned int (*irq_line_callback)(void)) {
    g_irq_line_callback = irq_line_callback;
}

void system_connect_rst_line(unsigned int (*irq_line_callback)(void)) {
    g_rst_line_callback = irq_line_callback;
}

void system_set_rst_cycles(unsigned int cycles) {
    g_rst_cycles = cycles;
}

void system_emit_pixel(unsigned int x, unsigned int y, const RGBValue color) {
    set_pixel(x, y, color);
}

void system_submit_frame(void) {
    submit_frame();
}
