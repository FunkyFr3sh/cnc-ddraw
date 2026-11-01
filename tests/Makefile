CC = i686-w64-mingw32-gcc
CFLAGS = -I../inc -O2 -Wall -Werror -std=c99 -flto -fwhole-program

.PHONY: all clean
all: input.exe

# Keep the child enumeration implementation; input.c counts renderer actions.
utils-test.o: ../src/utils.c $(wildcard ../inc/*.h)
	$(CC) $(CFLAGS) -Dutil_toggle_fullscreen=unused_toggle_fullscreen \
		-Dutil_toggle_maximize=unused_toggle_maximize \
		-Dutil_set_window_rect=unused_set_window_rect -c -o $@ $<

input.exe: input.c ../src/winapi_hooks.c ../src/keyboard.c utils-test.o $(wildcard ../inc/*.h)
	$(CC) $(CFLAGS) -o $@ input.c ../src/winapi_hooks.c ../src/keyboard.c utils-test.o -lgdi32 -lwinmm

clean:
	$(RM) input.exe utils-test.o
