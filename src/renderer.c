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
#include "sdl_manager.h"
#include "system.h"
#include "ppu.h"

#include <pthread.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define RGB_CHANNELS 3
#define BPP 8

static SDL_Renderer *g_renderer;

static RGBValue g_pixel_buffer[RESOLUTION_H][RESOLUTION_V];

static unsigned char g_pixel_rgb_data[RESOLUTION_V][RESOLUTION_H][RGB_CHANNELS];

static SDL_Texture *g_texture;

void initialize_renderer(void) {
    g_renderer = SDL_CreateRenderer(get_window(), -1, 0);

    if (!g_renderer) {
        printf("Failed to initialize renderer: %s\n", SDL_GetError());
        exit(-1);
    }

    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
            RESOLUTION_H, RESOLUTION_V);
}

void _render_frame(void) {
    SDL_UpdateTexture(g_texture, NULL, g_pixel_rgb_data, RESOLUTION_H * RGB_CHANNELS);

    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);

    SDL_RenderPresent(g_renderer);
};

void set_pixel(unsigned int x, unsigned int y, const RGBValue rgb) {
    g_pixel_buffer[x][y] = rgb;
}

void flush_frame(void) {
    for (unsigned int x = 0; x < RESOLUTION_H; x++) {
        for (unsigned int y = 0; y < RESOLUTION_V; y++) {
            const RGBValue rgb = g_pixel_buffer[x][y];

            g_pixel_rgb_data[y][x][0] = rgb.r;
            g_pixel_rgb_data[y][x][1] = rgb.g;
            g_pixel_rgb_data[y][x][2] = rgb.b;
        }
    }

    _render_frame();
}
