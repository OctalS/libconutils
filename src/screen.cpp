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

#include <iostream>

#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>

#include "conutils.h"

using namespace std;
using namespace conutils;

static int query_screen_size(size_t& width, size_t& height)
{
    struct winsize w;
    int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    if (!ret) {
        width = w.ws_col;
        height = w.ws_row;
    }

    return ret;
}

Screen::~Screen()
{
    clear();
    showCursor();
    close(mWinchFd);
}

Screen *Screen::getInstance()
{
    static std::unique_ptr<Screen> instance = 0;
    Screen *sc = nullptr;
    size_t width, height;
    sigset_t mask;

    if (instance)
        return instance.get();

    if (query_screen_size(width, height))
        return nullptr;

    instance = unique_ptr<Screen>(new Screen(width, height));
    sc = instance.get();
    if (!sc)
        return nullptr;

    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);

    if (sigprocmask(SIG_BLOCK, &mask, NULL))
        return nullptr;

    sc->mWinchFd = signalfd(-1, &mask, 0);
    if (sc->mWinchFd < 0)
        return nullptr;

    return sc;
}

int Screen::resize()
{
    size_t width, height;
    int ret;

    ret = query_screen_size(width, height);
    if (ret)
        return ret;

    ret = Surface::resize(width, height);
    if (ret)
        return ret;

    mBounds = {0, 0, (ssize_t)width, (ssize_t)height};
    return 0;
}

int Screen::wait_sigwinch(Rect& new_bounds)
{
    struct signalfd_siginfo fdsi;
    size_t width, height;
    ssize_t sz;
    int ret;

    sz = read(mWinchFd, &fdsi, sizeof(struct signalfd_siginfo));
    if (sz != sizeof(struct signalfd_siginfo))
        return -EIO;

    ret = query_screen_size(width, height);
    if (ret)
        return -EIO;

    new_bounds = {0, 0, (ssize_t)width, (ssize_t)height};
    return 0;
}

void Screen::clear()
{
    cout << "\x1b[0m\x1b[2J\x1b[1;1H";
    cout.flush();
    invalidate();
}

void Screen::showCursor()
{
    cout << "\x1b[?25h";
    cout.flush();
    mCursorVisible = true;
}

void Screen::hideCursor()
{
    cout << "\x1b[?25l";
    cout.flush();
    mCursorVisible = false;
}

void Screen::setCursorPos(const Point& pos)
{
    cout << "\x1b[" << pos.y << ";" << pos.x << "H";
    cout.flush();
}

/* TODO: add support for extended characters. */
void Screen::drawChar(const Char& ch)
{
    bool attr_changed = false;

    if (ch.attr != mCurrentAttr.attr || !mCurrentAttr.val) {
        /* Terminal attributes has changed. We must issue a reset. */
        cout << "\x1b[0m";

        if (ch.attr & Char::bold)
            cout << "\x1b[1m";
        if (ch.attr & Char::underscore)
            cout << "\x1b[4m";
        if (ch.attr & Char::blink)
            cout << "\x1b[5m";
        if (ch.attr & Char::reverse)
            cout << "\x1b[7m";
        attr_changed = true;
    }

    if (attr_changed || (ch.fg != mCurrentAttr.fg)) {
        cout << "\x1b[38;5;" << (int)ch.fg << "m";
        attr_changed = true;
    }

    if (attr_changed || (ch.bg != mCurrentAttr.bg)) {
        cout << "\x1b[48;5;" << (int)ch.bg << "m";
        attr_changed = true;
    }

    if (attr_changed)
        mCurrentAttr = ch;

    /* Display only printable characters to not mess up the layout. */
    if (isprint(ch.val))
        cout << ch.val;
    else
        cout << ' ';
}

void Screen::renderDone(const Rect& dirty)
{
    /* ANSI terminal coordinates starts from 1. */
    size_t x = dirty.top.x + 1;
    size_t y = dirty.top.y + 1;
    size_t x_cnt = x + dirty.width();
    size_t y_cnt = y + dirty.height();
    const Char *buf = data();
    size_t offset = 0;

    /* Invalidate current attributes. */
    mCurrentAttr = Char(0, 0, 0, 0);

    /* Update only the dirty region. */
    for (; y < y_cnt; y++) {
        x = dirty.top.x + 1;
        /* conutils coordinates start from 0. */
        offset = mBounds.index_for(Point(x - 1, y - 1));
        /* Goto x, y */
        cout << "\x1b[" << y << ";" << x << "H";
        for (; x < x_cnt; x++) {
            drawChar(buf[offset++]);
        }
    }

    cout.flush();
}
