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
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

static SDL_GameController *g_controller;

static void _init_controller() {
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return;
    }

    unsigned int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks == 0) {
        printf("No joysticks detected\n");
        return;
    }

    printf("Found %d connected joysticks\n", num_joysticks);

    unsigned int joystick;
    for (joystick = 0; joystick < num_joysticks; joystick++) {
        if (SDL_IsGameController(joystick)) {
            break;
        }
    }
    if (joystick == num_joysticks) {
        printf("Failed to recognize any joysticks as game controllers\n");
        return;
    }

    g_controller = SDL_GameControllerOpen(joystick);

    if (!g_controller) {
        printf("Failed to open joystick %d as controller\n", joystick);
    }
}

static unsigned int _get_controller_button(SDL_GameControllerButton button) {
    if (!g_controller) {
        return 0;
    }

    return SDL_GameControllerGetButton(g_controller, button);
}

void sc_init(void) {
   _init_controller();
}

void sc_poll_input(void) {
    int key_count;

    const uint8_t *key_states = SDL_GetKeyboardState(&key_count);

    bool button_states[] = {
        key_states[SDL_SCANCODE_Z] | _get_controller_button(SDL_CONTROLLER_BUTTON_B),               // a
        key_states[SDL_SCANCODE_X] | _get_controller_button(SDL_CONTROLLER_BUTTON_A),               // b
        key_states[SDL_SCANCODE_COMMA] | _get_controller_button(SDL_CONTROLLER_BUTTON_BACK),        // select
        key_states[SDL_SCANCODE_PERIOD] | _get_controller_button(SDL_CONTROLLER_BUTTON_START),      // start
        key_states[SDL_SCANCODE_UP] | _get_controller_button(SDL_CONTROLLER_BUTTON_DPAD_UP),        // up
        key_states[SDL_SCANCODE_DOWN] | _get_controller_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN),    // down
        key_states[SDL_SCANCODE_LEFT] | _get_controller_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT),    // left
        key_states[SDL_SCANCODE_RIGHT] | _get_controller_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)   // right
    };

    Controller *controller0 = get_controller(0);

    assert(controller0);

    sc_set_state(controller0, button_states);
}
