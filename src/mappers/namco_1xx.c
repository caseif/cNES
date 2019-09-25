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
#include "mappers/mappers.h"
#include "mappers/nrom.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PRG_BANK_SHIFT 13
#define PRG_BANK_GRANULARITY (1 << PRG_BANK_SHIFT)
#define CHR_BANK_SHIFT 10
#define CHR_BANK_GRANULARITY (1 << CHR_BANK_SHIFT)

#define REGISTER_SHIFT 11

#define CHR_RAM_SIZE 0x2000

static unsigned char g_prg_banks[3];
static unsigned char g_chr_banks[12];

static bool g_write_protections[4];

static unsigned char g_chip_ram[0x80];
static unsigned char g_chip_ram_addr;

static bool g_sound_disable;
static bool g_disable_nt_0;
static bool g_disable_nt_1;

static uint16_t g_irq_counter = 0;
static bool g_irq_pending = false;

static void _namco_1xx_init(Cartridge *cart) {
    g_prg_banks[0] = 0;
    g_prg_banks[1] = 1;
    g_prg_banks[2] = (cart->prg_size >> PRG_BANK_SHIFT) - 2;
    
    if (cart->has_nv_ram) {
        system_register_chip_ram(cart, g_chip_ram, sizeof(g_chip_ram));
    }
}

static uint8_t _namco_1xx_ram_read(Cartridge *cart, uint16_t addr) {
    if (addr < 0x4800) {
        return system_lower_memory_read(addr);
    }

    if (addr < 0x5000) {
        unsigned char val = g_chip_ram[g_chip_ram_addr & 0x7F];
        // auto-increment
        if (g_chip_ram_addr & 0x80) {
            g_chip_ram_addr = (g_chip_ram_addr & 80) | ((g_chip_ram_addr + 1) & 0x7F);
        }
        return val;
    }

    if (addr < 0x6000) {
        return g_irq_counter >> ((addr >> REGISTER_SHIFT) & 1);
    }

    if (addr < 0x8000) {
        return system_prg_ram_read(addr - 0x6000);
    }

    uint8_t bank = addr >= 0xE000 ? (cart->prg_size >> PRG_BANK_SHIFT) - 1 : g_prg_banks[(addr - 0x8000) >> PRG_BANK_SHIFT];
    return cart->prg_rom[((bank << PRG_BANK_SHIFT) | (addr % PRG_BANK_GRANULARITY)) % cart->prg_size];
}

static void _namco_1xx_ram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr < 0x4800) {
        system_lower_memory_write(addr, val);
        return;
    }

    if (addr < 0x5000) {
        g_chip_ram[g_chip_ram_addr & 0x7F] = val;
        // auto-increment
        if (g_chip_ram_addr & 0x80) {
            g_chip_ram_addr = (g_chip_ram_addr & 80) | ((g_chip_ram_addr + 1) & 0x7F);
        }

        return;
    } else if (addr < 0x5800) {
        g_irq_counter &= ~0xFF;
        g_irq_counter |= val;
    } else if (addr < 0x6000) {
        g_irq_counter &= 0xFF;
        g_irq_counter |= (val << 8);
    } else if (addr < 0x8000) {
        if (g_write_protections[(addr - 0x6000) >> REGISTER_SHIFT]) {
            return;
        }

        system_prg_ram_write(addr - 0x6000, val);
    } else if (addr < 0xE000) {
        g_chr_banks[(addr - 0x8000) >> REGISTER_SHIFT] = val;
    } else if (addr < 0xE800) {
        g_prg_banks[0] = val & 0x3F;
        g_sound_disable = val & 0x40;
    } else if (addr < 0xF000) {
        g_prg_banks[1] = val & 0x3F;
        g_disable_nt_0 = val & 0x40;
        g_disable_nt_1 = val & 0x80;
    } else if (addr < 0xF800) {
        g_prg_banks[2] = val & 0x3F;
    } else {
        g_write_protections[0] = (val & ~0x40) || (val & 1);
        g_write_protections[1] = (val & ~0x40) || (val & 2);
        g_write_protections[2] = (val & ~0x40) || (val & 4);
        g_write_protections[3] = (val & ~0x40) || (val & 8);

        g_chip_ram_addr = val;
    }
}

static bool _does_ref_ntram(uint16_t addr) {
    if (addr < 0x1000) {
        return !g_disable_nt_0;
    } else if (addr < 0x2000) {
        return !g_disable_nt_1;
    } else {
        return true;
    }
}

static uint8_t _namco_1xx_vram_read(Cartridge *cart, uint16_t addr) {
    if (addr >= 0x3000) {
        return nrom_vram_read(cart, addr);
    }

    unsigned char bank = g_chr_banks[addr >> CHR_BANK_SHIFT];

    uint16_t total_banks = cart->chr_size >> CHR_BANK_SHIFT;
    if (bank >= 0xE0) {
        if (_does_ref_ntram(addr)) {
            return ppu_name_table_read(((bank % 2) * 0x400) | (addr & 0x03FF));
        } else {
            if ((bank - 0xE0) < total_banks) {
                bank = total_banks - 0x20 + (bank - 0xE0);
            } else {
                bank = 0xFF;
            }
        }
    }

    if (bank >= total_banks) {
        return system_bus_read();
    }

    return cart->chr_rom[((bank << CHR_BANK_SHIFT) | (addr & 0x3FF)) % cart->chr_size];
}

static void _namco_1xx_vram_write(Cartridge *cart, uint16_t addr, uint8_t val) {
    if (addr >= 0x3000) {
        nrom_vram_write(cart, addr, val);
        return;
    }

    unsigned char bank = g_chr_banks[addr >> CHR_BANK_SHIFT];

    uint8_t total_banks = cart->chr_size >> CHR_BANK_SHIFT;
    if (bank >= 0xE0) {
        if (_does_ref_ntram(addr)) {
            ppu_name_table_write(((bank % 2) * 0x400) | (addr & 0x03FF), val);
            return;
        } else {
            if ((bank - 0xE0) < total_banks) {
                bank = total_banks - 0x20 + (bank - 0xE0);
            } else {
                bank = 0xFF;
            }
        }
    }

    if (bank >= total_banks) {
        system_bus_write(val);
        return;
    }

    cart->chr_rom[((bank << CHR_BANK_SHIFT) | (addr & 0x3FF)) % cart->chr_size] = val;
}

static void _namco_1xx_tick(void) {
    if ((g_irq_counter & 0x7FFF) == 0x7FFF) {
        g_irq_pending = true;
    } else {
        g_irq_counter++;
    }

    if (g_irq_pending) {
        cpu_pull_down_irq_line();
    }
}

void mapper_init_namco_1xx(Mapper *mapper, unsigned int submapper_id) {
    mapper->id = MAPPER_ID_COLOR_DREAMS;
    memcpy(mapper->name, "Namco 1XX", strlen("Namco 1XX") + 1);
    mapper->init_func       = _namco_1xx_init;
    mapper->ram_read_func   = _namco_1xx_ram_read;
    mapper->ram_write_func  = _namco_1xx_ram_write;
    mapper->vram_read_func  = _namco_1xx_vram_read;
    mapper->vram_write_func = _namco_1xx_vram_write;
    mapper->tick_func       = _namco_1xx_tick;
}
