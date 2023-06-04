#include <genesis.h>
#include "graphics.h"
#include "JoypadHandler.h"
#include "ScrollingMap.h"

int main()
{
    // Initialize the video processor, set screen resolution to 320x224
    VDP_init();
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    // Initialize the controllers and set up the controller callback function
    JOY_init();
    JOY_setEventHandler(NULL);

    // Set up VDP modes
    VDP_setHilightShadow(0);
    VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);

    ScrollingMap_init();

    // Load palettes
    PAL_setPalette(PAL0, PAL_BG, DMA);
    PAL_setPalette(PAL1, PAL_FG, DMA);
    PAL_setColor((PAL1 * 16), 0x0e00);  // Background color
    PAL_setPalette(PAL2, PAL_BG, DMA);
    PAL_setPalette(PAL3, PAL_FG, DMA);
    PAL_setColor((PAL3 * 16) + 15, 0x0eee);  // Text color

    while(1)
    {
        Joypad_update();
        ScrollingMap_update();

        // Wait for VBlank
        SYS_doVBlankProcess();
        ScrollingMap_updateVDP();
    }
}
