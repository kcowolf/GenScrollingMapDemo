#ifndef JOYPADHANDLER_H
#define JOYPADHANDLER_H

#include <genesis.h>

void Joypad_update();

// TODO -- Normally these wouldn't be exposed here.  JoypadHandler would manipulate the player's
// position and the camera's position would be calculated based on that.  However, for this demo
// I let the JoypadHandler manipulate the camera coordinates directly.
extern u32 cameraPixelX;
extern u32 cameraPixelY;

#endif // JOYPADHANDLER_H
