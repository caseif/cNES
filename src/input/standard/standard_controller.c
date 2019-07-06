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

#include "input/standard/standard_controller.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool button_states[8];
    bool strobe;
    unsigned int bit;
} ScState;

static UintConsumer g_poll_callback;

void sc_attach_driver(NullaryCallback init, UintConsumer callback) {
    init();
    g_poll_callback = callback;
}

uint8_t _sc_poll(Controller *controller) {
    ScState *state_cast = (ScState*) controller->state;

    if (state_cast->strobe) {
        state_cast->bit = 0;

        g_poll_callback(controller->id);
    } else if (state_cast->bit > 7) {
        return 1; // input is tied to vcc, so extra reads reutrn 1
    }

    // for some reason, the ternary expression originally here would sometimes evaluate to 2
    uint8_t res;
    if (state_cast->button_states[state_cast->bit]) {
        res = 1;
    } else {
        res = 0;
    }

    state_cast->bit += 1;
    return res;
}

void _sc_push(Controller *controller, uint8_t data) {
    ScState *state_cast = (ScState*) controller->state;

    state_cast->strobe = data & 1;

    if (state_cast->strobe) {
        state_cast->bit = 0;
        g_poll_callback(controller->id);
    }
}

Controller *create_standard_controller(unsigned int controller_id) {
    Controller *controller = (Controller*) malloc(sizeof(Controller));
    controller->id = controller_id;
    controller->poller = _sc_poll;
    controller->pusher = _sc_push;
    controller->state = (ScState*) malloc(sizeof(ScState));

    return controller;
}

void sc_set_state(Controller *controller, bool button_states[]) {
    memcpy(((ScState*) controller->state)->button_states, button_states, sizeof(bool) * 8);
}
