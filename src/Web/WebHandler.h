#ifndef WEBHANLDER_H
#define WEBHANLDER_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FastAccelStepper.h> // Needed for stepper types in extern declarations
#include <ESP32Servo.h>     // Needed for Servo type

// --- Extern Global Variable Declarations ---
// These variables are defined in main.cpp but needed by the web handler

// Steppers
extern FastAccelStepper *stepper_x;
extern FastAccelStepper *stepper_y_left;
extern FastAccelStepper *stepper_y_right;
extern FastAccelStepper *stepper_z;
extern FastAccelStepper *stepper_rot;

// Servos
extern Servo servo_pitch;

// State Variables
extern bool allHomed;
extern volatile bool isMoving;
extern volatile bool isHoming;
extern volatile bool inPickPlaceMode;
extern volatile bool pendingHomingAfterPnP;
extern volatile bool inCalibrationMode;
extern volatile bool stopRequested; // For paintSide stop check

// Tray Dimensions
extern float trayWidth_inch;
extern float trayHeight_inch;

// Speed/Accel
extern float patternXSpeed;
extern float patternXAccel;
extern float patternYSpeed;
extern float patternYAccel;
extern float patternZSpeed;  // Needed by sendAllSettingsUpdate (though not set via WS)
extern float patternZAccel; // Needed by sendAllSettingsUpdate (though not set via WS)
extern float patternRotSpeed; // Needed? Maybe not directly, but good practice
extern float patternRotAccel; // Needed? Maybe not directly, but good practice

// PnP Settings (Defined in PickPlace.cpp, declared extern in PickPlace.h, need extern here too?)
// It's better to include PickPlace.h if these are defined there. Let's assume they are needed here for now.
extern float pnpOffsetX_inch;
extern float pnpOffsetY_inch;
extern float placeFirstXAbsolute_inch;
extern float placeFirstYAbsolute_inch;
extern int placeGridCols;
extern int placeGridRows;
extern float placeGapX_inch;
extern float placeGapY_inch;
// Constants from PickPlace.h/cpp might be needed if used directly, e.g., pnpBorderWidth_inch
// extern float pnpBorderWidth_inch; // If needed directly by web handler funcs
// extern float pnpItemWidth_inch;
// extern float pnpItemHeight_inch;

// Painting Settings (Defined in Painting.cpp, declared extern in Painting.h)
// We need access to the arrays and offsets defined in main.cpp (or potentially moved)
extern float paintPatternOffsetX_inch;
extern float paintPatternOffsetY_inch;
extern float paintGunOffsetX_inch;
extern float paintGunOffsetY_inch;
extern float paintZHeight_inch[4];
extern int   paintPitchAngle[4];
extern int   paintPatternType[4];
extern float paintSpeed[4];

// --- Extern Function Declarations ---
// Functions defined in main.cpp (or other modules) called by the web handler

// Core main.cpp functions
extern void homeAllAxes();
extern void moveToPositionInches(float targetX_inch, float targetY_inch, float targetZ_inch);
// extern void moveToXYPositionInches(float targetX_inch, float targetY_inch); // Likely only called internally now
extern void calculateAndSetGridSpacing(int cols, int rows);

// PickPlace functions (Better to include PickPlace.h)
// extern void enterPickPlaceMode();
// extern void exitPickPlaceMode(bool requestHoming);
// extern void executeNextPickPlaceStep();
// extern void skipPickPlaceLocation();
// extern void goBackPickPlaceLocation();

// Painting functions (Better to include Painting.h)
// extern void paintSide(int sideIndex);


// --- Web Server and WebSocket Objects ---
extern WebServer webServer;
extern WebSocketsServer webSocket;

// --- Function Declarations for WebHandler.cpp ---
void setupWebServerAndWebSocket(); // Renamed from setupWebServer
void handleRoot();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
extern void saveSettings(); // Declare saveSettings defined in main.cpp
void sendCurrentPositionUpdate(); // Sends position via WebSocket
void sendAllSettingsUpdate(uint8_t specificClientNum, String message); // Sends all settings via WebSocket


#endif // WEBHANLDER_H 