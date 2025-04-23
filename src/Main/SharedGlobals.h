#ifndef SHARED_GLOBALS_H
#define SHARED_GLOBALS_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <WebSocketsServer.h>
#include <Preferences.h> // Needed for Preferences object if shared
#include <Bounce2.h>
#include <ESP32Servo.h> // Include Servo for servo object declaration

// === Shared Global Variable Declarations (Defined in main.cpp) ===

// Stepper Motors
extern FastAccelStepper *stepper_x;
extern FastAccelStepper *stepper_y_left;
extern FastAccelStepper *stepper_y_right;
extern FastAccelStepper *stepper_z;
extern FastAccelStepper *stepper_rot;

// WebSocket Server
extern WebSocketsServer webSocket;

// Control/State Flags
extern volatile bool stopRequested;
extern volatile bool isMoving; // Used by many functions
extern bool allHomed;
extern volatile bool isHoming;
extern volatile bool inPickPlaceMode;
extern volatile bool inCalibrationMode;

// PnP/Grid/Tray Settings (Potentially needed by patterns/actions)
extern const float pnpItemWidth_inch;
extern const float pnpItemHeight_inch;
extern float placeGapX_inch;
extern float placeGapY_inch;
extern float placeFirstXAbsolute_inch;
extern float placeFirstYAbsolute_inch;
extern int placeGridCols;
extern int placeGridRows;
extern float trayWidth_inch;
extern float trayHeight_inch;
extern const float pnpBorderWidth_inch; // Needed for gap calculation if done outside main

// PnP Specific Locations (Defined in main.cpp)
extern float pnpPickLocationX_inch;
extern float pnpPickLocationY_inch;
extern float pnpPickLocationZ_inch;
extern float pnpPlaceHeight_inch;

// Painting Settings (Potentially needed by patterns/actions)
extern float paintZHeight_inch[4];
extern int   paintPitchAngle[4];
extern int   paintPatternType[4];
extern float paintSpeed[4];
extern float paintGunOffsetX_inch;
extern float paintGunOffsetY_inch;
extern float patternZSpeed; // General Z speed
extern float patternZAccel; // General Z accel

// Painting Specific Settings (Arrays) - Defined in main.cpp
extern float paintStartX[4]; // NEW: Start X for each side
extern float paintStartY[4]; // NEW: Start Y for each side

// Core Constants
// REMOVED - These are #defined in GeneralSettings_PinDef.h
// extern const float STEPS_PER_INCH_XY;
// extern const float STEPS_PER_INCH_Z;
// extern const float STEPS_PER_DEGREE;

// Core Rotation Positions
extern const int ROT_POS_BACK_DEG;
extern const int ROT_POS_RIGHT_DEG;
extern const int ROT_POS_FRONT_DEG;
extern const int ROT_POS_LEFT_DEG;

// Servo Object
extern Servo servo_pitch;

// Preferences Object
extern Preferences preferences;

// === Shared Core Function Prototypes (Defined in main.cpp) ===

void moveToXYPositionInches_Paint(float targetX_inch, float targetY_inch, float speedHz, float accel);
void moveZToPositionInches(float targetZ_inch, float speedHz, float accel);
void rotateToAbsoluteDegree(int targetDegree);
void sendCurrentPositionUpdate(); // For updating UI after moves

// Potentially others if needed by actions/patterns, e.g.:
// void printAndBroadcast(const char* message); // If we want a single status function

#endif // SHARED_GLOBALS_H 