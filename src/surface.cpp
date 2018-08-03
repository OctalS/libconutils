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

Surface::Surface(size_t width, size_t height)
{
    resize(width, height);
}

const Rect& Surface::invalidate()
{
    mDirty = mBounds;
    return mDirty;
}

const Rect& Surface::invalidate(const Rect& bounds)
{
    /* Accumulate dirty. */
    if (mDirty.valid())
        mDirty = Rect::boundingRect(mDirty, Rect::intersect(mBounds, bounds));
    else
        mDirty = Rect::intersect(mBounds, bounds);

    return mDirty;
}

const Rect& Surface::invalidate(const size_t start, const size_t end)
{
    Rect dirty;
    Point start_p = mBounds.point_for(start);
    Point end_p = mBounds.point_for(end - 1); /* Make it [start, end) */

    /* dirty is always entire line wide. */
    if (start_p.y == end_p.y)
        dirty = Rect(start_p.x, start_p.y, end_p.x + 1, end_p.y + 1);
    else
        dirty = Rect(0, start_p.y, mBounds.width(), end_p.y + 1);

    return invalidate(dirty);
}

int Surface::resize(size_t width, size_t height)
{
    mData = unique_ptr<Char>(new Char[width * height]);
    if (!mData.get())
        return -ENOMEM;

    /* If we are attached to another surface we must invalidate the parent dirty region with our last position. */
    if (mParent)
        mParent->invalidate(bounds());

    mBounds = {0, 0, (ssize_t)width, (ssize_t)height};
    clear();
    return 0;
}

int Surface::fill(const Char& pattern, const Rect& crop)
{
    Rect dirty = mBounds;

    if (crop.valid()) {
        dirty = Rect::intersect(mBounds, crop);
        if (!dirty.valid())
            return -EINVAL;

        size_t w = dirty.width();
        size_t h = dirty.height();

        for (size_t y = 0; y < h; y++) {
            for (size_t x = 0; x < w; x++) {
                Point p(dirty.top.x + x, dirty.top.y + y);
                Char *data = mData.get() + mBounds.offset(p);
                *data = pattern;
            }
        }

    } else {
        size_t sz = mBounds.size();
        Char *data = mData.get();
        while (sz--)
            data[sz] = pattern;
    }

    invalidate(dirty);
    return 0;
}

int Surface::clear(const Rect& crop)
{
    return fill(Char(' '), crop);
}

int Surface::blend(const Surface& other, const Rect& src_crop, const Point& pos)
{
    Rect s_crop = Rect::intersect(other.mBounds, src_crop);
    Rect d_crop = Rect::intersect(mBounds, Rect(s_crop).move(pos));

    if (!s_crop.valid() || !d_crop.valid())
        return -EINVAL;

    const Char *src = other.mData.get();
    Char *dst = mData.get();
    size_t dst_w = d_crop.width();
    size_t dst_h = d_crop.height();

    for (size_t y = 0; y < dst_h; y++) {
        for (size_t x = 0; x < dst_w; x++) {
            Point src_p(s_crop.top.x + x, s_crop.top.y + y);
            Point dst_p(d_crop.top.x + x, d_crop.top.y + y);
            const Char *src_ch = src + other.mBounds.offset(src_p);
            Char *dst_ch = dst + mBounds.offset(dst_p);

            if (!(src_ch->attr & Char::transparent))
                *dst_ch = *src_ch;
        }
    }

    /* The dirty region is the calculated destination crop. */
    invalidate(d_crop);
    return 0;
}

int Surface::addLayer(Surface *sf, int Z)
{
    /* Surface already added else where. */
    if (sf->mParent)
        return -EINVAL;

    auto lm_iter = mLayerMap.find(Z);

    if (lm_iter == mLayerMap.end())
        mLayerMap[Z].insert(sf);
    else
        lm_iter->second.insert(sf);

    /* Invalidate the position of the newly added layer. */
    invalidate(sf->bounds());
    sf->mParent = this;

    return 0;
}

int Surface::addLayer(Surface *sf, const Point& pos, int Z)
{
    /* Surface already added else where. */
    if (sf->mParent)
        return -EINVAL;

    auto lm_iter = mLayerMap.find(Z);

    /* Position it. */
    sf->mPos = pos;

    if (lm_iter == mLayerMap.end())
        mLayerMap[Z].insert(sf);
    else
        lm_iter->second.insert(sf);

    /* Invalidate the position of the newly added layer. */
    invalidate(sf->bounds());
    sf->mParent = this;

    return 0;
}

int Surface::removeLayer(Surface *sf)
{
    /* The requested surface is not a child of this one. */
    if (sf->mParent != this)
        return -EINVAL;

    for (auto& lm_iter : mLayerMap) {
        int Z = lm_iter.first;
        set<Surface *>& layer = lm_iter.second;

        if (layer.erase(sf)) {
            /* If this was the last layer in this Z remove the map entry also. */
            if (layer.empty())
                mLayerMap.erase(Z);

            /* Invalidate the position of the removed layer. */
            invalidate(sf->bounds());
            sf->mParent = nullptr;
            return 0;
        }
    }

    return -EINVAL;
}

int Surface::moveLayer(Surface *sf, int Z)
{
    /* The requested surface is not a child of this one. */
    if (sf->mParent != this)
        return -EINVAL;

    for (auto& lm_iter : mLayerMap) {
        int old_z = lm_iter.first;
        set<Surface *>& layer = lm_iter.second;

        if (layer.erase(sf)) {
            /* If this was the last layer in this Z remove the map entry also. */
            if (layer.empty())
                mLayerMap.erase(old_z);

            auto lm_new_iter = mLayerMap.find(Z);

            if (lm_new_iter == mLayerMap.end())
                mLayerMap[Z].insert(sf);
            else
                lm_new_iter->second.insert(sf);

            /* Invalidate the position of the modified layer. */
            invalidate(sf->bounds());
            return 0;
        }
    }

    return -EINVAL;
}

bool Surface::containsLayer(Surface *sf)
{
    for (auto& lm_iter : mLayerMap) {
        set<Surface *>& layer = lm_iter.second;
        for (Surface *s : layer) {
            if (s == sf)
                return true;
        }
    }

    return false;
}

void Surface::show()
{
    invalidate();
    mVisible = true;
}

void Surface::hide()
{
    /* If we are attached to another surface we must invalidate the parent dirty region. */
    if (mParent) {
        invalidate();
        mParent->invalidate(bounds());
    }

    mVisible = false;
}

int Surface::move(const Point& pos)
{
    /* If we are attached to another surface we must invalidate the parent dirty region with our last position. */
    if (mParent) {
        invalidate();
        mParent->invalidate(bounds());
    }

    mPos = pos;
    return 0;
}

int Surface::move(const Point& pos, int Z)
{
    /* If we are attached to another surface we must invalidate the parent dirty region with our last position. */
    if (mParent) {
        invalidate();
        mParent->invalidate(bounds());
        mParent->moveLayer(this, Z);
    }

    mPos = pos;
    return 0;
}

int Surface::moveZ(int Z)
{
    if (mParent) {
        invalidate();
        return mParent->moveLayer(this, Z);
    }

    return -ENOLINK;
}

void Surface::render()
{
    /* First get the combined dirty region from all layers. */
    for (auto& lm_iter : mLayerMap) {
        set<Surface *>& layer = lm_iter.second;
        for (Surface *sf : layer) {
            if (sf->mVisible && sf->mDirty.valid()) {
                Rect sf_dirty = sf->mDirty;

                sf_dirty.top.x += sf->mPos.x;
                sf_dirty.top.y += sf->mPos.y;
                sf_dirty.bottom.x += sf->mPos.x;
                sf_dirty.bottom.y += sf->mPos.y;

                invalidate(sf_dirty);
            }
        }
    }

    /* Nothing to update here. */
    if (!mDirty.valid())
        return;

    /* If we are rendering other layers onto this surface make sure to clear the dirty region first. */
    if (!mLayerMap.empty())
        clear(mDirty);

    /* Now blend them onto this surface. */
    for (auto& lm_iter : mLayerMap) {
        set<Surface *>& layer = lm_iter.second;
        for (Surface *sf : layer) {
            if (sf->mVisible && sf->mBounds.valid()) {
                Rect src_crop = Rect::intersect(mDirty, sf->bounds());
                Point pos = src_crop.top;

                src_crop.top.x -= sf->mPos.x;
                src_crop.top.y -= sf->mPos.y;
                src_crop.bottom.x -= sf->mPos.x;
                src_crop.bottom.y -= sf->mPos.y;

                blend(*sf, src_crop, pos);

                /* We are done with sf, mark it clean. */
                sf->mDirty = Rect();
            }
        }
    }

    /* Continue upwards in the hierarchy if any. */
    if (mParent)
        mParent->render();

    renderDone(mDirty);

    /* We are done rendering, mark it clean. */
    mDirty = Rect();
}

string Surface::str(string ident) const
{
    stringstream ss;

    ss << ident << "Surface: " << this << " bounds: " << bounds().str() << " dirty: " << mDirty.str() << " visible: " << mVisible << "\n";
    if (!mLayerMap.empty()) {
        for (auto& lm_iter : mLayerMap) {
            ss << ident << "Z = " << lm_iter.first << ":\n";
            const set<Surface *>& layer = lm_iter.second;
            for (Surface *sf : layer)
                ss << sf->str(ident + "  ");
        }
    }

    return ss.str();
}

