libconutils 0.0
===============

libconutils is a small and simple Linux C++ library for providing
basic tools to write console graphics applications using ASCII characters.
It can be used for example for:
 * Programs that need console window oriented interface.
 * Text editors
 * Console based games
 * ...

The library target is to provide a very simple interface for quick development.
It does not depend on anything more that the standard C/C++ library.
It is NOT a substitute of ncuruses library. If you need something serious and portable
for production then you should rather stop here and use ncurses.
This is primary written for fun.

Building and installing
-----------------------
To build dynamic library (the default):

    make

To build static library:

    make static
    
Install:

    make install prefix=path (default is: /usr/local)
    
Uninstall:

    make uninstall
    
Clean:

    make clean

Include conutils/conutils.h and link with -lconutils

Documentation
-------------
* https://octals.github.io/libconutils/doc/html/index.html
* Build locally with Doxygen: `make doc`
