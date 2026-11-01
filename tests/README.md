# Input regression checks

Build with 32-bit MinGW-w64 GCC and run on Windows or in a disposable Wine prefix:

```sh
make -C tests
wine tests/input.exe
```

The fixture links the production input hooks. LTO removes unrelated renderer
functions; the test supplies only their external state and action counters.
It creates ordinary nested Win32 windows and requires a working desktop.
It contains no game code or assets and does not change the monitor mode.

Checks cover logical child hit-testing, hidden/disabled children, mouse capture
and signed drag coordinates, message-window filters, letterboxing, repeated
`PM_NOREMOVE`, queued cursor position, and unchanged input handling in modes
0–4. Overlay checks cover sibling order, focus, hidden and nonactivating windows,
and enumeration of multiple redirected siblings. Keyboard checks
cover Alt+Enter, alternate fullscreen/maximize keys, repeats, Print Screen and
Alt state. A renderer-thread check verifies that hiding a child releases the
drawing lock without requiring the UI thread to pump messages, and that the
deferred change preserves unrelated styles and focus. It covers modes 3–5
and rejects a queued target reparented outside the game root.
A failed check produces a nonzero exit status.

For integration testing, build a clean release DLL as well as a debug DLL.
Use only that DLL as the application's `ddraw.dll`, a fresh configuration and
`fixchilds=5`. Confirm scaled child-control clicks, mouse capture, screen changes,
Alt+Enter in both directions, sequence skipping and normal exit. Exercise GDI,
OpenGL and Direct3D9, plus an unchanged existing game profile. Unit checks alone
do not establish game or renderer compatibility.

## Scaled child-window smoke test

Mode 5 is opt-in for applications that draw child UI through the GDI blits
redirected to the primary surface. It leaves the child HWNDs at logical size;
it does not implement scaling of arbitrary native controls.

For patched Star Trek Generations (German or English), use cnc-ddraw directly
as `ddraw.dll`, without the separate game shim or backend DLL. The executable
still needs its VGA bypass and two window-style changes; these are not applied
by cnc-ddraw. Register the selected installation's `Path` and `CDPath` in the
registry hive used by that executable. Under Wine, enable native `ddraw`.

Start from this `ddraw.ini` (change `renderer` to `gdi` or `direct3d9` to compare):

```ini
[ddraw]
windowed=true
width=960
height=720
renderer=opengl
maintas=true
fixchilds=5
hook_peekmessage=true
devmode=true
noactivateapp=true
minfps=-2
maxfps=60
toggle_borderless=true
toggle_upscaled=true
savesettings=0
```

`devmode` leaves the cursor unlocked. `noactivateapp` avoids the game's native
frame repaint during activation. `minfps=-2` continuously uploads the surface.
Fullscreen here means borderless, without requesting a monitor mode change.

Launch with `S`, skip the opening sequences, and open Options with Escape.
Check Settings, Alt+Enter, Cancel/Return, moving cartography, sector view and
star selection. Return to windowed mode, resize, and check Settings again.
Finally select Quit/Accept and use Space to skip the closing sequence; verify
normal process exit. Allow transitions to finish before the next input or
screenshot. A key sent during a transition can precede the skippable sequence.

Use a disposable installation/prefix and a private desktop for automation.
Confirm the requested renderer is active; a fallback is not a pass for that
renderer. Screenshots, elapsed time and forced teardown alone cannot establish
successful input, scene progression or shutdown.
