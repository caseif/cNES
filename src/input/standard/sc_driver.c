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
#include <SDL.h>
#include <SDL_events.h>

#define BUTTON_COUNT 8

static SDL_GameController *g_controller_0;
static SDL_GameController *g_controller_1;

static SDL_Scancode g_poll_keys[BUTTON_COUNT] = {
        SDL_SCANCODE_Z,
        SDL_SCANCODE_X,
        SDL_SCANCODE_COMMA,
        SDL_SCANCODE_PERIOD,
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT
};

static SDL_GameControllerButton g_poll_buttons[BUTTON_COUNT] = {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
};

static void _init_controllers() {
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

    unsigned int joystick_0;
    for (joystick_0 = 0; joystick_0 < num_joysticks; joystick_0++) {
        if (SDL_IsGameController(joystick_0)) {
            break;
        }
    }
    if (joystick_0 == num_joysticks) {
        printf("Failed to recognize any joysticks as game controllers\n");
        return;
    }

    g_controller_0 = SDL_GameControllerOpen(joystick_0);

    if (!g_controller_0) {
        printf("Failed to open joystick %d as controller 1\n", joystick_0);
        return;
    }

    printf("Using joystick %d for player 1 controller\n", joystick_0);

    unsigned int joystick_1;
    for (joystick_1 = 4; joystick_1 < num_joysticks; joystick_1++) {
        if (SDL_IsGameController(joystick_1)) {
            break;
        }
    }
    if (joystick_1 != num_joysticks) {
        g_controller_1 = SDL_GameControllerOpen(joystick_1);
        if (!g_controller_1) {
            printf("Failed to open joystick %d as controller 2\n", joystick_1);
            return;
        }
    }

    printf("Using joystick %d for player 2 controller\n", joystick_1);
}

static unsigned int _get_controller_button(unsigned int controller_id, SDL_GameControllerButton button) {
    assert(controller_id <= 1);

    SDL_GameController *controller = controller_id == 0 ? g_controller_0 : g_controller_1;

    if (!controller) {
        return 0;
    }

    return SDL_GameControllerGetButton(controller, button);
}

static unsigned int _get_controller_axis(unsigned int controller_id, SDL_GameControllerAxis axis, bool expected) {
    assert(controller_id <= 1);

    SDL_GameController *controller = controller_id == 0 ? g_controller_0 : g_controller_1;

    if (!controller) {
        return 0;
    }

    int val = SDL_GameControllerGetAxis(controller, axis);
    return expected ? val >= 16384 : val <= -16384;
}

void sc_init(void) {
   _init_controllers();
}

void sc_poll_input(unsigned int controller_id) {
    assert(controller_id <= 1);

    int key_count;

    const uint8_t *key_states = SDL_GetKeyboardState(&key_count);

    bool button_states[8];
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (controller_id == 0) {
            button_states[i] = key_states[g_poll_keys[i]]
                    | _get_controller_button(controller_id, g_poll_buttons[i]);
        } else {
            button_states[i] = _get_controller_button(controller_id, g_poll_buttons[i]);
        }
    };

    button_states[4] |= _get_controller_axis(controller_id, SDL_CONTROLLER_AXIS_LEFTY, 0);
    button_states[5] |= _get_controller_axis(controller_id, SDL_CONTROLLER_AXIS_LEFTY, 1);
    button_states[6] |= _get_controller_axis(controller_id, SDL_CONTROLLER_AXIS_LEFTX, 0);
    button_states[7] |= _get_controller_axis(controller_id, SDL_CONTROLLER_AXIS_LEFTX, 1);

    Controller *controller = get_controller(controller_id);

    assert(controller);

    sc_set_state(controller, button_states);
}
