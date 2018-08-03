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

#include <sstream>

#include "conutils.h"

using namespace std;
using namespace conutils;

string Point::str() const
{
    stringstream ss;
    ss << x << ", " << y;
    return ss.str();
}

const Rect Rect::intersect(const Rect& r1, const Rect& r2)
{
    Rect r;

    r.top.x = max(r1.top.x, r2.top.x);
    r.top.y = max(r1.top.y, r2.top.y);
    r.bottom.x = min(r1.bottom.x, r2.bottom.x);
    r.bottom.y = min(r1.bottom.y, r2.bottom.y);

    return r;
}

const Rect Rect::boundingRect(const Rect& r1, const Rect& r2)
{
    Rect r;

    r.top.x = min(r1.top.x, r2.top.x);
    r.top.y = min(r1.top.y, r2.top.y);
    r.bottom.x = max(r1.bottom.x, r2.bottom.x);
    r.bottom.y = max(r1.bottom.y, r2.bottom.y);

    return r;
}

Rect& Rect::move(const Point& pos)
{
    ssize_t w = width();
    ssize_t h = height();

    top = pos;
    bottom = Point(top.x + w, top.y + h);

    return *this;
}

string Rect::str() const
{
    return top.str() + ", " + bottom.str();
}

Screen::Screen(size_t width, size_t height)
    : Surface(width, height)
{
    mBounds = {0, 0, (ssize_t)width, (ssize_t)height};
    hideCursor();
}

