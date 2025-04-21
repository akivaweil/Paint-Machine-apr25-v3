#ifndef PICKPLACE_H
#define PICKPLACE_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <WebSocketsServer.h> // For broadcasting status
#include <Bounce2.h> // For button debouncer access (if needed directly)
#include "../Main/GeneralSettings_PinDef.h" // Access pin definitions

// Forward declare stepper objects defined in main.cpp
extern FastAccelStepper *stepper_x;
extern FastAccelStepper *stepper_y_left;
extern FastAccelStepper *stepper_y_right;
extern FastAccelStepper *stepper_z; // Add Z if PnP needs Z control

// Forward declare WebSocket object defined in main.cpp
extern WebSocketsServer webSocket;

// Forward declare global state flags defined in main.cpp that PnP needs to READ
extern bool allHomed;
extern volatile bool isMoving; // True if any motor is moving (generic or PnP)
extern volatile bool isHoming; // True if homing sequence is active
extern volatile bool inPickPlaceMode; // True if PnP mode is active
extern volatile bool pendingHomingAfterPnP; // Flag set when PnP completes to trigger homing in main loop

// === Pick and Place Constants and Variables ===
// These are used internally by PickPlace.cpp and potentially by main.cpp if needed

// Base Offset for all PnP Operations (Defined in PickPlace.cpp)
extern float pnpOffsetX_inch;
extern float pnpOffsetY_inch;

// Pick position RELATIVE to the offset (Offset IS the pick position)
const float PICK_X_RELATIVE_INCH = 0.0;
const float PICK_Y_RELATIVE_INCH = 0.0;

// First Place position ABSOLUTE (Defined in main.cpp)
extern float placeFirstXAbsolute_inch;
extern float placeFirstYAbsolute_inch;

// Grid and Spacing (Constants)
extern int placeGridCols;
extern int placeGridRows;

// === Function Declarations ===

// Initialization (if needed, e.g., for PnP specific hardware)
// void setupPickPlace();

// Main PnP control functions (called from main.cpp)
void enterPickPlaceMode();
void exitPickPlaceMode(bool shouldHomeAfterExit = false);
void executeNextPickPlaceStep();
void moveToNextPickPlaceSquare();

// Helper function (called internally or possibly from main.cpp if needed)
void moveToXYPositionInches_PnP(float targetX_inch, float targetY_inch); // PnP specific XY move
void moveToZ_PnP(float targetZ_inch, bool wait_for_completion = true); // PnP specific Z move with optional wait

// Optional: Add function to handle PnP button press logic if moved from loop()
// void handlePickPlaceButton();


#endif // PICKPLACE_H 