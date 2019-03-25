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

#include "sdl_manager.h"
#include "system.h"
#include "util.h"
#include "ppu/ppu.h"

#include <stdbool.h>
#include <SDL2/SDL.h>

static SDL_Window *g_window;

static LinkedList g_callbacks;

void _close_listener(SDL_Event *event) {
    switch (event->type) {
        case SDL_WINDOWEVENT:
            if (event->window.event != SDL_WINDOWEVENT_CLOSE) {
                break;
            }
        case SDL_QUIT:
            kill_execution();
            close_window();
            return;
    }
}

void initialize_window() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    g_window = SDL_CreateWindow("cNES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            RESOLUTION_H * 2, RESOLUTION_V * 2, SDL_WINDOW_SHOWN);

    if (!g_window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        exit(-1);
    }

    add_event_callback(_close_listener);

    SDL_ShowWindow(g_window);
}

void do_window_loop(void) {
    SDL_Event event;

    while (SDL_WaitEvent(&event)) {
        LinkedList *item = &g_callbacks;
        do {
            if (item->value != NULL) {
                ((EventCallback*) item->value)(&event);
            }
            item = item->next;
        } while (item != NULL);

        SDL_PumpEvents();
    }
}

SDL_Window *get_window() {
    return g_window;
}

void close_window(void) {
    SDL_Quit();
}

void add_event_callback(EventCallback *callback) {
    add_to_linked_list(&g_callbacks, callback);
}
