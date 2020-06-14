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
#include "c6502/cpu.h"
#include "input/input_device.h"
#include "mappers/mappers.h"
#include "mappers/nrom.h"
#include "ppu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MMC3_DEBUG_LOGGING 0

#define CHR_RAM_SIZE 0x2000

#define CHR_BANK_GRANULARITY 0x400
#define PRG_BANK_GRANULARITY 0x2000

#define IRQ_COOLDOWN_PERIOD 8

// false -> $C000-DFFF fixed, $8000-9FFF swappable
// true  -> $8000-9FFF fixed, $C000-DFFF swappable
static bool g_prg_switch_ranges = false;
// false -> 2 banks at $0000, 4 at $1000
// true  -> 4 banks at $0000, 2 at $1000
static bool g_chr_inversion = false;

static uint8_t g_bank_select = 0;

static uint8_t g_chr_big_1 = 0;
static uint8_t g_chr_big_2 = 0;
static uint8_t g_chr_little_1 = 0;
static uint8_t g_chr_little_2 = 0;
static uint8_t g_chr_little_3 = 0;
static uint8_t g_chr_little_4 = 0;
static uint8_t g_prg_1 = 0;
static uint8_t g_prg_2 = 1;

static uint8_t g_irq_counter;
static uint8_t g_irq_latch;
static bool g_irq_reload;
static bool g_irq_enabled = false;
static uint16_t a12_cooldown = 0;
static bool g_pending_irq = false;

// submapper configurations
static bool g_use_counter_edge = false;
static bool g_use_a12_fall = false;

static uint32_t _mmc3_get_prg_offset(Cartridge *cart, uint16_t addr) {
    assert(addr >= 0x8000);

    uint8_t bank = 0;
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (g_prg_switch_ranges) {
            bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 2; // fixed, use second-to-last bank
        } else {
            bank = g_prg_1;
        }
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        bank = g_prg_2;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        if (g_prg_switch_ranges) {
            bank = g_prg_1;
        } else {
            bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 2; // fixed, use second-to-last bank
        }
    } else if (addr >= 0xE000 && addr <= 0xFFFF) {
        bank = (cart->prg_size / PRG_BANK_GRANULARITY) - 1; // fixed, used last bank
    }
    uint32_t offset = ((bank * PRG_BANK_GRANULARITY) | (addr % PRG_BANK_GRANULARITY)) % cart->prg_size;
    return offset;
}

static uint32_t _mmc3_get_chr_offset(Cartridge *cart, uint16_t addr) {
    assert(addr < 0x2000);

    // undo the inversion (so the 0x0000..0x0FFF and 0x1000..0x1FFF ranges are swapped back)
    if (g_chr_inversion) {
        addr ^= 0x1000;
    }

    uint16_t bank_size = addr < 0x1000 ? 0x800 : 0x400; // big banks come first

    uint8_t bank;

    if (addr >= 0x0000 && addr <= 0x07FF) {
        bank = g_chr_big_1;
    } else if (addr >= 0x0800 && addr <= 0x0FFF) {
        bank = g_chr_big_2;
    } else if (addr >= 0x1000 && addr <= 0x13FF) {
        bank = g_chr_little_1;
    } else if (addr >= 0x1400 && addr <= 0x17FF) {
        bank = g_chr_little_2;
    } else if (addr >= 0x1800 && addr <= 0x1BFF) {
        bank = g_chr_little_3;
    } else if (addr >= 0x1C00 && addr <= 0x1FFF) {
        bank = g_chr_little_4;
    }

    return ((bank * CHR_BANK_GRANULARITY) | (addr % bank_size)) % cart->chr_size;
}

static unsigned int _mmc3_irq_connection(void) {
    return g_pending_irq ? 0 : 1;
}

static void _mmc3_init(Cartridge *cart) {
    system_connect_irq_line(_mmc3_irq_connection);
}

static uint8_t _mmc3_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x6000) {
        return system_lower_memory_read(addr);
    }

    if (addr < 0x8000) {
        return system_prg_ram_read(addr % 0x2000);
    }

    uint32_t prg_offset = _mmc3_get_prg_offset(cart, addr);

    if (prg_offset >= cart->prg_size) {
        printf("Invalid PRG read from $%04x ($%04x is outside PRG ROM range)\n", addr, prg_offset);
        exit(-1);
    }

    return cart->prg_rom[prg_offset];
}

static void _mmc3_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x6000) {
        system_lower_memory_write(addr, val);
        return;
    }

    if (addr < 0x8000) {
        system_prg_ram_write(addr % 0x2000, val);
        return;
    }

    switch (addr & 0xE001) {
        case 0x8000:
            #if MMC3_DEBUG_LOGGING
            printf("$8000 write\n");
            printf("  PRG range switch: %01d -> %01d\n", g_prg_switch_ranges, (val >> 6) & 1);
            printf("  CHR inversion: %01d -> %01d\n", g_chr_inversion, (val >> 7) & 1);
            printf("  Range select: %01d\n", val & 0x7);
            #endif

            g_prg_switch_ranges = (val >> 6) & 1;
            g_chr_inversion = (val >> 7) & 1;
            g_bank_select = val & 0x7;

            return;
        case 0x8001: {
            uint8_t *bank;

            switch (g_bank_select) {
                case 0:
                    bank = &g_chr_big_1;
                    val &= 0xFE; // ignore last bit for double-width banks
                    break;
                case 1:
                    bank = &g_chr_big_2;
                    val &= 0xFE; // ignore last bit for double-width banks
                    break;
                case 2:
                    bank = &g_chr_little_1;
                    break;
                case 3:
                    bank = &g_chr_little_2;
                    break;
                case 4:
                    bank = &g_chr_little_3;
                    break;
                case 5:
                    bank = &g_chr_little_4;
                    break;
                case 6:
                    bank = &g_prg_1;
                    val &= 0x3F;
                    break;
                case 7:
                    bank = &g_prg_2;
                    val &= 0x3F;
                    break;
            }

            #if MMC3_DEBUG_LOGGING
            printf("$8001 write\n");
            printf("  Selected range: %01d\n", g_bank_select);
            printf("  New bank: %02d -> %02d\n", *bank, val);
            #endif

            *bank = val;

            return;
        }
        case 0xA000:
            ppu_set_mirroring_mode((val & 1) ? MIRROR_HORIZONTAL : MIRROR_VERTICAL);
            return;
        case 0xA001:
            // unimplemented for MMC3
            return;
        case 0xC000:
            g_irq_latch = val;
            return;
        case 0xC001:
            g_irq_counter = 0xFF;
            g_irq_reload = true;
            return;
        case 0xE000:
            if (g_irq_enabled) {
                g_irq_enabled = false;
            }
            g_pending_irq = false; // acknowledge any pending interrupt
            return;
        case 0xE001:
            g_irq_enabled = true;
            return;
        default:
            return; // ignore bogus write
    }
}

static uint8_t _mmc3_vram_read(Cartridge *cart, uint16_t addr) {
        if (addr >= 0x0000 && addr <= 0x1FFF) {
            if (cart->chr_size == 0) {
                return system_chr_ram_read(addr);
            }

            uint32_t chr_offset = _mmc3_get_chr_offset(cart, addr);
            
            if (chr_offset >= cart->chr_size) {
                printf("Invalid CHR read from $%04x ($%04x is outside CHR ROM range)\n", addr, chr_offset);
                exit(-1);
            }
            
            return cart->chr_rom[chr_offset];
        } else if (addr >= 0x2000 && addr <= 0x3EFF) {
            // name tables
            return ppu_name_table_read(addr % 0x1000);
        } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
            // palette table
            return ppu_palette_table_read(addr % 0x20);
        } else {
            // open bus, generally returns low address byte
            return addr & 0xFF;
        }
}

static void _mmc3_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        // CHR
        if (cart->chr_size == 0) {
            system_chr_ram_write(addr, val);
        }
        return;
    } else if (addr >= 0x2000 && addr <= 0x3EFF) {
        // name tables
        ppu_name_table_write(addr % 0x1000, val);
        return;
    } else if (addr >= 0x3F00 && addr <= 0x3FFF) {
        // palette table
        ppu_palette_table_write(addr % 0x20, val);
        return;
    }
}

static void _mmc3_tick(void) {
    #if MMC3_DEBUG_LOGGING
    printf("MMC3 counter: %d\n", g_irq_counter);
    #endif

    bool new_a12 = (ppu_get_internal_regs()->addr_bus & 0x1000);
    if (a12_cooldown > 0) {
        a12_cooldown--;
    }

    bool clock_counter;
    if (g_use_a12_fall) {
        clock_counter = !a12_cooldown && !new_a12;
    } else {
        clock_counter = !a12_cooldown && new_a12;
    }

    if (clock_counter) {
        #if MMC3_DEBUG_LOGGING
        printf("MMC3 IRQ counter clocked @ (%03d, %03d)\n", ppu_get_scanline(), ppu_get_scanline_tick());
        #endif
        uint8_t counter_old = g_irq_counter;

        if (g_irq_reload || g_irq_counter == 0) {
            g_irq_counter = g_irq_latch;
            g_irq_reload = false;
        } else {
            g_irq_counter--;
        }

        if ((!g_use_counter_edge || counter_old > 0) && g_irq_counter == 0 && g_irq_enabled) {
            g_pending_irq = true;
        }
    }

    if (new_a12 == !g_use_a12_fall) {
        a12_cooldown = IRQ_COOLDOWN_PERIOD;
    }
}

void mapper_init_mmc3(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_MMC3;
    memcpy(mapper->name, "MMC3", strlen("MMC3") + 1);
    mapper->init_func       = _mmc3_init;
    mapper->ram_read_func   = _mmc3_ram_read;
    mapper->ram_write_func  = _mmc3_ram_write;
    mapper->vram_read_func  = _mmc3_vram_read;
    mapper->vram_write_func = _mmc3_vram_write;
    mapper->tick_func       = _mmc3_tick;

    if (submapper_id == 3) {
        g_use_a12_fall = true;
    } else if (submapper_id == 4) {
        g_use_counter_edge = true; // trigger IRQ when counter changes to 0
    }
}
