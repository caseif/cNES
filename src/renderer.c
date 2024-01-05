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

#include "renderer.h"
#include "system.h"
#include "ppu.h"
#include "util.h"

#include <stdbool.h>
#include <SDL.h>

#define RGB_CHANNELS 3
#define BPP 8

#define VIEWPORT_TOP 8
#define VIEWPORT_BOTTOM 231

#define VIEWPORT_H RESOLUTION_H
#define VIEWPORT_V (VIEWPORT_BOTTOM - VIEWPORT_TOP + 1)

typedef unsigned char pixel_buffer_t[VIEWPORT_V][VIEWPORT_H][RGB_CHANNELS];

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;

static LinkedList g_callbacks = {0};

static RGBValue g_pixel_buffer[RESOLUTION_H][RESOLUTION_V];

static pixel_buffer_t g_pixel_rgb_data_1;
static pixel_buffer_t g_pixel_rgb_data_2;

static pixel_buffer_t *g_pixel_buffer_front = &g_pixel_rgb_data_1;
static pixel_buffer_t *g_pixel_buffer_back = &g_pixel_rgb_data_2;

static SDL_Texture *g_texture;

bool g_close_requested = false;

void _close_listener(SDL_Event *event) {
    switch (event->type) {
        case SDL_WINDOWEVENT:
            if (event->window.event != SDL_WINDOWEVENT_CLOSE) {
                break;
            }
            // intentional fall-through
        case SDL_QUIT:
            kill_execution();
            close_window();
            g_close_requested = true;
            return;
    }
}

void initialize_window() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    g_window = SDL_CreateWindow("cNES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            VIEWPORT_H * WINDOW_SCALE, VIEWPORT_V * WINDOW_SCALE, SDL_WINDOW_SHOWN);

    if (!g_window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        exit(-1);
    }

    add_event_callback(_close_listener);

    SDL_ShowWindow(g_window);
}

void do_window_loop(void) {
    while (true) {
        SDL_Event event;

        if (SDL_PollEvent(&event)) {
            LinkedList *item = &g_callbacks;
            do {
                if (item->value != NULL) {
                    ((EventCallback) item->value)(&event);
                }
                item = item->next;
            } while (item != NULL);
        }

        draw_frame();

        if (g_close_requested) {
            break;
        }

        SDL_PumpEvents();
    }
}

SDL_Window *get_window() {
    return g_window;
}

void close_window(void) {
    SDL_Quit();
}

void set_window_title(const char *title) {
    SDL_SetWindowTitle(g_window, title);
}

void add_event_callback(EventCallback callback) {
    add_to_linked_list(&g_callbacks, (void*) callback);
}

void initialize_renderer(void) {
    printf("Initializing renderer with base resolution %dx%d\n", VIEWPORT_H, VIEWPORT_V);

    g_renderer = SDL_CreateRenderer(get_window(), -1, 0);

    if (!g_renderer) {
        printf("Failed to initialize renderer: %s\n", SDL_GetError());
        exit(-1);
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
            VIEWPORT_H, VIEWPORT_V);
}

void set_pixel(unsigned int x, unsigned int y, const RGBValue rgb) {
    g_pixel_buffer[x][y] = rgb;
}

void submit_frame(void) {
    for (unsigned int x = 0; x < RESOLUTION_H; x++) {
        for (unsigned int y = 0; y < RESOLUTION_V; y++) {
            if (y < VIEWPORT_TOP || y > VIEWPORT_BOTTOM) {
                continue;
            }

            const RGBValue rgb = g_pixel_buffer[x][y];

            (*g_pixel_buffer_back)[y - VIEWPORT_TOP][x][0] = rgb.r;
            (*g_pixel_buffer_back)[y - VIEWPORT_TOP][x][1] = rgb.g;
            (*g_pixel_buffer_back)[y - VIEWPORT_TOP][x][2] = rgb.b;
        }
    }

    pixel_buffer_t *new_front = g_pixel_buffer_back;
    g_pixel_buffer_back = g_pixel_buffer_front;
    g_pixel_buffer_front = new_front;
    memcpy(*g_pixel_buffer_back, *g_pixel_buffer_front, sizeof(pixel_buffer_t));
}

void draw_frame(void) {
    SDL_UpdateTexture(g_texture, NULL, g_pixel_buffer_front, VIEWPORT_H * RGB_CHANNELS);

    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);

    SDL_RenderPresent(g_renderer);
}
