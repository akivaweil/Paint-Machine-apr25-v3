#ifndef GENERALSETTINGS_PINDEF_H
#define GENERALSETTINGS_PINDEF_H

// =====================
// WiFi Settings (Credentials defined in main.cpp)
// =====================
extern const char* ssid;
extern const char* password;
extern const char* hostname;

// =====================
// Motion Settings
// =====================
#define STEPS_PER_INCH_XY 254      // Steps per inch for X/Y (400 steps/rev / (20 teeth * 2mm pitch) * 25.4 mm/in)
#define STEPS_PER_INCH_Z 2580.64    // Steps per inch for Z (Assuming same as XY)
#define STEPS_PER_DEGREE 11.11111f   // Adjusted for better precision (4000.0f / 360.0f = 11.11111f)

// Homing
#define HOMING_SPEED 2000          // Homing speed (steps/sec)
#define HOMING_ACCEL 5000          // Homing acceleration (steps/s^2)
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
#define ROLL_VERTICAL 45            // Roll angle for vertical spray (degrees)
#define ROLL_HORIZONTAL 140         // Roll angle for horizontal spray (degrees)
#define SERVO_INIT_POS_PITCH PITCH_SERVO_MAX // Initial pitch position on boot
#define SERVO_INIT_POS_ROLL ROLL_VERTICAL    // Initial roll position on boot

// =====================
// Miscellaneous Settings
// =====================
#define DEBOUNCE_INTERVAL 5        // Debounce interval for limit switches (ms) - Increased back to 5ms
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
#define Z_STEP_PIN 38
#define Z_DIR_PIN 37
#define Z_HOME_SWITCH 4
#define STEPS_PER_INCH_Z 2580.64
#define Z_HOME_POS_INCH 0.0f
#define Z_MAX_TRAVEL_NEG_INCH -2.75f // Maximum travel downwards in inches from home (bottom)

// PnP Z Heights (Absolute positions in inches, relative to Z_HOME_POS_INCH = 0)
// Ensure these are within [Z_MAX_TRAVEL_NEG_INCH, 0]
#define PNP_Z_TRAVEL_INCH -0.5f // Safe height for XY travel between pick and place
#define PNP_Z_PICK_INCH   -2.5f // Height to extend cylinder for picking
#define PNP_Z_PLACE_INCH  -2.0f // Height to extend cylinder for placing

// Rotation
#define ROTATION_STEP_PIN 40
#define ROTATION_DIR_PIN 39

// Servos
#define PITCH_SERVO_PIN 47        // Updated as requested
#define ROLL_SERVO_PIN 48         // Updated as requested

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

#endif // GENERALSETTINGS_PINDEF_H 