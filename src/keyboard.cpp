/*
 * libconutils
 *
 * Copyright (C) 2018 Vladislav Levenetz <octal.s@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <map>
#include <vector>

#include <unistd.h>

#include "conutils.h"

using namespace std;
using namespace conutils;

typedef map<char, int (*)(const vector<int>&)> keymap;

struct term_keymap {
    const char *name;
    keymap map;
};

/* Helper for parameters and meta keys setup */
static int esc_seq_param(const vector<int>& params, int key)
{
    int ret_key = Keyboard::Keyboard::KEY_UNKNOWN;

    if (params.empty())
        ret_key = key;
    else if (params.back() >= 1 && params.back() <= 8)
        ret_key = key + params.back() * Keyboard::MOD_META;

    return ret_key;
}

/* Helper macros for keymap entries */
#define PARAM(key) [](const vector<int>& params) { return esc_seq_param(params, (key)); }

/* Most common xterms keymaps */

static term_keymap xterm_keymap = {
    .name = "xterm",

    .map = {
        { '~', [](const vector<int>& params)
               {
                   int key;

                   if (params.empty())
                       return (int)Keyboard::KEY_UNKNOWN;

                   switch (params[0]) {
                   case 2:
                       key = Keyboard::KEY_INS;
                       break;
                   case 3:
                       key = Keyboard::KEY_DEL;
                       break;
                   case 5:
                       key = Keyboard::KEY_PGUP;
                       break;
                   case 6:
                       key = Keyboard::KEY_PGDOWN;
                       break;
                   case 15:
                       key = Keyboard::KEY_F5;
                       break;
                   case 17:
                       key = Keyboard::KEY_F6;
                       break;
                   case 18:
                       key = Keyboard::KEY_F7;
                       break;
                   case 19:
                       key = Keyboard::KEY_F8;
                       break;
                   case 20:
                       key = Keyboard::KEY_F9;
                       break;
                   case 21:
                       key = Keyboard::KEY_F10;
                       break;
                   case 23:
                       key = Keyboard::KEY_F11;
                       break;
                   case 24:
                       key = Keyboard::KEY_F12;
                       break;
                   default:
                       return (int)Keyboard::KEY_UNKNOWN;
                   }

                   if (params.size() == 2 && params[1] >= 1 && params[1] <= 8)
                       key += params[1] * Keyboard::MOD_META;
                   if (params.size() > 2)
                       key = Keyboard::KEY_UNKNOWN;

                   return (int)key;
               }
        },

        { 'A', PARAM(Keyboard::KEY_UP) },
        { 'B', PARAM(Keyboard::KEY_DOWN) },
        { 'C', PARAM(Keyboard::KEY_RIGHT) },
        { 'D', PARAM(Keyboard::KEY_LEFT) },
        { 'H', PARAM(Keyboard::KEY_HOME) },
        { 'F', PARAM(Keyboard::KEY_END) },
        { 'P', PARAM(Keyboard::KEY_F1) },
        { 'Q', PARAM(Keyboard::KEY_F2) },
        { 'R', PARAM(Keyboard::KEY_F3) },
        { 'S', PARAM(Keyboard::KEY_F4) },
        { 'H', PARAM(Keyboard::KEY_HOME) },
        { 'F', PARAM(Keyboard::KEY_END) },
    },
};

static term_keymap *term_keymap;

Keyboard::~Keyboard()
{
    /* Restore terminal settings. */
    tcsetattr(STDIN_FILENO, TCSANOW, &mOldTermios);
}

Keyboard *Keyboard::getInstance()
{
    static std::unique_ptr<Keyboard> instance = 0;
    Keyboard *kb = nullptr;

    if (instance)
        return instance.get();

    instance = unique_ptr<Keyboard>(new Keyboard());
    kb = instance.get();
    if (!kb)
        return nullptr;

    /* Set non-buffered mode. */
    if (setvbuf(stdin, NULL, _IONBF, 0))
        return nullptr;

    /* Write/discard already buffered data in the stream. */
    fflush(stdin);

    if (tcgetattr(STDIN_FILENO, &kb->mNewTermios))
        return nullptr;

    /* Save terminal settings. */
    kb->mOldTermios = kb->mNewTermios;

    kb->mNewTermios.c_lflag &= ~(ECHO | ICANON);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &kb->mNewTermios))
        return nullptr;

    kb->mFds.fd = STDIN_FILENO;
    kb->mFds.events = POLLIN;

    /* TODO: detect terminal for key maps. Default to xterm for now. */
    term_keymap = &xterm_keymap;

    return kb;
}

int Keyboard::pollOnce(int timeout_ms)
{
    int rc;
    int key = 0;

    rc = poll(&mFds, 1, timeout_ms);
    if (rc < 0)
        return rc;
    if (!rc)
        return -ETIMEDOUT;

    rc = read(STDIN_FILENO, &key, 1);
    if (rc < 0)
        return rc;

    /* Workaround for ESC key or ESC sequence start. */
    if (key == KEY_ESC) {
        rc = poll(&mFds, 1, 1);
        if (rc < 0)
            return rc;

        if (rc > 0)
            key = KEY_ESC_SEQ;
    }

    return key;
}

int Keyboard::parseEsc()
{
    int key;
    vector<int> params;
    int p = 0;
    bool running = true;

    /* Parse parameters in the form val ; val ; ... */
    while (running) {
        key = pollOnce();
        if (key < 0)
            return key;

        switch (key) {
        case '0' ... '9':
            key -= 48;
            p = p ? p * 10 + key : key;
            break;

        case ';':
            params.push_back(p);
            p = 0;
            break;

        default:
            if (p)
                params.push_back(p);
            running = false;
            break;
        }
    }

    auto entry = term_keymap->map.find(key);
    key = KEY_UNKNOWN;
    if (entry != term_keymap->map.end())
        key = entry->second(params);

    return key;
}

int Keyboard::waitForKey(int timeout_ms)
{
    int key = pollOnce(timeout_ms);

    if (key != KEY_ESC_SEQ || key < 0)
        return key;

    /* It's a sequence! Fetch the next key. */

    key = pollOnce();
    if (key < 0)
        return key;

    switch (key) {
    case '[':
    case 'O':
        key = parseEsc();
        break;

    default:
        /* Most ALT + key combinations will generate ESC + key */
        key += MOD_ALT;
    }

    return key;
}
