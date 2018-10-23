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

/** @mainpage
 *
 * @section intro_sec Introduction
 *
 * libconutils is a small and simple Linux C++ library for providing
 * basic tools to write console graphics applications using ASCII characters.
 * It can be used for example for:\n
 *  * Programs that need console window oriented interface.
 *  * Text editors
 *  * Console based games
 *  * ...
 *
 * The library target is to provide a very simple interface for quick development.
 * It does not depend on anything more that the standard C/C++ library.
 * It is NOT a substitute of ncuruses library. If you need something serious and portable
 * for production then you should rather stop here and use ncurses.
 * This is primary written for fun.
 *
 * @note This library does not implement any kind of thread synchronization.
 *       If used in a multi-threaded program be sure to implement synchronization yourself.
 *
 * @section install_sec Compile and install
 *
 * To build shared library (default) run:\n
 *
 * @code
 * make
 * @endcode
 *
 * To build static library run:\n
 *
 * @code
 * make static
 * @endcode

 * To install run:\n
 *
 * @code
 * make install
 * @endcode
 *
 * Default installation prefix is /usr/local
 * You can change it by:\n
 *
 * @code
 * make install prefix=path
 * @endcode
 *
 * include conutils/conutils.h and link with -lconutils and use -std=c++11
 *
 * To uninstall run:\n
 *
 * @code
 * make uninstall
 * @endcode
 *
 * To clean run:\n
 *
 * @code
 * make clean
 * @endcode
 *
 * @section usage_sec Usage
 *
 * There are a few main objects that you will use mostly:\n
 * * @link conutils::Char Char @endlink\n
 *   This represents a single character. Characters can have foreground color,
 *   background color and attributes. Characters can also be transparent (invisible).
 *   Currently the library defaults to 256 color mode.
 * * @link conutils::Surface Surface @endlink\n
 *   A surface is basically a rectangle of @link conutils::Char characters @endlink that
 *   you write all your images, text or whatever to it.
 *   The class provides various operations like resizing, positioning, Z order etc.
 *   The surfaces can be linked together in a tree to create a hierarchy of surfaces.
 *   When you make a change on a surface of this tree it is recursively propagated to the top
 *   updating all surfaces along the way. The surface tree will calculate only the minimal region
 *   that needs to be updated to minimize data copying operations.
 *   The idea is that when you make changes to a surface you call for an update on this particular
 *   surface and not any other.
 *   For example if your changed area is the marked in green it will translate to the parent and so on:\n
 *   @image html tree.png
 *   @note When a surface is parent to other surfaces you should not make any changes to it different
 *         than movement, visibility or adding/removing child surfaces because its buffer will be used
 *         to render it's child surfaces. Make content changes only to leaf node surfaces in the tree.
 * * @link conutils::Screen Screen @endlink\n
 *   The screen is a special type of surface that renders it's contents to the terminal screen.
 *   This will be the top surface in a surface tree. The screen is a singleton class.
 * * @link conutils::Keyboard Keyboard @endlink\n
 *   This is the class that will fetch your keystrokes. Please note that some terminals
 *   send wicked escape sequences for the special keys and some may not work. I'm looking forward
 *   to minimize all of those. The keyboard is a singleton class.
 *
 * Here are some short examples to get you started:\n
 *
 * @code{.cpp}
 * #include <conutils.h>
 *
 * using namespace conutils;
 *
 * ...
 *
 * // Obtain the screen and the keyboard
 * Screen   *sc = Screen::getInstance();
 * Keyboard *kb = Keyboard::getInstance();
 * if (!sc || !kb)
 *     ... error  ...
 *
 * // Create a window with two children
 * Surface window(10, 10);
 * Surface window_bg(10, 10);
 * Surface child1(2, 2);
 * Surface child2(2, 2);
 *
 * // Solid fill the surfaces
 * window_bg.fill(Char('.', Char::green)); // Window background with green dots
 * child1.fill(Char('a', Char::yellow));   // Yellow a's
 * child2.fill(Char('b', Char::blue));     // Blue b's
 *
 * // Create the tree
 * window.addLayer(&window_bg, 0);         // Add window background with Z order 0
 * window.addLayer(&child1, {1, 1}, 1);    // Add child1 at pos 1, 1 with Z order 1
 * window.addLayer(&child2, {2, 2}, 2);    // Add child2 at pos 1, 1 with Z order 2
 * sc->addLayer(&window, {10, 10});        // Add the window to the screen at 10, 10 with Z order 0 (default)
 *
 * // Post update
 * window.render();
 *
 * // Wait for any key
 * kb->waitForKey();
 *
 * ...
 * @endcode
 *
 * For more details check individual class methods.
 */

#ifndef __CONUTILS_H__
#define __CONUTILS_H__

#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <poll.h>

#include <memory>
#include <string>
#include <map>
#include <set>

namespace conutils {

/**
 * Represents a point on a 2D plane.
 */
struct Point {
    Point(ssize_t x = 0, ssize_t y = 0)
        : x(x), y(y) { }

    inline bool operator== (const Point& rhs) const { return rhs.x == x && rhs.y == y; }
    inline bool operator!= (const Point& rhs) const { return !operator==(rhs); }

    ssize_t x;
    ssize_t y;

    /** @return Dumps this object into a string. For debugging. */
    std::string str() const;
};

/**
 * Represents a rectangle with dimensions [ top(x, y), bottom(x, y) )
 */
struct Rect {
    Rect() { }
    Rect(ssize_t tx, ssize_t ty, ssize_t bx, ssize_t by)
        : top(tx, ty), bottom(bx, by) { }
    Rect(const Point& top, const Point& bottom)
        : top(top), bottom(bottom) { }

    inline bool   operator== (const Rect& rhs) const { return rhs.top == top && rhs.bottom == bottom; }
    inline bool   operator!= (const Rect& rhs) const { return !operator==(rhs); }

    /** @return Rectangle width */
    inline size_t width() const { return bottom.x - top.x; }
    /** @return Rectangle height */
    inline size_t height() const { return bottom.y - top.y; }
    /** @return Rectangle size (area) */
    inline size_t size() const { return width() * height(); }

    /** @return The linear coordinate of x and y in this rectangle. */
    inline size_t index_for(const Point& point) const { return (point.y - top.y) * width() + (point.x - top.x); }

    /** @return The corresponding point for index without checking for bounds. */
    inline Point  point_for(size_t index) const { return Point(top.x + (index % width()), top.y + (index / width())); }

    /** @return true if this rectangle can exist. */
    inline bool   valid() const { return bottom.x > top.x && bottom.y > top.y; };

    /**
     * Moves this rectangle to a new position pos
     *
     * @return This rectangle after moving it.
     */
    Rect&         move(const Point& pos);

    /** @return Dumps this object into a string. For debugging. */
    std::string   str() const;

    /** @return The intersected rectangle of r1 and r2 if any. */
    static const Rect intersect(const Rect& r1, const Rect& r2);
    /** @return The bounding rectangle of r1 and r2. */
    static const Rect boundingRect(const Rect& r1, const Rect& r2);

    Point top;
    Point bottom;
};

/**
 * Represents an ASCII character with colors and attributes.
 */
struct Char {
    /** Characters attributes */
    enum {
        none        = 0x00,
        bold        = 0x01,
        underscore  = 0x02,
        blink       = 0x04,
        reverse     = 0x08,
        transparent = 0x80,
    };

    /** Standart first 8 colors */
    enum {
        black,
        red,
        green,
        yellow,
        blue,
        magenta,
        cyan,
        white,
    };

    Char(char val = ' ', uint8_t fg = white, uint8_t bg = black, uint8_t attr = none)
        : val(val), fg(fg), bg(bg), attr(attr) { }

    inline bool operator== (const Char& rhs) const { return rhs.val == val && rhs.fg == fg && rhs.bg == bg && rhs.attr == attr; }
    inline bool operator!= (const Char& rhs) const { return !operator==(rhs); }

    /** Character value. */
    char val;
    /** Foreground color. */
    uint8_t fg;
    /** Background color. */
    uint8_t bg;
    /** OR'ed values of character attributes. */
    uint8_t attr;
};

/**
 * Represents a drawable area. Surfaces can be linked together in a tree.
 * A change in a leaf surface of this tree will propagate to the top.
 * The class provides only basic Surface manipulation. Any thread synchronization
 * must be implemented manually.
 */
class Surface {
public:
    Surface() { }
    Surface(size_t width, size_t height);

    /**
     * @warning Caller must remove this surface from it's parrent (if any)
     *          manually before it is destructed.
     */
    virtual ~Surface() { }

    /** @return Surface width. */
    inline size_t         width()     const { return mBounds.width(); }
    /** @return Surface height. */
    inline size_t         height()    const { return mBounds.height(); }
    /** @return Surface size (area). */
    inline size_t         size()      const { return mBounds.size(); }
    /** @return Surface position in the context it exists in. */
    inline const Point&   pos()       const { return mPos; }
    /** @return Surface visibility. */
    inline bool           visible()   const { return mVisible; }
    /** @return The unclipped surface bounds in the context it exists in. */
    inline const Rect     bounds()    const { return Rect(mBounds).move(mPos); }
    /** @return The surface's parent (if any) */
    inline const Surface *parent()    const { return mParent; }

    /**
     * @return Get access to surface's data buffer.
     *         Iterate this buffer in the range [0, size()).
     *         When writing make sure to invalidate() the modified region
     *         so your modifications will be taken into account.
     */
    inline Char*          data()            { return mData.get(); }

    /**
     * Resize this surface with new width and height.
     * @return 0 on success, < 0 on error.
     */
    int                   resize(size_t width, size_t height);

    /**
     * Clears the surface.
     *
     * @param crop : If valid only crop will be cleared.
     *               Clears entire surface otherwise.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   clear(const Rect& crop = Rect());

    /**
     * Fill the surface with a pattern.
     *
     * @param pattern : Pattern Char to fill with.
     * @param crop    : If valid only crop will be filled.
     *                  Fills entire surface otherwise.
     */
    int                   fill(const Char& pattern, const Rect& crop = Rect());

    /**
     * Blends another surface onto this one. The other surface is clipped accordingly.
     * Transparent characters are not copied.
     *
     * @param other    : The surface to blend in.
     * @param src_crop : If valid only src_crop from other will be blended, entire surface otherwise.
     * @param pos      : Where to position the blended surface into this one.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   blend(const Surface& other, const Rect& src_crop, const Point& pos);

    /**
     * Moves this surface to a new pos
     *
     * @param pos : New position in the context this surface exists in.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   move(const Point& pos);

    /**
     * Moves this surface to a new pos and new Z
     *
     * @param pos : New position in the context this surface exists in.
     * @param Z   : New Z position in the context this surface exists in.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   move(const Point& pos, int Z);

    /**
     * Moves this surface to a new Z
     *
     * @param Z   : New Z position in the context this surface exists in.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   moveZ(int Z);

    /** Makes this surface visible. */
    void                  show();

    /** Makes this surface not visible. */
    void                  hide();

    /**
     * Makes another surface as a layer of this one.
     *
     * @param sf : The surface to add.
     * @param Z  : Optional Z position. Default 0.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   addLayer(Surface *sf, int Z = 0);

    /**
     * Makes another surface as a layer of this one.
     *
     * @param sf  : The surface to add.
     * @param pos : Position to place added layer.
     * @param Z   : Z position.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   addLayer(Surface *sf, const Point& pos, int Z = 0);

    /**
     * Removes a previously attached surface from this one.
     *
     * @param sf : The surface to remove.
     *
     * @return 0 on success, < 0 on error.
     */
    int                   removeLayer(Surface *sf);

    /**
     * Moves an already attached surface into this one to another Z
     *
     * @param sf : The surface to move
     * @param Z  : The new Z
     *
     * @return 0 on success, < 0 on error.
     */
    int                   moveLayer(Surface *sf, int Z);

    /**
     * Checks if a surface exists as a layer into this one.
     *
     * @param sf : The surface to move
     *
     * @return true or false respectively.
     */
    bool                  containsLayer(Surface *sf);

    /**
     * Marks the entire surface dirty (modified)
     *
     * @return The dirty region. In this case the entire surface bounds.
     */
    const Rect&           invalidate();

    /**
     * Marks a region of the surface as dirty (modified)
     *
     * @param bounds : The region to be marked as dirty.
     *
     * @return The new dirty region of the surface.
     */
    const Rect&           invalidate(const Rect& bounds);

    /**
     * Marks a region of the surface as dirty (modified)
     * between start and end index exclusive of the surface Char buffer.
     *
     * @param start : Start buffer index
     * @param end   : End buffer index
     *
     * @return The new dirty region of the surface.
     */
    const Rect&           invalidate(const size_t start, const size_t end);

    /**
     * Render all layers (if any) onto this surface and recursively repeat
     * this process for the parent (if any).
     * Users can overload renderDone() which will be called when done
     * to further process the contents of the surface. This is usually
     * done by the top surface of the tree.
     */
    void                  render();

    /** @return Dumps this object into a string. For debugging. */
    std::string           str(std::string ident = "") const;

protected:
    /**
     * Called when render() of this surface is complete.
     *
     * @warning Please do minimal processing here to not stall further rendering.
     *          Also avoid calling any surface modifications methods.
     *
     * @param dirty : The region that was updated in this surface.
     */
    virtual void          renderDone(const Rect& dirty) { }

private:
    Rect mBounds;
    Rect mDirty;
    Point mPos;
    bool mVisible = true;
    Surface *mParent = nullptr;
    std::unique_ptr<Char> mData = nullptr;
    std::map<int /*Z*/, std::set<Surface *>> mLayerMap;
};

/**
 * Singleton class representing the terminal screen.
 * It is responsible for displaying actual characters on the screen.
 */
class Screen : private Surface {
public:
    ~Screen();

    /** @return Pointer to the screen instance. NULL if something went wrong. */
    static Screen *getInstance();

    /** @return Screen width */
    inline size_t  width() const { return Surface::width(); }
    /** @return Screen height */
    inline size_t  height() const { return Surface::height(); }

    /** Clears and invalidate() the screen. */
    void           clear();

    /**
     * Resize the screen.
     * New dimensions will be queried from the terminal by this call.
     *
     * @return 0 on success, < 0 on error.
     */
    int            resize();

    /**
     * Waits indefinitely for SIGWINCH signal. (Window size changed)
     *
     * @param new_bounds : Will be filled with the new bounds on return.
     *
     * @return 0 on success, < 0 on error.
     */
    int            wait_sigwinch(Rect& new_bounds);

    /**
     * Makes another surface as a layer of this one.
     *
     * @param sf : The surface to add.
     * @param Z  : Optional Z position. Default 0.
     *
     * @return 0 on success, < 0 on error.
     */
    inline int     addLayer(Surface *sf, int Z = 0) { return Surface::addLayer(sf, Z); }

    /**
     * Makes another surface as a layer of this one.
     *
     * @param sf  : The surface to add.
     * @param pos : Position to place added layer.
     * @param Z   : Z position.
     *
     * @return 0 on success, < 0 on error.
     */
    inline int     addLayer(Surface *sf, const Point& pos, int Z = 0) { return Surface::addLayer(sf, pos, Z); };

    /**
     * Removes a previously attached surface from this one.
     *
     * @param sf : The surface to remove.
     *
     * @return 0 on success, < 0 on error.
     */
    inline int     removeLayer(Surface *sf) { return Surface::removeLayer(sf); }

    /**
     * Moves an already attached surface into this one to another Z
     *
     * @param sf : The surface to move
     * @param Z  : The new Z
     *
     * @return 0 on success, < 0 on error.
     */
    inline int     moveLayer(Surface *sf, int Z) { return Surface::moveLayer(sf, Z); }

    /**
     * Checks if a surface exists as a layer into this one.
     *
     * @param sf : The surface to move
     *
     * @return true or false respectively.
     */
    inline bool    containsLayer(Surface *sf) { return Surface::containsLayer(sf); }

    /** Shows the cursor. */
    void           showCursor();

    /** Hides the cursor. */
    void           hideCursor();

    /**
     * Sets new cursor position
     * @note Terminal cursors position starts from 1, 1
     */
    void           setCursorPos(const Point& pos);

    /** @return Visibility of the cursor. */
    inline bool    cursorVisible() const { return mCursorVisible; }

private:
    Screen(size_t width, size_t height);

    int init();
    void drawChar(const Char& ch);
    void renderDone(const Rect& dirty);

    Rect mBounds;
    Char mCurrentAttr;
    bool mCursorVisible = true;
    int mWinchFd = -1;
};

/** Singleton class representing the keyboard. */
class Keyboard
{
public:
    /** Special keys */
    enum {
        KEY_UNKNOWN        = 0,
        KEY_TAB            = 9,
        KEY_CR             = 10,
        KEY_ESC            = 27,
        KEY_BS             = 127,
        KEY_F1             = 10000,
        KEY_F2             = 10001,
        KEY_F3             = 10002,
        KEY_F4             = 10003,
        KEY_F5             = 10004,
        KEY_F6             = 10005,
        KEY_F7             = 10006,
        KEY_F8             = 10007,
        KEY_F9             = 10008,
        KEY_F10            = 10009,
        KEY_F11            = 10010,
        KEY_F12            = 10011,
        KEY_INS            = 10012,
        KEY_DEL            = 10013,
        KEY_HOME           = 10014,
        KEY_END            = 10015,
        KEY_PGUP           = 10016,
        KEY_PGDOWN         = 10017,
        KEY_UP             = 10018,
        KEY_DOWN           = 10019,
        KEY_LEFT           = 10020,
        KEY_RIGHT          = 10021,
        KEY_ESC_SEQ        = 10022,
    };

    /** Modifications keys */
    enum {
        MOD_META           = 1000,
        MOD_SHIFT          = 2000,
        MOD_ALT            = 3000,
        MOD_SHIFT_ALT      = 4000,
        MOD_CTRL           = 5000,
        MOD_SHIFT_CTRL     = 6000,
        MOD_ALT_CTRL       = 7000,
        MOD_SHIFT_ALT_CTRL = 8000,
    };

    ~Keyboard();

    /** @return Pointer to the keyboard instance. NULL if something went wrong. */
    static Keyboard *getInstance();

    /**
     * Waits for a key to be pressed.
     *
     * @param timeout_ms : Timeout in milliseconds. -1 == wait indefinitely.
     *
     * @return One of the following:\n
     *         * A valid ASCII printable key\n
     *         * One of the special keys. If alt, ctrl or shift is in
     *           combination it will be arithmetically added to the returned key.
     *           For example MOD_ALT + KEY_UP\n
     *         * -ETIMEDOUT if a time out was specified. Other negative codes are errors.
     */
    int              waitForKey(int timeout_ms = -1);

private:
    int pollOnce(int timeout_ms = -1);
    int parseEsc();

    struct termios mOldTermios;
    struct termios mNewTermios;
    struct pollfd mFds;
};

} /* namespace conutils */

#endif /* __CONUTILS_H__ */
