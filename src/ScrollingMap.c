#include <genesis.h>
#include "graphics.h"
#include "MathUtil.h"
#include "ScrollingMap.h"

// NOTE: While not a direct port from the original, the structure and techniques used here were inspired from
// sikthehedgehog's Dragon's Castle.
//
// https://github.com/sikthehedgehog/dragon/blob/master/src-68k/stage.68k

// The maximum coordinates (towards the bottom right) where the camera can be without showing anything beyond the map edges.
u32 cameraLimitPixelX;
u32 cameraLimitPixelY;

// The coordinates of the camera (top-left pixel on the screen).  These will be clamped to be within the map boundaries (0,0)-(cameraLimitPixelX,cameraLimitPixelY)
u32 cameraPixelX;
u32 cameraPixelY;

// Coordinates of the tile containing cameraPixelX and cameraPixelY.
u16 cameraTileX;
u16 cameraTileY;

// Buffers used for copying map data to VRAM.
u16 rowBuffer[VDP_PLANE_TILE_WIDTH];
u16 columnBuffer[VDP_PLANE_TILE_HEIGHT];

Map currentMap;  // SGDK Map structure.  w and h are width and height in tiles.

void redrawRow(u16 rowToUpdate);
void redrawColumn(u16 columnToUpdate);
void redrawStage();
void updateCamera();

void ScrollingMap_init()
{
    VDP_setPlanSize(VDP_PLANE_TILE_WIDTH, VDP_PLANE_TILE_HEIGHT);

    currentMap.w = TILEMAP_TEST_1_TILE_WIDTH;
    currentMap.h = TILEMAP_TEST_1_TILE_HEIGHT;
    currentMap.tilemap = (u16*) TILEMAP_TEST_1_MAP;

    // TODO -- Initialize the camera's position based on the player's starting position.
    cameraPixelX = 0;
    cameraPixelY = 0;
    cameraLimitPixelX = TILE_TO_PIXEL(currentMap.w) - SCREEN_PIXEL_WIDTH;
    cameraLimitPixelY = TILE_TO_PIXEL(currentMap.h) - SCREEN_PIXEL_HEIGHT;

    // Load tiles
    VDP_loadTileData((const u32*) TILEMAP_TEST_1_TILES, MAP_TILE_START_IDX, TILEMAP_TEST_1_TILE_COUNT, 0);

    // Calculate row offsets so we don't need to multiply later.
    u16 rowOffset = 0;
    u16 i;
    for (i = 0; i < currentMap.h; i++)
    {
        mapRowOffsets[i] = rowOffset;
        rowOffset += currentMap.w;
    }

    updateCamera();
    redrawStage();
}

void ScrollingMap_update()
{
    u16 oldCameraTileX = cameraTileX;
    u16 oldCameraTileY = cameraTileY;

    updateCamera();

    if (cameraTileX < oldCameraTileX)
    {
        // Moved left.
        redrawColumn(cameraTileX);
    }
    else if (cameraTileX > oldCameraTileX)
    {
        // Moved right.
        redrawColumn(cameraTileX + SCREEN_TILE_WIDTH);
    }

    if (cameraTileY < oldCameraTileY)
    {
        // Moved up.
        redrawRow(cameraTileY);
    }
    else if (cameraTileY > oldCameraTileY)
    {
        // Moved down.
        redrawRow(cameraTileY + SCREEN_TILE_HEIGHT);
    }
}

void ScrollingMap_updateVDP()
{
    VDP_setHorizontalScroll(PLAN_B, -cameraPixelX);
    VDP_setVerticalScroll(PLAN_B, cameraPixelY);
}

void redrawRow(u16 rowToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = currentMap.tilemap + mapRowOffsets[rowToUpdate] + cameraTileX;

    u16 rowBufferIdx = cameraTileX;
    u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, MAP_TILE_START_IDX);

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
    DMA_queueDma(DMA_VRAM, (u32) rowBuffer, VDP_PLAN_B + ((((rowToUpdate & VDP_PLANE_TILE_HEIGHT_MINUS_ONE) << 6)) << 1), VDP_PLANE_TILE_WIDTH, 2);
}

void redrawColumn(u16 columnToUpdate)
{
    // Calculate where in the tilemap the new row's tiles are located.
    const u16* mapDataAddr = currentMap.tilemap + mapRowOffsets[cameraTileY] + columnToUpdate;

    u16 columnBufferIdx = cameraTileY;
    u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, MAP_TILE_START_IDX);

    // Copy the tiles into the buffer.
    u16 i;
    for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
    {
        columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
        columnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
        columnBufferIdx++;
        mapDataAddr += currentMap.w;
    }

    // Queue copying the buffer into VRAM.
    DMA_queueDma(DMA_VRAM, (u32) columnBuffer, VDP_PLAN_B + ((columnToUpdate & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
}

// Redraw the whole stage.  Normally this would be done with the screen blacked out.
void redrawStage()
{
    u16 currentCol = SCREEN_TILE_WIDTH_PLUS_ONE;
    do
    {
        currentCol--;

        // Calculate where in the tilemap the new row's tiles are located.
        const u16* mapDataAddr = currentMap.tilemap + mapRowOffsets[cameraTileY] + cameraTileX + currentCol;

        u16 columnBufferIdx = cameraTileY;
        u16 baseTile = TILE_ATTR_FULL(PAL0, 0, 0, 0, MAP_TILE_START_IDX);

        // Copy the tiles into the buffer.
        u16 i;
        for (i = VDP_PLANE_TILE_HEIGHT; i != 0; i--)
        {
            columnBufferIdx &= 0x1F;  // columnBufferIdx MOD 32 (VDP_PLANE_TILE_HEIGHT)
            columnBuffer[columnBufferIdx] = baseTile + *mapDataAddr;
            columnBufferIdx++;
            mapDataAddr += currentMap.w;
        }

        // Since we're redrawing the whole stage, do the DMA immediately instead of queuing it up.
        DMA_doDma(DMA_VRAM, (u32) columnBuffer, VDP_PLAN_B + (((cameraTileX + currentCol) & VDP_PLANE_TILE_WIDTH_MINUS_ONE) << 1), VDP_PLANE_TILE_HEIGHT, VDP_PLANE_TILE_WIDTH_TIMES_TWO);
    }
    while (currentCol != 0);
}

void updateCamera()
{
    // TODO -- Calculate the camera's position based on the player's current position.

    // TODO -- Make sure the camera's position doesn't go below zero (for this demo we do this in JoypadHandler).

    if (cameraPixelX > cameraLimitPixelX)
    {
        cameraPixelX = cameraLimitPixelX;
    }

    if (cameraPixelY > cameraLimitPixelY)
    {
        cameraPixelY = cameraLimitPixelY;
    }

    cameraTileX = PIXEL_TO_TILE(cameraPixelX);
    cameraTileY = PIXEL_TO_TILE(cameraPixelY);
}
