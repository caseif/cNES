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

#include "input/input_device.h"
#include "input/standard/sc_driver.h"
#include "input/standard/standard_controller.h"

#include <assert.h>
#include <SDL2/SDL_events.h>

void sc_poll_input(void) {
    int key_count;

    const uint8_t *key_states = SDL_GetKeyboardState(&key_count);

    bool button_states[] = {
        key_states[SDL_SCANCODE_Z],      // a
        key_states[SDL_SCANCODE_X],      // b
        key_states[SDL_SCANCODE_COMMA],  // select
        key_states[SDL_SCANCODE_PERIOD], // start
        key_states[SDL_SCANCODE_UP],     // up
        key_states[SDL_SCANCODE_DOWN],   // down
        key_states[SDL_SCANCODE_LEFT],   // left
        key_states[SDL_SCANCODE_RIGHT]   // right
    };

    Controller *controller0 = get_controller(0);

    assert(controller0);

    sc_set_state(controller0, button_states);
}
