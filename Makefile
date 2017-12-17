ifndef debug
    OPTFLAGS = -O3
else
    OPTFLAGS = -g3
endif

ifndef cpp
    XFLAGS = -x c++
	STD = c++11
else
	STD = c99
endif

CFLAGS  = $(OPTFLAGS) -std=$(STD) -Wall -Werror -fPIC
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=lib/%.o)

CC      = clang
PREFIX  = /usr
CFLAGS += -I$(PREFIX)/include
INCLUDES = -I./src

all: libdir lib/libobj.so

.PHONY: test
test:
	$(CC) -o obj-test $(CFLAGS) $(XFLAGS) $(INCLUDES) $(wildcard test/src/*.c) -lobj -lm

.PHONY: libdir
libdir:
	mkdir -p lib

$(OBJECTS): $(@F:%.o=src/%.c)
	$(CC) $(CFLAGS) $(XFLAGS) $(INCLUDES) -c -o $@ $(@F:%.o=src/%.c)

lib/libobj.so: $(OBJECTS)
	$(CC) -shared -o $@ $(OBJECTS) $(LIBS)

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/include/libobj
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp src/*.h $(DESTDIR)$(PREFIX)/include/libobj
	cp lib/libobj.so $(DESTDIR)$(PREFIX)/lib

.PHONY: clean
clean:
	rm -rf obj-test lib
