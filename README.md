# GBA Pong

A from-scratch Pong game for the Game Boy Advance, written in C.

## Build
Compiled with devkitPro (devkitARM + libgba). Run `make` to produce the `.gba` ROM.

## Run
Load the ROM in any GBA emulator (e.g. mGBA).

## Rendering
Mode 3 bitmap: pixel colours are written directly to VRAM. No tiles or sprites.

## Frame Updates
The game loop is synced to VBlank. Each frame, only the changed rectangles are erased and redrawn.

## Bounce Physics
The bounce angle comes from the offset between the ball's centre and the paddle's centre.

## Anti-Tunnelling
Per-frame movement stays below the paddle width, so the ball can never skip past a paddle.

## Controls
D-pad Up/Down moves the paddle. START restarts after a win.