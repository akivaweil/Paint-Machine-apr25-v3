#ifndef GENERALSETTINGS_PINDEF_H
#define GENERALSETTINGS_PINDEF_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>

// =====================
// WiFi Settings (Credentials defined in main.cpp)
// =====================
extern const char* ssid;
extern const char* password;
extern const char* hostname;

// =====================
// Motion Settings
// =====================

// Homing
#define HOMING_TIMEOUT 15000       // Homing timeout (ms)

// Pattern/General Move Speeds/Accelerations (Variables)
extern float patternXSpeed;      // Default pattern X speed (steps/sec)
extern float patternXAccel;      // Default pattern X acceleration (steps/sec^2)
extern float patternYSpeed;      // Default pattern Y speed (steps/sec)
extern float patternYAccel;      // Default pattern Y acceleration (steps/sec^2)
extern float patternZSpeed;      // Speed for Z moves within patterns (steps/sec)
extern float patternZAccel;      // Accel for Z moves within patterns (steps/sec^2)
extern float patternRotSpeed;    // Speed for Rotation moves within patterns (steps/sec)
extern float patternRotAccel;    // Accel for Rotation moves within patterns (steps/sec^2)

// =====================
// Servo Settings
// =====================
#define PITCH_SERVO_MIN 150         // Minimum safe pitch angle (degrees) - adjusted after servo swap
#define PITCH_SERVO_MAX 180         // Maximum safe pitch angle (degrees) - matches ROLL_HORIZONTAL
#define SERVO_INIT_POS_PITCH PITCH_SERVO_MAX // Initial pitch position on boot

// =====================
// Miscellaneous Settings
// =====================
#define DEFAULT_PATTERN_DELAY 1000 // Default delay between pattern repetitions (ms)

// =====================
// Pin Assignments - UPDATED as requested
// =====================
// X Axis
#define X_STEP_PIN 36
#define X_DIR_PIN 35
#define X_HOME_SWITCH 5           // Updated to pin 5 as requested

// Y Axis (Left & Right)
#define Y_LEFT_STEP_PIN 42
#define Y_LEFT_DIR_PIN 41
#define Y_LEFT_HOME_SWITCH 16     // Updated as requested

#define Y_RIGHT_STEP_PIN 1
#define Y_RIGHT_DIR_PIN 2
#define Y_RIGHT_HOME_SWITCH 18    // Updated as requested

// Z Axis
#define Z_STEP_PIN 38             // Reverted from 38
#define Z_DIR_PIN 37             // Reverted from 37 (Conflict with Y Left Dir)
#define Z_HOME_SWITCH 4          // Reverted from 15 (Conflict with Y Right Home)
#define STEPS_PER_INCH_Z 254

#define Z_HOME_POS_INCH 0.0f       // Position value when Z is at the home switch (top)
#define Z_MAX_TRAVEL_NEG_INCH 0.0f // Minimum Z position (usually home or slightly negative)
#define Z_MAX_TRAVEL_POS_INCH 2.75f  // Maximum downward position in inches (bottom limit)

// Define safe Z height for travel during PnP (relative to home/0)
// Ensure this is within the 0 to 2.75 range.
// A smaller value means higher up (closer to 0).
#define PNP_Z_CLEARANCE_INCH 0.1f // Example: Lift 0.1 inch from home before XY travel

// Define Z height for picking up items (relative to home/0)
// This needs calibration based on pickup surface and item height.
// Ensure this is within the 0 to 2.75 range.
#define PNP_Z_PICK_HEIGHT_INCH 2.5f // Example: Move down to 2.5 inches for pickup

// Define Z height for placing items (relative to home/0)
// This needs calibration based on placement surface.
// Ensure this is within the 0 to 2.75 range.
#define PNP_Z_PLACE_HEIGHT_INCH 2.0f // Example: Move down to 2.0 inches for placing

// Rotation
#define ROTATION_STEP_PIN 40
#define ROTATION_DIR_PIN 39

// Servos
#define PITCH_SERVO_PIN 47        // Updated as requested

// Actuators (Relays)
#define PICK_CYLINDER_PIN 10      // Relay for pick cylinder (Changed back from 12)
#define SUCTION_PIN 11            // Relay for suction vacuum (Changed back from 13)

// Inputs
#define PNP_CYCLE_BUTTON_PIN 17

// =====================
// Pick and Place Settings -- MOVED TO PickPlaceSettings.h
// =====================
// #define PICK_X_POS_INCH 10.0f
// #define PICK_Y_POS_INCH 0.25f
// #define PLACE_GRID_ROWS 6
// #define PLACE_GRID_COLS 4
// #define PLACE_GRID_FIRST_X_INCH 16.0f
// #define PLACE_GRID_FIRST_Y_INCH 13.0f
// #define PLACE_GRID_SPACING_X_INCH 4.0f
// #define PLACE_GRID_SPACING_Y_INCH 4.0f
// #define PICK_PLACE_ACTION_DELAY_MS 500

#define SERVO_MOVEMENT_DELAY_MS 75   // Delay between each degree of servo movement (higher = slower)

// Include the custom Print class definition for the extern declaration
// #include <Print.h> // Required for the Print class definition -> NO LONGER NEEDED HERE

// Forward declare the custom stream class if it's defined elsewhere
// class SerialWebSocketStream; -> NO LONGER NEEDED HERE, full def included via SerialWebSocketStream.h

// ========================================================================
// Pattern Settings
// ========================================================================

// === General Constants ===
#define DEBOUNCE_INTERVAL 5      // milliseconds (Restored original value)
#define HOMING_SPEED 3500          // steps/s (Changed from 2000)
#define HOMING_ACCEL 12500         // steps/s^2 (Changed from 5000, scaled 2.5x)
#define HOMING_TIMEOUT 15000   // milliseconds

// Stepper Conversion Factors
#define STEPS_PER_INCH_XY 254      // Steps per inch for X/Y (400 steps/rev / (20 teeth * 2mm pitch) * 25.4 mm/in) (Restored original value)
#define STEPS_PER_DEGREE 11.11111f // Adjusted for better precision (4000.0f / 360.0f = 11.11111f) (Restored original value)

// Travel Limits (Inches, relative to homed position 0)
#define X_MAX_TRAVEL_POS_INCH 30.0
#define Y_MAX_TRAVEL_POS_INCH 30.0
#define Z_MAX_TRAVEL_POS_INCH 2.75f // Maximum Z travel downwards (Restored original value)

// PnP Item Dimensions (NEW - Moved from main.cpp)
const float pnpItemWidth_inch = 3.0f;
const float pnpItemHeight_inch = 3.0f;
const float pnpBorderWidth_inch = 0.25f; // Border around grid

// === Extern Variables ===
// These are defined in one .cpp file (e.g., main.cpp) and declared here
// to be accessible in other .cpp files (e.g., PickPlace.cpp, Painting.cpp)

// Stepper Engine & Motors
extern FastAccelStepperEngine engine;
extern FastAccelStepper *stepper_x;
extern FastAccelStepper *stepper_y_left;
extern FastAccelStepper *stepper_y_right;
extern FastAccelStepper *stepper_z;
extern FastAccelStepper *stepper_rot;

// Web Server & Socket
extern WebServer webServer;
extern WebSocketsServer webSocket;

// Servos
extern Servo servo_pitch;

// State Variables
extern bool allHomed;
extern volatile bool isMoving;
extern volatile bool isHoming;
extern volatile bool inPickPlaceMode;
extern volatile bool pendingHomingAfterPnP;
extern volatile bool inCalibrationMode;

// Speed/Accel Variables
extern float patternXSpeed;
extern float patternXAccel;
extern float patternYSpeed;
extern float patternYAccel;
extern float patternZSpeed;
extern float patternZAccel;
extern float patternRotSpeed;
extern float patternRotAccel;

// Tray Dimensions
extern float trayWidth_inch;
extern float trayHeight_inch;

// Painting Specific Settings (Arrays)
extern float paintZHeight_inch[4]; // Default Z heights for painting each side
extern int paintPitchAngle[4]; // Default pitch angles (servo values) - Use MAX as default center/0deg visual?
// extern int paintRollAngle[4]; // Default roll angles <-- REMOVED
extern int paintPatternType[4]; // ADDED: 0 = Up-Down, 90 = Left-Right (Default Back/Front=Up-Down, Left/Right=Left-Right)
extern float paintSpeed[4]; // Default speeds

// Painting State
// ... existing code ...

#endif // GENERALSETTINGS_PINDEF_H 