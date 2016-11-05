#include <genesis.h>
#include "graphics.h"
#include "MathUtil.h"
#include "ScrollingMap.h"

// TODO -- Fix reusing buffers before we get the chance to DMA them.  Using DMA_doDma() as workaround.
// TODO -- Is passing many variables as parameters like we are in redraw functions really a good idea?  Probably not.
// TODO -- Need to name fg/bg variables better.
// TODO -- Background should probably wrap -- at least horizontally if not vertically.

// NOTE: While not a direct port from the original, the structure and techniques used here were inspired from
// sikthehedgehog's Dragon's Castle.
//
// https://github.com/sikthehedgehog/dragon/blob/master/src-68k/stage.68k

// NOTE: Width of background map must be ((width of foreground map / 2) + 160).
// NOTE: Height of background map must be ((height of foreground map / 2) + 112).

// The maximum coordinates (towards the bottom right) where the camera can be without showing anything beyond the map edges.
u32 cameraLimitPixelX;
u32 cameraLimitPixelY;

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
u16 rowBuffer[VDP_PLANE_TILE_WIDTH];
u16 columnBuffer[VDP_PLANE_TILE_HEIGHT];

Map currentMap;  // SGDK Map structure.  w and h are width and height in tiles.
Map currentBackground;

u16 mapTilesetStartIdx;
u16 backgroundTilesetStartIdx;

void redrawRow(u16 rowToUpdate, u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 cameraTileX, u16 rowOffset);
void redrawColumn(u16 columnToUpdate, u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 mapWidth, u16 cameraTileY, u16 rowOffset);
void redrawPlane(u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 mapWidth, u16 cameraTileX, u16 cameraTileY, u16 rowOffset);
void updateCamera();

void ScrollingMap_init()
{
    VDP_setPlanSize(VDP_PLANE_TILE_WIDTH, VDP_PLANE_TILE_HEIGHT);

    currentMap.w = TILEMAP_TEST_FG_TILE_WIDTH;
    currentMap.h = TILEMAP_TEST_FG_TILE_HEIGHT;
    currentMap.tilemap = (u16*) TILEMAP_TEST_FG_MAP;

    currentBackground.w = TILEMAP_TEST_BG_TILE_WIDTH;
    currentBackground.h = TILEMAP_TEST_BG_TILE_HEIGHT;
    currentBackground.tilemap = (u16*) TILEMAP_TEST_BG_MAP;

    // TODO -- Initialize the camera's position based on the player's starting position.
    fgCameraPixelX = 0;
    fgCameraPixelY = 0;
    cameraLimitPixelX = TILE_TO_PIXEL(currentMap.w) - SCREEN_PIXEL_WIDTH;
    cameraLimitPixelY = TILE_TO_PIXEL(currentMap.h) - SCREEN_PIXEL_HEIGHT;

    // Load tiles
    backgroundTilesetStartIdx = MAP_TILE_START_IDX;
    mapTilesetStartIdx = MAP_TILE_START_IDX + TILEMAP_TEST_BG_TILE_COUNT;

    VDP_loadTileData((const u32*) TILEMAP_TEST_BG_TILES, backgroundTilesetStartIdx, TILEMAP_TEST_BG_TILE_COUNT, 0);
    VDP_loadTileData((const u32*) TILEMAP_TEST_FG_TILES, mapTilesetStartIdx, TILEMAP_TEST_FG_TILE_COUNT, 0);

    // Calculate row offsets so we don't need to multiply later.
    u16 rowOffset = 0;
    u16 i;
    for (i = 0; i < currentMap.h; i++)
    {
        fgRowOffsets[i] = rowOffset;
        rowOffset += currentMap.w;
    }

    rowOffset = 0;
    for (i = 0; i < currentBackground.h; i++)
    {
        bgRowOffsets[i] = rowOffset;
        rowOffset += currentBackground.w;
    }

    updateCamera();
    redrawPlane(VDP_PLAN_A, currentMap.tilemap, PAL1, mapTilesetStartIdx, currentMap.w, fgCameraTileX, fgCameraTileY, fgRowOffsets[fgCameraTileY]);
    redrawPlane(VDP_PLAN_B, currentBackground.tilemap, PAL0, backgroundTilesetStartIdx, currentBackground.w, bgCameraTileX, bgCameraTileY, bgRowOffsets[bgCameraTileY]);
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
        redrawColumn(fgCameraTileX, VDP_PLAN_A, currentMap.tilemap, PAL1, mapTilesetStartIdx, currentMap.w, fgCameraTileY, fgRowOffsets[fgCameraTileY]);
    }
    else if (fgCameraTileX > oldFGCameraTileX)
    {
        // Moved right.
        redrawColumn(fgCameraTileX + SCREEN_TILE_WIDTH, VDP_PLAN_A, currentMap.tilemap, PAL1, mapTilesetStartIdx, currentMap.w, fgCameraTileY, fgRowOffsets[fgCameraTileY]);
    }

    if (fgCameraTileY < oldFGCameraTileY)
    {
        // Moved up.
        redrawRow(fgCameraTileY, VDP_PLAN_A, currentMap.tilemap, PAL1, mapTilesetStartIdx, fgCameraTileX, fgRowOffsets[fgCameraTileY]);
    }
    else if (fgCameraTileY > oldFGCameraTileY)
    {
        // Moved down.
        redrawRow(fgCameraTileY + SCREEN_TILE_HEIGHT, VDP_PLAN_A, currentMap.tilemap, PAL1, mapTilesetStartIdx, fgCameraTileX, fgRowOffsets[fgCameraTileY + SCREEN_TILE_HEIGHT]);
    }

    // Background
    if (bgCameraTileX < oldBGCameraTileX)
    {
        // Moved left.
        redrawColumn(bgCameraTileX, VDP_PLAN_B, currentBackground.tilemap, PAL0, backgroundTilesetStartIdx, currentBackground.w, bgCameraTileY, bgRowOffsets[bgCameraTileY]);
    }
    else if (bgCameraTileX > oldBGCameraTileX)
    {
        // Moved right.
        redrawColumn(bgCameraTileX + SCREEN_TILE_WIDTH, VDP_PLAN_B, currentBackground.tilemap, PAL0, backgroundTilesetStartIdx, currentBackground.w, bgCameraTileY, bgRowOffsets[bgCameraTileY]);
    }

    if (bgCameraTileY < oldBGCameraTileY)
    {
        // Moved up.
        redrawRow(bgCameraTileY, VDP_PLAN_B, currentBackground.tilemap, PAL0, backgroundTilesetStartIdx, bgCameraTileX, bgRowOffsets[bgCameraTileY]);
    }
    else if (bgCameraTileY > oldBGCameraTileY)
    {
        // Moved down.
        redrawRow(bgCameraTileY + SCREEN_TILE_HEIGHT, VDP_PLAN_B, currentBackground.tilemap, PAL0, backgroundTilesetStartIdx, bgCameraTileX, bgRowOffsets[bgCameraTileY + SCREEN_TILE_HEIGHT]);
    }
}

void ScrollingMap_updateVDP()
{
    // Foreground
    VDP_setHorizontalScroll(PLAN_A, -fgCameraPixelX);
    VDP_setVerticalScroll(PLAN_A, fgCameraPixelY);

    // Background, scrolls at half the rate.
    VDP_setHorizontalScroll(PLAN_B, -(fgCameraPixelX >> 1));
    VDP_setVerticalScroll(PLAN_B, (fgCameraPixelY >> 1));
}

void redrawRow(u16 rowToUpdate, u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 cameraTileX, u16 rowOffset)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = tilemap + rowOffset + cameraTileX;

    u16 rowBufferIdx = cameraTileX;
    u16 baseTile = TILE_ATTR_FULL(palette, 0, 0, 0, mapTileStart);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_WIDTH; i != 0; i--)
    {
        rowBufferIdx &= 0x3F;  // rowBufferIdx MOD 64 (VDP_PLANE_TILE_WIDTH)
        // TODO -- Need to determine which is better -- rowBuffer[rowBufferIdx] or *(rowBuffer + rowBufferIdx).
        rowBuffer[rowBufferIdx] = baseTile + *mapDataAddr;
        rowBufferIdx++;
        mapDataAddr++;
    }

    // Queue copying the buffer into VRAM.
    //DMA_queueDma(DMA_VRAM, (u32) rowBuffer, plane + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
    DMA_doDma(DMA_VRAM, (u32) rowBuffer, plane + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
}

void redrawColumn(u16 columnToUpdate, u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 mapWidth, u16 cameraTileY, u16 rowOffset)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = tilemap + rowOffset + columnToUpdate;

    u16 columnBufferIdx = cameraTileY;
    u16 baseTile = TILE_ATTR_FULL(palette, 0, 0, 0, mapTileStart);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
    {
        columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
        columnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
        columnBufferIdx++;
        mapDataAddr += mapWidth;
    }

    // Queue copying the buffer into VRAM.
    //DMA_queueDma(DMA_VRAM, (u32) columnBuffer, plane + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    DMA_doDma(DMA_VRAM, (u32) columnBuffer, plane + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
}

// Redraw the whole stage.  Normally this would be done with the screen blacked out.
void redrawPlane(u16 plane, u16* tilemap, u16 palette, u16 mapTileStart, u16 mapWidth, u16 cameraTileX, u16 cameraTileY, u16 rowOffset)
{
    // TODO -- Need to use parameters for camera tile X and Y.  Currently assumes we are starting at 0,0.

    u16 currentCol = SCREEN_TILE_WIDTH_PLUS_ONE;
    do
    {
        currentCol--;

        // Calculate where in the tilemap the new row's tiles are located.
        const u16* mapDataAddr = tilemap + rowOffset + cameraTileX + currentCol;

        u16 columnBufferIdx = cameraTileY;
        u16 baseTile = TILE_ATTR_FULL(palette, 0, 0, 0, mapTileStart);

        // Copy the tiles into the buffer.
        u16 i;
        for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
        {
            columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
            columnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
            columnBufferIdx++;
            mapDataAddr += mapWidth;
        }

        // Since we're redrawing the whole stage, do the DMA immediately instead of queuing it up.
        DMA_doDma(DMA_VRAM, (u32) columnBuffer, plane + (((cameraTileX + currentCol) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    }
    while (currentCol != 0);
}

void updateCamera()
{
    // TODO -- Calculate the camera's position based on the player's current position.

    // TODO -- Make sure the camera's position doesn't go below zero (for this demo we do this in JoypadHandler).

    if (fgCameraPixelX > cameraLimitPixelX)
    {
        fgCameraPixelX = cameraLimitPixelX;
    }

    if (fgCameraPixelY > cameraLimitPixelY)
    {
        fgCameraPixelY = cameraLimitPixelY;
    }

    fgCameraTileX = PIXEL_TO_TILE(fgCameraPixelX);
    fgCameraTileY = PIXEL_TO_TILE(fgCameraPixelY);

    bgCameraTileX = PIXEL_TO_TILE(fgCameraPixelX >> 1);
    bgCameraTileY = PIXEL_TO_TILE(fgCameraPixelY >> 1);
}
