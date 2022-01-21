#include <genesis.h>
#include "graphics.h"
#include "MathUtil.h"
#include "ScrollingMap.h"

// TODO -- Background should probably wrap -- at least horizontally if not vertically.

// NOTE: While not a direct port from the original, the structure and techniques used here were inspired from
// sikthehedgehog's Dragon's Castle.
//
// https://github.com/sikthehedgehog/dragon/blob/master/src-68k/stage.68k

// NOTE: Width of background map must be ((width of foreground map / 2) + 160).
// NOTE: Height of background map must be ((height of foreground map / 2) + 112).
// NOTE: Assumes background will each only use one palette.  Sonic 2's foregrounds can use at least 2.

#define PLANE_FG VDP_BG_A
#define PLANE_BG VDP_BG_B

// The maximum coordinates (towards the bottom right) where the camera can be without showing anything beyond the map edges.
u32 fgCameraLimitPixelX;
u32 fgCameraLimitPixelY;

// The coordinates of the camera (top-left pixel on the screen).  These will be clamped to be within the map boundaries (0,0)-(cameraLimitPixelX,cameraLimitPixelY)
u32 fgCameraPixelX;
u32 fgCameraPixelY;

// Coordinates of the tile containing fgCameraPixelX and fgCameraPixelY.
u16 fgCameraTileX;
u16 fgCameraTileY;

// Coordinates of the tile at the top left of the background.  Background scrolls at half the rate of the foreground.
u16 bgCameraTileX;
u16 bgCameraTileY;

// Buffers used for copying map data to VRAM.
u16 fgRowBuffer[VDP_PLANE_TILE_WIDTH];
u16 fgColumnBuffer[VDP_PLANE_TILE_HEIGHT];
u16 bgRowBuffer[VDP_PLANE_TILE_WIDTH];
u16 bgColumnBuffer[VDP_PLANE_TILE_HEIGHT];

u16 fgMapTileWidth;
u16 fgMapTileHeight;
const u16* fgMapTilemap;

u16 bgMapTileWidth;
u16 bgMapTileHeight;
const u16* bgMapTilemap;


u16 fgTilesetStartIdx;
u16 bgTilesetStartIdx;

void redrawForegroundRow(u16 rowToUpdate);
void redrawBackgroundRow(u16 rowToUpdate);
void redrawForegroundColumn(u16 columnToUpdate);
void redrawBackgroundColumn(u16 columnToUpdate);
void redrawForegroundScreen();
void redrawBackgroundScreen();
void updateCamera();

void ScrollingMap_init()
{
    VDP_setPlanSize(VDP_PLANE_TILE_WIDTH, VDP_PLANE_TILE_HEIGHT);

    fgMapTileWidth = TILEMAP_FG_TILE_WIDTH;
    fgMapTileHeight = TILEMAP_FG_TILE_HEIGHT;
    fgMapTilemap = (u16*) TILEMAP_FG;

    bgMapTileWidth = TILEMAP_BG_TILE_WIDTH;
    bgMapTileHeight = TILEMAP_BG_TILE_HEIGHT;
    bgMapTilemap = (u16*) TILEMAP_BG;

    // TODO -- Initialize the camera's position based on the player's starting position.
    fgCameraPixelX = 0;
    fgCameraPixelY = 0;
    fgCameraLimitPixelX = TILE_TO_PIXEL(fgMapTileWidth) - SCREEN_PIXEL_WIDTH;
    fgCameraLimitPixelY = TILE_TO_PIXEL(fgMapTileHeight) - SCREEN_PIXEL_HEIGHT;

    // Load tiles
    bgTilesetStartIdx = MAP_TILE_START_IDX;
    fgTilesetStartIdx = MAP_TILE_START_IDX + TILESET_BG_TILE_COUNT;

    VDP_loadTileData((const u32*) TILESET_BG, bgTilesetStartIdx, TILESET_BG_TILE_COUNT, 0);
    VDP_loadTileData((const u32*) TILESET_FG, fgTilesetStartIdx, TILESET_FG_TILE_COUNT, 0);

    // Calculate row offsets so we don't need to multiply later.
    u16 rowOffset = 0;
    u16 i;
    for (i = 0; i < fgMapTileHeight; i++)
    {
        fgRowOffsets[i] = rowOffset;
        rowOffset += fgMapTileWidth;
    }

    rowOffset = 0;
    for (i = 0; i < bgMapTileHeight; i++)
    {
        bgRowOffsets[i] = rowOffset;
        rowOffset += bgMapTileWidth;
    }

    updateCamera();
    ScrollingMap_updateVDP();
    redrawForegroundScreen();
    redrawBackgroundScreen();
}

void ScrollingMap_update()
{
    u16 oldFGCameraTileX = fgCameraTileX;
    u16 oldFGCameraTileY = fgCameraTileY;
    u16 oldBGCameraTileX = bgCameraTileX;
    u16 oldBGCameraTileY = bgCameraTileY;

    updateCamera();

    // Foreground
    if (fgCameraTileX < oldFGCameraTileX)
    {
        // Moved left.
        redrawForegroundColumn(fgCameraTileX);
    }
    else if (fgCameraTileX > oldFGCameraTileX)
    {
        // Moved right.
        redrawForegroundColumn(fgCameraTileX + SCREEN_TILE_WIDTH);
    }

    if (fgCameraTileY < oldFGCameraTileY)
    {
        // Moved up.
        redrawForegroundRow(fgCameraTileY);
    }
    else if (fgCameraTileY > oldFGCameraTileY)
    {
        // Moved down.
        redrawForegroundRow(fgCameraTileY + SCREEN_TILE_HEIGHT);
    }

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
    // Foreground
    VDP_setHorizontalScroll(BG_A, -fgCameraPixelX);
    VDP_setVerticalScroll(BG_A, fgCameraPixelY);

    // Background, scrolls at half the rate.
    VDP_setHorizontalScroll(BG_B, -(fgCameraPixelX >> 1));
    VDP_setVerticalScroll(BG_B, (fgCameraPixelY >> 1));
}

void redrawForegroundRow(u16 rowToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = fgMapTilemap + fgRowOffsets[rowToUpdate] + fgCameraTileX;

    u16 rowBufferIdx = fgCameraTileX;
    u16 baseTile = TILE_ATTR_FULL(PAL1, 0, 0, 0, fgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_WIDTH; i != 0; i--)
    {
        rowBufferIdx &= 0x3F;  // rowBufferIdx MOD 64 (VDP_PLANE_TILE_WIDTH)
        // TODO -- Need to determine which is better -- rowBuffer[rowBufferIdx] or *(rowBuffer + rowBufferIdx).
        fgRowBuffer[rowBufferIdx] = baseTile + *mapDataAddr;
        rowBufferIdx++;
        mapDataAddr++;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (void*) fgRowBuffer, PLANE_FG + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
}

void redrawBackgroundRow(u16 rowToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = bgMapTilemap + bgRowOffsets[rowToUpdate] + bgCameraTileX;

    u16 rowBufferIdx = bgCameraTileX;
    u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, bgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_WIDTH; i != 0; i--)
    {
        rowBufferIdx &= 0x3F;  // rowBufferIdx MOD 64 (VDP_PLANE_TILE_WIDTH)
        // TODO -- Need to determine which is better -- rowBuffer[rowBufferIdx] or *(rowBuffer + rowBufferIdx).
        bgRowBuffer[rowBufferIdx] = baseTile + *mapDataAddr;
        rowBufferIdx++;
        mapDataAddr++;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (void*) bgRowBuffer, PLANE_BG + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
}

void redrawForegroundColumn(u16 columnToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = fgMapTilemap + fgRowOffsets[fgCameraTileY] + columnToUpdate;

    u16 columnBufferIdx = fgCameraTileY;
    u16 baseTile = TILE_ATTR_FULL(PAL1, 0, 0, 0, fgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
    {
        columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
        fgColumnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
        columnBufferIdx++;
        mapDataAddr += fgMapTileWidth;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (void*) fgColumnBuffer, PLANE_FG + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
}

void redrawBackgroundColumn(u16 columnToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = bgMapTilemap + bgRowOffsets[bgCameraTileY] + columnToUpdate;

    u16 columnBufferIdx = bgCameraTileY;
    u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, bgTilesetStartIdx);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
    {
        columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
        bgColumnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
        columnBufferIdx++;
        mapDataAddr += bgMapTileWidth;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (void*) bgColumnBuffer, PLANE_BG + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
}

// Redraw the whole screen.  Normally this would be done with the screen blacked out.
void redrawForegroundScreen()
{
    u16 currentCol = SCREEN_TILE_WIDTH_PLUS_ONE;
    do
    {
        currentCol--;

        // Calculate where in the tilemap the new row's tiles are located.
        const u16* mapDataAddr = fgMapTilemap + fgRowOffsets[fgCameraTileY] + fgCameraTileX + currentCol;

        u16 columnBufferIdx = fgCameraTileY;
        u16 baseTile = TILE_ATTR_FULL(PAL1, 0, 0, 0, fgTilesetStartIdx);

        // Copy the tiles into the buffer.
        u16 i;
        for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
        {
            columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
            fgColumnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
            columnBufferIdx++;
            mapDataAddr += fgMapTileWidth;
        }

        // Since we're redrawing the whole screen, do the DMA immediately instead of queuing it up.
        DMA_doDma(DMA_VRAM, (void*) fgColumnBuffer, PLANE_FG + (((fgCameraTileX + currentCol) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    }
    while (currentCol != 0);
}

void redrawBackgroundScreen()
{
    u16 currentCol = SCREEN_TILE_WIDTH_PLUS_ONE;
    do
    {
        currentCol--;

        // Calculate where in the tilemap the new row's tiles are located.
        const u16* mapDataAddr = bgMapTilemap + bgRowOffsets[bgCameraTileY] + bgCameraTileX + currentCol;

        u16 columnBufferIdx = bgCameraTileY;
        u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, bgTilesetStartIdx);

        // Copy the tiles into the buffer.
        u16 i;
        for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
        {
            columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
            bgColumnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
            columnBufferIdx++;
            mapDataAddr += bgMapTileWidth;
        }

        // Since we're redrawing the whole screen, do the DMA immediately instead of queuing it up.
        DMA_doDma(DMA_VRAM, (void*) bgColumnBuffer, PLANE_BG + (((bgCameraTileX + currentCol) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    }
    while (currentCol != 0);
}

void updateCamera()
{
    // TODO -- Calculate the camera's position based on the player's current position.

    // TODO -- Make sure the camera's position doesn't go below zero (for this demo we do this in JoypadHandler).

    if (fgCameraPixelX > fgCameraLimitPixelX)
    {
        fgCameraPixelX = fgCameraLimitPixelX;
    }

    if (fgCameraPixelY > fgCameraLimitPixelY)
    {
        fgCameraPixelY = fgCameraLimitPixelY;
    }

    fgCameraTileX = PIXEL_TO_TILE(fgCameraPixelX);
    fgCameraTileY = PIXEL_TO_TILE(fgCameraPixelY);

    bgCameraTileX = PIXEL_TO_TILE(fgCameraPixelX >> 1);
    bgCameraTileY = PIXEL_TO_TILE(fgCameraPixelY >> 1);
}
