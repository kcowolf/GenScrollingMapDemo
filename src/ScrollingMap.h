#ifndef SCROLLINGMAP_H
#define SCROLLINGMAP_H

#include <genesis.h>

#define MAP_TILE_START_IDX 1

#define VDP_PLANE_TILE_WIDTH 64
#define VDP_PLANE_TILE_WIDTH_MINUS_ONE 63
#define VDP_PLANE_TILE_WIDTH_TIMES_TWO 128
#define VDP_PLANE_TILE_HEIGHT 32
#define VDP_PLANE_TILE_HEIGHT_MINUS_ONE 31

void ScrollingMap_init();
void ScrollingMap_update();
void ScrollingMap_updateVDP();

// TODO -- Set this equal to the maximum allowed map height (in tiles).
// 128 tiles = 1024 pixels = 4.57 times the height of the screen.  Seems like a reasonable limit.
#define ROW_OFFSET_COUNT 128

u16 fgRowOffsets[ROW_OFFSET_COUNT];
u16 bgRowOffsets[ROW_OFFSET_COUNT];

#endif // SCROLLINGMAP_H
