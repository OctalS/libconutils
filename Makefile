src    := screen.cpp   \
	  keyboard.cpp \
          surface.cpp  \
          geometry.cpp

inc    := conutils.h

flags  := -std=c++11 -I. -O2 -Wall -Werror
out    := libconutils
prefix ?= /usr/local

obj    := $(src:%.cpp=%.o)

.PHONY: shared static doc clean

shared: flags += -fPIC
shared: $(out).so

static: $(out).a

doc:
	cd doc && doxygen Doxyfile

install: $(out).so
	install -m 0644 $(out).so $(prefix)/lib
	install -m 0644 $(inc) $(prefix)/include

uninstall:
	rm -f $(prefix)/lib/$(out).so
	rm -f $(prefix)/include/conutils.h

clean:
	rm -f $(obj) $(out).*

$(out).a: $(obj)
	ar cr $@ $^

$(out).so: $(obj)
	g++ $(flags) -shared -o $@ $^

%.o: %.cpp $(inc)
	g++ $(flags) -c $<
