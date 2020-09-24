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

#pragma once

#include <stdint.h>

#define CONTROLLER_TYPE_NONE 0
#define CONTROLLER_TYPE_STANDARD 1

// forward declaration
struct controller_t;

typedef uint8_t (*ControllerPollFunction)(struct controller_t *controller);
typedef void (*ControllerPushFunction)(struct controller_t *controller, uint8_t data);

typedef void (*NullaryCallback)(void);
typedef void (*UintConsumer)(unsigned int);

typedef struct controller_t {
    unsigned int id;
    unsigned int type;
    ControllerPollFunction poller;
    ControllerPushFunction pusher;
    void *state;
} Controller;

void init_controllers(void);

Controller *get_controller(unsigned int port);

void controller_connect(Controller *controller);

void controller_disconnect(unsigned int port);

uint8_t controller_poll(unsigned int port);

void controller_push(unsigned int port, uint8_t data);
