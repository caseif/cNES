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

#include <SDL2/SDL.h>

#define RGB_CHANNELS 3
#define BPP 8

const RGBValue g_palette[] = {
    {0x7C, 0x7C, 0x7C}, {0x00, 0x00, 0xFC}, {0x00, 0x00, 0xBC}, {0x44, 0x28, 0xBC},
    {0x94, 0x00, 0x84}, {0xA8, 0x00, 0x20}, {0xA8, 0x10, 0x00}, {0x88, 0x14, 0x00},
    {0x50, 0x30, 0x00}, {0x00, 0x78, 0x00}, {0x00, 0x68, 0x00}, {0x00, 0x58, 0x00},
    {0x00, 0x40, 0x58}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xBC, 0xBC, 0xBC}, {0x00, 0x78, 0xF8}, {0x00, 0x58, 0xF8}, {0x68, 0x44, 0xFC},
    {0xD8, 0x00, 0xCC}, {0xE4, 0x00, 0x58}, {0xF8, 0x38, 0x00}, {0xE4, 0x5C, 0x10},
    {0xAC, 0x7C, 0x00}, {0x00, 0xB8, 0x00}, {0x00, 0xA8, 0x00}, {0x00, 0xA8, 0x44},
    {0x00, 0x88, 0x88}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xF8, 0xF8, 0xF8}, {0x3C, 0xBC, 0xFC}, {0x68, 0x88, 0xFC}, {0x98, 0x78, 0xF8},
    {0xF8, 0x78, 0xF8}, {0xF8, 0x58, 0x98}, {0xF8, 0x78, 0x58}, {0xFC, 0xA0, 0x44},
    {0xF8, 0xB8, 0x00}, {0xB8, 0xF8, 0x18}, {0x58, 0xD8, 0x54}, {0x58, 0xF8, 0x98},
    {0x00, 0xE8, 0xD8}, {0x78, 0x78, 0x78}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
    {0xFC, 0xFC, 0xFC}, {0xA4, 0xE4, 0xFC}, {0xB8, 0xB8, 0xF8}, {0xD8, 0xB8, 0xF8},
    {0xF8, 0xB8, 0xF8}, {0xF8, 0xA4, 0xC0}, {0xF0, 0xD0, 0xB0}, {0xFC, 0xE0, 0xA8},
    {0xF8, 0xD8, 0x78}, {0xD8, 0xF8, 0x78}, {0xB8, 0xF8, 0xB8}, {0xB8, 0xF8, 0xD8},
    {0x00, 0xFC, 0xFC}, {0xF8, 0xD8, 0xF8}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

static unsigned char g_pixel_buffer[RESOLUTION_H][RESOLUTION_V];

static unsigned char g_pixel_rgb_data[RESOLUTION_V][RESOLUTION_H][RGB_CHANNELS];

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;

void initialize_renderer(void) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_CreateWindowAndRenderer(RESOLUTION_H * 4, RESOLUTION_V * 4, SDL_WINDOW_SHOWN, &g_window, &g_renderer);

    SDL_SetWindowTitle(g_window, "cNES");

    SDL_ShowWindow(g_window);

    if (!g_window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    }
}

void _render_frame(void) {
    SDL_Texture *texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
            RESOLUTION_H, RESOLUTION_V);

    SDL_UpdateTexture(texture, NULL, g_pixel_rgb_data, RESOLUTION_H * RGB_CHANNELS);

    SDL_RenderCopy(g_renderer, texture, NULL, NULL);

    SDL_RenderPresent(g_renderer);

    SDL_Event event;
    SDL_PollEvent(&event);
    switch (event.type) {
        case SDL_QUIT:
            exit(0);
    }
};

void set_pixel(unsigned int x, unsigned int y, unsigned char palette_index) {
    g_pixel_buffer[x][y] = palette_index;
}

void flush_frame(void) {
    for (unsigned int x = 0; x < RESOLUTION_H; x++) {
        for (unsigned int y = 0; y < RESOLUTION_V; y++) {
            const RGBValue rgb = g_palette[g_pixel_buffer[x][y]];

            g_pixel_rgb_data[y][x][0] = rgb.r;
            g_pixel_rgb_data[y][x][1] = rgb.g;
            g_pixel_rgb_data[y][x][2] = rgb.b;
        }
    }

    _render_frame();
}


