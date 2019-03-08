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

#include "input/global/toggles.h"
#include "ppu/ppu.h"

#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

#define KEY_MODE_NORMAL SDLK_F1
#define KEY_MODE_NAME_TABLE SDLK_F2
#define KEY_MODE_PATTERN_TABLE SDLK_F3

static void *_toggle_listener(void *_) {
    SDL_Event event;

    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case KEY_MODE_NORMAL:
                        set_render_mode(RM_NORMAL);
                        break;
                    case KEY_MODE_NAME_TABLE:
                        set_render_mode(get_render_mode() == RM_NT3 ? RM_NT0 : (RenderMode) ((unsigned int) get_render_mode() + 1));
                        break;
                    case KEY_MODE_PATTERN_TABLE:
                        set_render_mode(RM_PT);
                        break;
                }
                break;
        }
    }

    return NULL;
}

int init_toggle_listener(void) {
    pthread_t listener_thread;

    int rc;
    if ((rc = pthread_create(&listener_thread, NULL, &_toggle_listener, NULL)) != 0) {
        fprintf(stderr, "Failed to initialize toggle listener (error code %d)\n", rc);
        return 1;
    }

    return 0;
}
