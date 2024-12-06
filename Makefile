-include config.mk

TARGET   ?= ddraw.dll

LDFLAGS  ?= -Wl,--enable-stdcall-fixup -s -static -shared
CFLAGS   ?= -Iinc -O2 -Wall -std=c99 -Wno-incompatible-pointer-types
LIBS      = -lgdi32 -lwinmm -lole32 -lMsimg32 -lPsapi

COMMIT   := $(shell git describe --match=NeVeRmAtCh --always --dirty || echo UNKNOWN)
BRANCH   := $(shell git rev-parse --abbrev-ref HEAD || echo UNKNOWN)

ECHOTEST := $(shell echo \"\")
ifeq ($(ECHOTEST),\"\")
	# Windows
	HASH     := \#
	ECOMMIT  := $(shell echo $(HASH)define GIT_COMMIT "$(COMMIT)" > inc/git.h)
	EBRANCH  := $(shell echo $(HASH)define GIT_BRANCH "$(BRANCH)" >> inc/git.h)
else
	# Either *nix or Windows with BusyBox (e.g. w64devkit)
	ECOMMIT  := $(shell echo "#define GIT_COMMIT" \"$(COMMIT)\" > inc/git.h)
	EBRANCH  := $(shell echo "#define GIT_BRANCH" \"$(BRANCH)\" >> inc/git.h)
endif

ifdef DEBUG
	CFLAGS   += -D _DEBUG -D _DEBUG_X
endif

ifdef _WIN32_WINNT
	CFLAGS   += -march=i486 -D _WIN32_WINNT=$(_WIN32_WINNT)
endif

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
