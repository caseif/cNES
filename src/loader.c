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
#include "util.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAPPER_MSG "Found mapper %d (%s)\n"

#define NES_MAGIC 0x4E45531A
#define PRG_CHUNK_SIZE ((size_t) 0x4000)
#define CHR_CHUNK_SIZE ((size_t) 0x2000)
#define PRG_RAM_CHUNK_SIZE ((size_t) 0x2000)

typedef struct {
    unsigned int mirror_mode:1 PACKED;
    bool has_nv_ram:1 PACKED;
    bool has_trainer:1 PACKED;
    bool ignore_mirror_ctrl:1 PACKED;
    unsigned int mapper_low:4 PACKED;
} Flag6;

typedef struct {
    bool vs_unisystem:1 PACKED;
    bool play_choice_10:1 PACKED;
    unsigned int nes2:2 PACKED;
    unsigned int mapper_high:4 PACKED;
} Flag7;

typedef struct {
    unsigned int mapper_highest:4 PACKED;
    unsigned int submapper:4 PACKED;
} Flag8;

typedef struct {
    unsigned int prg_rom_size_msb:4 PACKED;
    unsigned int chr_rom_size_msb:4 PACKED;
} Flag9;

typedef struct {
    unsigned int prg_ram_shift_count:4 PACKED;
    unsigned int prg_nvram_shift_count:4 PACKED;
} Flag10;

typedef struct {
    unsigned int chr_ram_shift_count:4 PACKED;
    unsigned int chr_nvram_shift_count:4 PACKED;
} Flag11;

typedef struct {
    unsigned int timing_mode:2 PACKED;
    unsigned int :6 PACKED; // unused
} Flag12;

void _init_mapper(Mapper *mapper, void (*init_func)(Mapper*, unsigned int), unsigned int submapper_id) {
    init_func(mapper, submapper_id);
    printf(MAPPER_MSG, mapper->id, mapper->name);
}

Mapper *_create_mapper(unsigned int mapper_id, unsigned int submapper_id) {
    Mapper *mapper = (Mapper*) malloc(sizeof(Mapper));

    switch (mapper_id) {
        case MAPPER_ID_NROM:
            _init_mapper(mapper, mapper_init_nrom, submapper_id);
            break;
        case MAPPER_ID_MMC1:
            _init_mapper(mapper, mapper_init_mmc1, submapper_id);
            break;
        case MAPPER_ID_UNROM:
            _init_mapper(mapper, mapper_init_unrom, submapper_id);
            break;
        case MAPPER_ID_CNROM:
            _init_mapper(mapper, mapper_init_cnrom, submapper_id);
            break;
        case MAPPER_ID_MMC3:
            _init_mapper(mapper, mapper_init_mmc3, submapper_id);
            break;
        case MAPPER_ID_AXROM:
            _init_mapper(mapper, mapper_init_axrom, submapper_id);
            break;
        case MAPPER_ID_COLOR_DREAMS:
            _init_mapper(mapper, mapper_init_color_dreams, submapper_id);
            break;
        case MAPPER_ID_NAMCO_1XX:
            _init_mapper(mapper, mapper_init_namco_1xx, submapper_id);
            break;
        default:
            printf("Mapper %d is not supported at this time\n", mapper_id);
            free(mapper);
            return NULL;
    }

    return mapper;
}

Cartridge *load_rom(FILE *file, char *file_name) {
    unsigned char buffer[16];

    // read the first 16 bytes
    fread(buffer, 16, 1, file);

    // check the magic (the file format is Little Endian)
    uint32_t magic = endian_swap(*((uint32_t*) buffer));
    if (magic != NES_MAGIC) {
        printf("Bad magic! (0x%x)\n", magic);
        return NULL;
    }

    // extract the PRG ROM size
    size_t prg_size = buffer[4];

    // extract the CHR ROM size
    size_t chr_size = buffer[5];

    Flag6 flag6 = (Flag6) {};
    memcpy(&flag6, &(buffer[6]), 1);
    Flag7 flag7 = (Flag7) {};
    memcpy(&flag7, &(buffer[7]), 1);

    uint16_t mapper_id = (flag7.mapper_high << 4) | flag6.mapper_low;
    uint8_t submapper_id = 0;
    size_t prg_ram_size = PRG_RAM_CHUNK_SIZE;
    size_t prg_nvram_size = PRG_RAM_CHUNK_SIZE;
    size_t chr_ram_size = CHR_CHUNK_SIZE;
    size_t chr_nvram_size = CHR_CHUNK_SIZE;
    unsigned int timing_mode = TIMING_MODE_NTSC;

    if (flag7.nes2 == 2) {
        Flag8 flag8 = (Flag8) {};
        memcpy(&flag8, &(buffer[8]), 1);
        Flag9 flag9 = (Flag9) {};
        memcpy(&flag9, &(buffer[9]), 1);
        Flag10 flag10 = (Flag10) {};
        memcpy(&flag10, &(buffer[10]), 1);
        Flag11 flag11 = (Flag11) {};
        memcpy(&flag11, &(buffer[11]), 1);
        Flag12 flag12 = (Flag12) {};
        memcpy(&flag12, &(buffer[12]), 1);
        //TODO: rest of flags are unsupported for now

        mapper_id |= flag8.mapper_highest << 8;
        submapper_id = flag8.submapper;

        prg_size |= flag9.prg_rom_size_msb << 8;
        chr_size |= flag9.chr_rom_size_msb << 8;

        if (flag10.prg_ram_shift_count > 20) {
            printf("Refusing to grant more than 67 MB of PRG RAM\n");
            return NULL;
        } else if (flag10.prg_nvram_shift_count > 20) {
            printf("Refusing to grant more than 67 MB of PRG NVRAM\n");
            return NULL;
        } else if (flag11.chr_ram_shift_count > 20) {
            printf("Refusing to grant more than 67 MB of CHR RAM\n");
            return NULL;
        } else if (flag11.chr_nvram_shift_count > 20) {
            printf("Refusing to grant more than 67 MB of CHR NVRAM\n");
            return NULL;
        }

        prg_ram_size = flag10.prg_ram_shift_count > 0 ? (64 << flag10.prg_ram_shift_count) : 0;
        prg_nvram_size = flag10.prg_ram_shift_count > 0 ? (64 << flag10.prg_nvram_shift_count) : 0;

        chr_ram_size = flag11.chr_ram_shift_count > 0 ? (64 << flag11.chr_ram_shift_count) : 0;
        chr_nvram_size = flag11.chr_ram_shift_count > 0 ? (64 << flag11.chr_nvram_shift_count) : 0;

        timing_mode = flag12.timing_mode;
    }

    if (timing_mode == TIMING_MODE_PAL) {
        printf("PAL ROMs are not supported at this time\n");
        return NULL;
    } else if (timing_mode == TIMING_MODE_DENDY) {
        printf("Dendy ROMs are not supported at this time\n");
        return NULL;
    }

    Mapper *mapper = _create_mapper(mapper_id, submapper_id);
    if (!mapper) {
        printf("Failed to create mapper\n");
        return NULL;
    }

    if (flag6.has_trainer) {
        printf("ROMs with trainers are not supported at this time\n");
        return NULL;
    }

    size_t read_items;

    unsigned char *prg_data = (unsigned char*) malloc(prg_size * PRG_CHUNK_SIZE);

    printf("Attempting to read %ld PRG chunks\n", prg_size);

    if ((read_items = fread(prg_data, PRG_CHUNK_SIZE, prg_size, file)) != prg_size) {
        printf("Failed to read all PRG data (expected %ld chunks, found %ld).\n", prg_size, read_items);

        free(prg_data);

        return NULL;
    }

    unsigned char *chr_data = (unsigned char*) malloc(chr_size * CHR_CHUNK_SIZE);

    printf("Attempting to read %ld CHR chunks\n", chr_size);

    if ((read_items = fread(chr_data, CHR_CHUNK_SIZE, chr_size, file)) != chr_size) {
        printf("Failed to read all CHR data (expected %ld chunks, found %ld).\n", chr_size, read_items);

        free(chr_data);
        free(prg_data);

        return NULL;
    }

    Cartridge *cart = (Cartridge*) malloc(sizeof(Cartridge));

    cart->title = file_name;
    cart->mapper = mapper;
    cart->prg_rom = prg_data;
    cart->chr_rom = chr_data;
    cart->prg_size = prg_size * PRG_CHUNK_SIZE;
    cart->chr_size = chr_size * CHR_CHUNK_SIZE;
    cart->mirror_mode = flag6.mirror_mode;
    cart->has_nv_ram = flag6.has_nv_ram;
    cart->ignore_mirror_ctrl = flag6.ignore_mirror_ctrl;
    cart->prg_ram_size = prg_ram_size;
    cart->prg_nvram_size = prg_nvram_size;
    cart->chr_ram_size = chr_ram_size;
    cart->chr_nvram_size = chr_nvram_size;
    cart->timing_mode = timing_mode;

    if (mapper->init_func != NULL) {
        mapper->init_func(cart);
    }

    return cart;
}
