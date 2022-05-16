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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#define MAX_PATH_LEN 256

#define CNES_DIR "cnes"
#define SAVES_DIR "saves"

#if defined(_WIN32) && !defined(__CYGWIN__)
#define HOME_ENV_VAR "UserProfile"
#define APP_DATA_ENV_VAR "LOCALAPPDATA"
#define DEFAULT_DATA_PATH "AppData\\Local"
#else
#define HOME_ENV_VAR "HOME"
#define APP_DATA_ENV_VAR "XDG_DATA_HOME"
#define DEFAULT_DATA_PATH ".local/share"
#endif

static int pi_mkdir(const char *path) {
    struct stat st;
    stat(path, &st);
    if (st.st_mode & S_IFDIR) {
        return 0;
    }

    #ifdef _WIN32
    return _mkdir(path);
    #else
    return mkdir(path, 0755);
    #endif
}

static char *pi_getcwd(char *path, size_t max_len) {
    #ifdef _WIN32
    return _getcwd(path, max_len);
    #else
    return getcwd(path, max_len);
    #endif
}

static char *get_data_dir(void) {
    bool must_free = false;

    char *data_dir;

    const char *app_data_ev = getenv(APP_DATA_ENV_VAR);

    if (app_data_ev != NULL && strlen(app_data_ev) > 0) {
        data_dir = malloc(strlen(app_data_ev) + strlen(CNES_DIR) + 2);
        sprintf(data_dir, "%s/%s", app_data_ev, CNES_DIR);
    } else {
        const char *home_ev = getenv(HOME_ENV_VAR);

        if (home_ev != NULL && strlen(home_ev) > 0) {
            data_dir = malloc(strlen(home_ev) + strlen(DEFAULT_DATA_PATH) + strlen(CNES_DIR) + 3);
            sprintf(data_dir, "%s/%s/%s", home_ev, DEFAULT_DATA_PATH, CNES_DIR);
        } else {
            char *cwd = malloc(MAX_PATH_LEN);
            if (!pi_getcwd(cwd, MAX_PATH_LEN)) {
                printf("Failed to get working directory\n");
                return NULL;
            }

            data_dir = malloc(strlen(cwd) + strlen(CNES_DIR) + 2);
            sprintf(data_dir, "%s/%s", cwd, "." CNES_DIR);

            free(cwd);
        }
    }

    if (pi_mkdir(data_dir) != 0) {
        printf("Failed to create app data directory at %s\n", data_dir);
        free(data_dir);
        return NULL;
    }

    return data_dir;
}

static char *get_save_dir(void) {
    char *data_dir = get_data_dir();

    if (data_dir == NULL) {
        return NULL;
    }

    char *full_dir = malloc(strlen(data_dir) + strlen(SAVES_DIR) + 2);
    sprintf(full_dir, "%s/%s", data_dir, SAVES_DIR);

    free(data_dir);

    return full_dir;
}

static FILE *_open_game_file(char *game_title, char *file_name, char *flags) {
    char *save_dir = get_save_dir();

    if (save_dir == NULL) {
        printf("Failed to get save directory while opening %s\n", file_name);
        return NULL;
    }

    if (pi_mkdir(save_dir) != 0) {
        printf("Failed to create save directory while opening %s\n", file_name);
        free(save_dir);
        return NULL;
    }

    char *game_path = malloc(strlen(save_dir) + strlen(game_title) + 2);
    sprintf(game_path, "%s/%s", save_dir, game_title);
    free(save_dir);

    if (pi_mkdir(game_path) != 0) {
        printf("Failed to create game directory while opening %s\n", file_name);
        free(game_path);
        return NULL;
    }

    char *file_path = malloc(strlen(game_path) + strlen(file_name) + 2);
    sprintf(file_path, "%s/%s", game_path, file_name);
    free(game_path);

    FILE *save_file = fopen(file_path, flags);
    free(file_path);

    return save_file;
}

bool read_game_data(char *game_title, char *file_name, void *buf, size_t buf_len, bool quiet) {
    FILE *in_file = _open_game_file(game_title, file_name, "r");

    if (!in_file || !fread(buf, buf_len, 1, in_file)) {
        if (quiet) {
            printf("Failed to read from file %s\n", file_name);
        }
        return false;
    }

    fclose(in_file);

    return true;
}

bool write_game_data(char *game_title, char *file_name, void *buf, size_t buf_len) {
    FILE *out_file = _open_game_file(game_title, file_name, "w+");

    if (!out_file || !fwrite(buf, buf_len, 1, out_file)) {
        printf("Failed to write to file %s\n", file_name);
        return false;
    }

    fclose(out_file);

    return true;
}
