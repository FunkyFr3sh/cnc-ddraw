-include config.mk

TARGET   ?= ddraw.dll

LDFLAGS  ?= -Wl,--enable-stdcall-fixup -s -static -shared
CFLAGS   ?= -Iinc -O2 -Wall -std=c99 -Wno-incompatible-pointer-types
LIBS      = -lgdi32 -lwinmm -lole32 -lmsimg32 -lpsapi

CC        = i686-w64-mingw32-gcc
WINDRES  ?= i686-w64-mingw32-windres

SRCS     := $(wildcard src/*.c) $(wildcard src/*/*.c) ddraw.rc
OBJS     := $(addsuffix .o, $(basename $(SRCS)))

.PHONY: clean all
all: $(TARGET)

%.o: %.rc
	$(WINDRES) -J rc $< $@ || windres -J rc $< $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ ddraw.def $(LIBS)

clean:
	$(RM) $(TARGET) $(OBJS) || del $(TARGET) $(subst /,\\,$(OBJS))
