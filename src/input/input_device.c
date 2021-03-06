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

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define MIN_PORT 0
#define MAX_PORT 1

static Controller *controllers[2];

uint8_t nil_poller(Controller *controller) {
    return 0;
}

void nil_pusher(Controller *controller, uint8_t data) {
}

static Controller empty_controller = {0, CONTROLLER_TYPE_NONE, nil_poller, nil_pusher, NULL};

void init_controllers(void) {
    for (unsigned int i = MIN_PORT; i <= MAX_PORT; i++) {
        controllers[i] = &empty_controller;
    }
}

Controller *get_controller(unsigned int port) {
    assert(port >= MIN_PORT && port <= MAX_PORT);
    return controllers[port];
}

void controller_connect(Controller *controller) {
    assert(controller->id >= MIN_PORT && controller->id <= MAX_PORT);
    controllers[controller->id] = controller;
}

void controller_disconnect(unsigned int port) {
    assert(port >= MIN_PORT && port <= MAX_PORT);

    if (controllers[port] != &empty_controller) {
        if (controllers[port]->state) {
            free(controllers[port]->state);
        }
        free(controllers[port]);
    }

    controllers[port] = &empty_controller;
}

uint8_t controller_poll(unsigned int port) {
    assert(port >= MIN_PORT && port <= MAX_PORT);
 
    return controllers[port]->poller(controllers[port]);
}

void controller_push(unsigned int port, uint8_t data) {
    // there's only one output port, which is directed to both controllers
    for (int port = MIN_PORT; port <= MAX_PORT; port++) {
        controllers[port]->pusher(controllers[port], data);
    }
}
