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

    // Load palettes
    VDP_setPalette(PAL0, PAL_BG);
    VDP_setPalette(PAL1, PAL_FG);
    VDP_setPaletteColor((PAL1 * 16), 0x0e00);  // Background color
    VDP_setPalette(PAL2, palette_green);
    VDP_setPalette(PAL3, palette_blue);
    VDP_setPaletteColor((PAL3 * 16) + 15, 0x0eee);  // Text color

    ScrollingMap_init();

    while(1)
    {
        Joypad_update();
        ScrollingMap_update();
        SYS_doVBlankProcess();
        ScrollingMap_updateVDP();
    }
}
