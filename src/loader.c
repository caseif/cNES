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

#define NES_MAGIC 0x4E45531A
#define PRG_CHUNK_SIZE ((size_t) 0x4000)
#define CHR_CHUNK_SIZE ((size_t) 0x2000)

typedef struct {
    MirroringMode mirror_mode:1 PACKED;
    bool has_prg_ram:1 PACKED;
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

Cartridge *load_rom(FILE *file) {
    unsigned char buffer[8];

    // read the first 8 bytes
    fread(buffer, 8, 1, file);

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

    Flag6 flag6 = {0};
    memcpy(&flag6, &(buffer[6]), 1);
    Flag7 flag7 = {0};
    memcpy(&flag7, &(buffer[7]), 1);

    if (flag7.nes2) {
        printf("NES 2.0 format is unsupported.\n");
        return NULL;
    }

    uint8_t mapper = (flag7.mapper_high << 4) | flag6.mapper_low;

    if (mapper != 0) {
        printf("Only Mapper 0 is supported at this time (found %d).\n", mapper);
        return NULL;
    }

    // skip next 8 bytes (we don't care about them for the moment)
    fseek(file, 8, SEEK_CUR);

    // skip the trainer if present
    if (flag6.has_trainer) {
        fseek(file, 512, SEEK_CUR);
    }

    size_t read_items;

    char *prg_data = malloc(prg_size * PRG_CHUNK_SIZE);

    printf("Attempting to read %ld PRG chunks\n", prg_size);

    if ((read_items = fread(prg_data, PRG_CHUNK_SIZE, prg_size, file)) != prg_size) {
        printf("Failed to read all PRG data (expected %ld chunks, found %ld).\n", prg_size, read_items);

        free(prg_data);

        return NULL;
    }

    char *chr_data = malloc(chr_size * CHR_CHUNK_SIZE);

    printf("Attempting to read %ld CHR chunks\n", chr_size);

    if ((read_items = fread(chr_data, CHR_CHUNK_SIZE, chr_size, file)) != chr_size) {
        printf("Failed to read all CHR data (expected %ld chunks, found %ld).\n", chr_size, read_items);

        free(chr_data);
        free(prg_data);

        return NULL;
    }

    Cartridge *cart = malloc(sizeof(Cartridge));

    cart->prg_rom = prg_data;
    cart->chr_rom = chr_data;
    cart->prg_size = prg_size * PRG_CHUNK_SIZE;
    cart->chr_size = chr_size * CHR_CHUNK_SIZE;
    cart->mirror_mode = flag6.mirror_mode;
    cart->has_prg_ram = flag6.has_prg_ram;
    cart->ignore_mirror_ctrl = flag6.ignore_mirror_ctrl;
    cart->mapper = mapper;

    return cart;
}