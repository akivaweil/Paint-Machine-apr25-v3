#ifndef PAINTING_H
#define PAINTING_H

#include <Arduino.h>
#include "../Main/GeneralSettings_PinDef.h" // For servo limits, etc.

// === Extern Variables (Defined in Painting.cpp) ===

// Painting Offsets
extern float paintPatternOffsetX_inch;
extern float paintPatternOffsetY_inch;
extern float paintGunOffsetX_inch;
extern float paintGunOffsetY_inch;

// Painting Side Settings (Arrays for 4 sides: 0=Back, 1=Right, 2=Front, 3=Left)
extern float paintZHeight_inch[4];
extern int   paintPitchAngle[4];
extern int   paintRollAngle[4];
extern float paintSpeed[4];

// === Constants ===

#define PAINT_Z_TRAVEL_CLEARANCE_INCH 0.2f // How far above paint height to travel

// Rotation Positions (degrees relative to Back=0)
// Declared here, defined in Painting.cpp
extern const int ROT_POS_BACK_DEG;
extern const int ROT_POS_RIGHT_DEG;
extern const int ROT_POS_FRONT_DEG;
extern const int ROT_POS_LEFT_DEG;

// === Function Declarations ===

// Placeholder painting functions
void startPaintingSequence();
void paintSide(int sideIndex); // Side index 0-3

#endif // PAINTING_H 