#include "JoypadHandler.h"

// TOP_SPEED is in pixels/frame.  Maximum value is 8.
#define TOP_SPEED 4

u16 joystate;
u16 pressedStart = 0;

void Joypad_update()
{
    joystate = JOY_readJoypad(JOY_1);

    if (joystate & BUTTON_RIGHT)
    {
        cameraPixelX += TOP_SPEED;
    }
    else if (joystate & BUTTON_LEFT)
    {
        // Since we're directly manipulating the camera, make sure it doesn't go negative.
        if (cameraPixelX != 0)
        {
            cameraPixelX -= TOP_SPEED;
        }
    }

    if (joystate & BUTTON_UP)
    {
        // Since we're directly manipulating the camera, make sure it doesn't go negative.
        if (cameraPixelY != 0)
        {
            cameraPixelY -= TOP_SPEED;
        }
    }
    else if (joystate & BUTTON_DOWN)
    {
        cameraPixelY += TOP_SPEED;
    }
}
