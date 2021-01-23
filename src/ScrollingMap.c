#include <genesis.h>
#include "graphics.h"
#include "MathUtil.h"
#include "ScrollingMap.h"

// TODO -- Background should probably wrap -- at least horizontally if not vertically.

// NOTE: While not a direct port from the original, the structure and techniques used here were inspired from
// sikthehedgehog's Dragon's Castle.
//
// https://github.com/sikthehedgehog/dragon/blob/master/src-68k/stage.68k

// NOTE: Assumes background will each only use one palette.  Sonic 2's foregrounds can use at least 2.

#define PAL_BG PAL0

#define PLANE_BG VDP_PLAN_B

// The maximum coordinates (towards the bottom right) where the camera can be without showing anything beyond the map edges.
u32 bgCameraLimitPixelX;
u32 bgCameraLimitPixelY;

// The coordinates of the camera (top-left pixel on the screen).  These will be clamped to be within the map boundaries (0,0)-(cameraLimitPixelX,cameraLimitPixelY)
u32 bgCameraPixelX;
u32 bgCameraPixelY;

// Coordinates of the tile containing bgCameraPixelX and bgCameraPixelY.
u16 bgCameraTileX;
u16 bgCameraTileY;

// Buffers used for copying map data to VRAM.
u16 bgRowBuffer1[VDP_PLANE_TILE_WIDTH];
u16 bgRowBuffer2[VDP_PLANE_TILE_WIDTH];
u16 bgColumnBuffer1[VDP_PLANE_TILE_HEIGHT];
u16 bgColumnBuffer2[VDP_PLANE_TILE_HEIGHT];

// SGDK Map structures.  w and h are width and height in tiles.
Map bgMap;

u16 bgTilesetStartIdx;

void redrawBackgroundRow(u16 rowToUpdate);
void redrawBackgroundColumn(u16 columnToUpdate);
void redrawBackgroundScreen();
void updateCamera();

void ScrollingMap_init()
{
    VDP_setPlanSize(VDP_PLANE_TILE_WIDTH, VDP_PLANE_TILE_HEIGHT);

    bgMap.w = TILEMAP_TEST_BG_TILE_WIDTH;
    bgMap.h = TILEMAP_TEST_BG_TILE_HEIGHT;
    bgMap.tilemap = (u16*) TILEMAP_TEST_BG_MAP;

    // TODO -- Initialize the camera's position based on the player's starting position.
    bgCameraPixelX = 0;
    bgCameraPixelY = 0;
    bgCameraLimitPixelX = TILE_TO_PIXEL(bgMap.w) - SCREEN_PIXEL_WIDTH;
    bgCameraLimitPixelY = TILE_TO_PIXEL(bgMap.h) - SCREEN_PIXEL_HEIGHT;

    // Load tiles
    bgTilesetStartIdx = MAP_TILE_START_IDX;

    VDP_loadTileData((const u32*) TILEMAP_TEST_BG_TILES, bgTilesetStartIdx, TILEMAP_TEST_BG_TILE_COUNT, 0);

    // Calculate row offsets so we don't need to multiply later.
    u16 rowOffset = 0;
    u16 i;
    for (i = 0; i < bgMap.h; i += 2)
    {
        bgRowOffsets[i >> 1] = rowOffset;
        rowOffset += (bgMap.w << 1);
    }

    updateCamera();
    ScrollingMap_updateVDP();
    redrawBackgroundScreen();
}

void ScrollingMap_update()
{
    u16 oldBGCameraTileX = bgCameraTileX;
    u16 oldBGCameraTileY = bgCameraTileY;

    updateCamera();

    // Background
    if (bgCameraTileX < oldBGCameraTileX)
    {
        // Moved left.
        redrawBackgroundColumn(bgCameraTileX);
    }
    else if (bgCameraTileX > oldBGCameraTileX)
    {
        // Moved right.
        redrawBackgroundColumn(bgCameraTileX + SCREEN_TILE_WIDTH);
    }

    if (bgCameraTileY < oldBGCameraTileY)
    {
        // Moved up.
        redrawBackgroundRow(bgCameraTileY);
    }
    else if (bgCameraTileY > oldBGCameraTileY)
    {
        // Moved down.
        redrawBackgroundRow(bgCameraTileY + SCREEN_TILE_HEIGHT);
    }
}

void ScrollingMap_updateVDP()
{
    // Background
    VDP_setHorizontalScroll(PLAN_B, -(bgCameraPixelX));
    VDP_setVerticalScroll(PLAN_B, (bgCameraPixelY));
}

void redrawBackgroundRow(u16 rowToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = bgMap.tilemap + bgRowOffsets[rowToUpdate >> 1] + bgCameraTileX;

    u16 rowBufferIdx = bgCameraTileX;
    u16 baseTile = TILE_ATTR_FULL(PAL_BG, 0, 0, 0, bgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_WIDTH; i != 0; i--)
    {
        rowBufferIdx &= 0x3F;  // rowBufferIdx MOD 64 (VDP_PLANE_TILE_WIDTH)
        // TODO -- Need to determine which is better -- rowBuffer[rowBufferIdx] or *(rowBuffer + rowBufferIdx).
        bgRowBuffer1[rowBufferIdx] = baseTile + *mapDataAddr;
        bgRowBuffer2[rowBufferIdx] = baseTile + *(mapDataAddr + bgMap.w);
        rowBufferIdx++;
        mapDataAddr++;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (u32) bgRowBuffer1, PLANE_BG + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
    DMA_queueDma(DMA_VRAM, (u32) bgRowBuffer2, PLANE_BG + (((((rowToUpdate + 1) & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
}

void redrawBackgroundColumn(u16 columnToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = bgMap.tilemap + bgRowOffsets[bgCameraTileY >> 1] + columnToUpdate;

    u16 columnBufferIdx = bgCameraTileY;
    u16 baseTile = TILE_ATTR_FULL(PAL_BG, 0, 0, 0, bgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
    {
        columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
        bgColumnBuffer1[columnBufferIdx] = baseTile + *mapDataAddr;
        bgColumnBuffer2[columnBufferIdx] = baseTile + *(mapDataAddr + 1);
        columnBufferIdx++;
        mapDataAddr += bgMap.w;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (u32) bgColumnBuffer1, PLANE_BG + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    DMA_queueDma(DMA_VRAM, (u32) bgColumnBuffer2, PLANE_BG + (((columnToUpdate + 1) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
}

// Redraw the whole screen.  Normally this would be done with the screen blacked out.

void redrawBackgroundScreen()
{
    u16 currentCol = SCREEN_TILE_WIDTH_PLUS_TWO;
    do
    {
        currentCol -= 2;

        // Calculate where in the tilemap the new row's tiles are located.
        const u16* mapDataAddr = bgMap.tilemap + bgRowOffsets[bgCameraTileY >> 1] + bgCameraTileX + currentCol;

        u16 columnBufferIdx = bgCameraTileY;
        u16 baseTile = TILE_ATTR_FULL(PAL_BG, 0, 0, 0, bgTilesetStartIdx);

        // Copy the tiles into the buffer.
        u16 i;
        for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
        {
            columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
            bgColumnBuffer1[columnBufferIdx] = baseTile + *mapDataAddr;
            bgColumnBuffer2[columnBufferIdx] = baseTile + *(mapDataAddr + 1);
            columnBufferIdx++;
            mapDataAddr += bgMap.w;
        }

        // Since we're redrawing the whole screen, do the DMA immediately instead of queuing it up.
        DMA_doDma(DMA_VRAM, (u32) bgColumnBuffer1, PLANE_BG + (((bgCameraTileX + currentCol) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
        DMA_doDma(DMA_VRAM, (u32) bgColumnBuffer2, PLANE_BG + (((bgCameraTileX + currentCol + 1) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    }
    while (currentCol != 0);
}

void updateCamera()
{
    // TODO -- Calculate the camera's position based on the player's current position.

    // TODO -- Make sure the camera's position doesn't go below zero (for this demo we do this in JoypadHandler).

    if (bgCameraPixelX > bgCameraLimitPixelX)
    {
        bgCameraPixelX = bgCameraLimitPixelX;
    }

    if (bgCameraPixelY > bgCameraLimitPixelY)
    {
        bgCameraPixelY = bgCameraLimitPixelY;
    }

    // bgCameraTileX only changes when the camera moves 16 pixels horizontally.  & 0xFFFE means it will always be an even number.
    bgCameraTileX = PIXEL_TO_TILE(bgCameraPixelX) & 0xFFFE;
    // bgCameraTileY only changes when the camera moves 16 pixels vertically.  & 0xFFFE means it will always be an even number.
    bgCameraTileY = PIXEL_TO_TILE(bgCameraPixelY) & 0xFFFE;
}
