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
#include "cpu/cpu.h"
#include "input/global/hotkeys.h"
#include "ppu/ppu.h"

#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

#define KEY_MODE_NORMAL SDLK_F1
#define KEY_MODE_NAME_TABLE SDLK_F2
#define KEY_MODE_PATTERN_TABLE SDLK_F3
#define KEY_ACTION_CONTINUE SDLK_F5
#define KEY_ACTION_STEP SDLK_F6
#define KEY_ACTION_BREAK SDLK_F8
#define KEY_ACTION_DUMP_RAM SDLK_F9
#define KEY_ACTION_DUMP_VRAM SDLK_F10
#define KEY_ACTION_DUMP_OAM SDLK_F11

static void _global_hotkey_callback(SDL_Event *event) {
    switch (event->type) {
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case KEY_MODE_NORMAL:
                    set_render_mode(RM_NORMAL);
                    printf("Showing normal output\n");
                    break;
                case KEY_MODE_NAME_TABLE:
                    set_render_mode((int) get_render_mode() < RM_NT0 || (int) get_render_mode() >= RM_NT3
                            ? RM_NT0
                            : (RenderMode) ((unsigned int) get_render_mode() + 1));
                    printf("Showing name table %d\n", ((int) get_render_mode() - (int) RM_NT0));
                    break;
                case KEY_MODE_PATTERN_TABLE:
                    set_render_mode(RM_PT);
                    printf("Showing pattern tables\n");
                    break;
                case KEY_ACTION_CONTINUE:
                    if (!is_execution_halted()) {
                        printf("Can't continue during live execution\n");
                        break;
                    }

                    printf("Continuing execution\n");
                    continue_execution();
                    break;
                case KEY_ACTION_STEP:
                    if (!is_execution_halted()) {
                        printf("Can't step during live execution\n");
                        break;
                    }

                    printf("Stepping execution\n");
                    step_execution();
                    break;
                case KEY_ACTION_BREAK:
                    if (is_execution_halted()) {
                        printf("Execution is already halted\n");
                        break;
                    }

                    printf("Breaking execution\n");
                    break_execution();
                    break;
                case KEY_ACTION_DUMP_RAM:
                    printf("Dumping system RAM\n");
                    dump_ram();
                    break;
                case KEY_ACTION_DUMP_VRAM:
                    printf("Dumping VRAM\n");
                    dump_vram();
                    break;
                case KEY_ACTION_DUMP_OAM:
                    printf("Dumping OAM\n");
                    dump_oam();
                    break;
                    
            }
            break;
    }
}

void init_global_hotkeys(void) {
    add_event_callback(_global_hotkey_callback);
}
