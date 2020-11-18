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

// TODO -- Set this equal to the maximum allowed map height (in tiles), divided by 2.
// If ROW_OFFSET_COUNT == 128, the maximum map height is 256 tiles (2048 pixels).
#define ROW_OFFSET_COUNT 128

u16 bgRowOffsets[ROW_OFFSET_COUNT];

#endif // SCROLLINGMAP_H
