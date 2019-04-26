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
#include "loader.h"
#include "renderer.h"
#include "sdl_manager.h"
#include "system.h"
#include "input/global/hotkeys.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void interrupt_handler(int signum) {
    kill_execution();
    close_window();
}

void *_start_system_thread(void *_) {
    do_system_loop();
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Too few args!\n");
        printf("Usage: %s <ROM>\n", argv[0]);
        exit(1);
    }

    if (argc > 2) {
        printf("Too many args!\n");
        printf("Usage: %s <ROM>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, interrupt_handler);

    char *rom_file_name = argv[1];

    FILE *rom_file = fopen(rom_file_name, "rb");

    if (!rom_file) {
        printf("Could not open ROM file %s.\n", rom_file_name);
        return -1;
    }

    Cartridge *cart = load_rom(rom_file);

    if (!cart) {
        printf("Failed to load ROM.\n");
        return -1;
    }

    printf("Successfully loaded ROM file %s.\n", rom_file_name);

    printf("Initializing global input handler...\n");

    init_global_hotkeys();

    printf("Starting execution...\n");

    initialize_window();
    initialize_renderer();

    initialize_system(cart);

    pthread_t system_thread;
    int rc;
    if ((rc = pthread_create(&system_thread, NULL, &_start_system_thread, NULL)) != 0) {
        fprintf(stderr, "Failed to create system thread (error code %d)\n", rc);
        return 1;
    }

    do_window_loop();

    return 0;
}
