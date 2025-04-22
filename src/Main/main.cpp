#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include <WiFi.h>        // Added for WiFi
#include <ArduinoOTA.h>  // Added for OTA
#include <WebServer.h>   // Added for HTTP Server
#include <WebSocketsServer.h> // Added for WebSocket Server
#include <ESP32Servo.h>     // Added for Servos
#include <ArduinoJson.h> // Added for JSON handling
#include "GeneralSettings_PinDef.h" // Updated include
#include "../PickPlace/PickPlace.h" // Include the new PnP header
#include <Preferences.h> // Added for Preferences
#include "../Painting/Painting.h" // Include the new Painting header
#include "html_content.h" // Include the new HTML header

// Define WiFi credentials (required by settings.h)
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";
const char* hostname = "ESP32-PaintMachine"; // Example hostname

// Stepper Engine
FastAccelStepperEngine engine = FastAccelStepperEngine();

// Stepper Motors
FastAccelStepper *stepper_x = NULL;
FastAccelStepper *stepper_y_left = NULL;
FastAccelStepper *stepper_y_right = NULL;
FastAccelStepper *stepper_z = NULL;
FastAccelStepper *stepper_rot = NULL; // Added Rotation Stepper
// Note: Rotation stepper is defined in settings.h but not used here as it lacks a home switch.

// Limit Switch Debouncers
Bounce debouncer_x_home = Bounce();
Bounce debouncer_y_left_home = Bounce();
Bounce debouncer_y_right_home = Bounce();
Bounce debouncer_z_home = Bounce();
Bounce debouncer_pnp_cycle_button = Bounce(); // Added for physical button

// Servos
Servo servo_pitch;
Servo servo_roll;

// Web Server and WebSocket Server
WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Preferences object
Preferences preferences;

// State Variables
bool allHomed = false; // Tracks if initial homing completed successfully
volatile bool isMoving = false; // Tracks if a move command is active
volatile bool isHoming = false; // Tracks if homing sequence is active
volatile bool inPickPlaceMode = false; // Tracks if PnP sequence is active
volatile bool pendingHomingAfterPnP = false; // Flag to home after exiting PnP
volatile bool inCalibrationMode = false; // Tracks if calibration mode is active

// NEW: Tray Dimension Variables
float trayWidth_inch = 24.0; // Default tray width
float trayHeight_inch = 18.0; // Default tray height

// Pattern/General Speed/Accel Variables (declared extern in GeneralSettings_PinDef.h)
float patternXSpeed = 20000.0; // Actual value
float patternXAccel = 20000.0; // Actual value
float patternYSpeed = 20000.0; // Actual value
float patternYAccel = 20000.0; // Actual value
float patternZSpeed = 500.0;
float patternZAccel = 1300.0;
float patternRotSpeed = 2000.0; // Reduced from 3000 for more reliable movement
float patternRotAccel = 1000.0; // Reduced from 2000 for more reliable movement

// PnP variables (declared extern in PickPlace.h, defined in PickPlace.cpp)
// Commented out here because they are defined and managed in PickPlace.cpp
/*
float pnpOffsetX_inch = 15.0;
float pnpOffsetY_inch = 0.0;
float placeFirstXAbsolute_inch = 20.0;
float placeFirstYAbsolute_inch = 20.0;
*/
// PnP Grid/Spacing Variables
int placeGridCols = 4;        // Default Columns
int placeGridRows = 5;        // Default Rows
float placeGapX_inch = 0.0f; // Calculated X GAP between items (formerly placeSpacingX_inch)
float placeGapY_inch = 0.0f; // Calculated Y GAP between items (formerly placeSpacingY_inch)

// Painting Offsets (declared extern in Painting.h, defined in Painting.cpp)
/*
float paintPatternOffsetX_inch = 0.0f;
float paintPatternOffsetY_inch = 0.0f;
float paintGunOffsetX_inch = 0.0f;   // Offset of nozzle from TCP X
float paintGunOffsetY_inch = 1.5f;   // Offset of nozzle from TCP Y (e.g., 1.5 inches forward)

// Painting Side Settings (Arrays for 4 sides: 0=Back, 1=Right, 2=Front, 3=Left)
float paintZHeight_inch[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Default Z height for painting each side
int   paintPitchAngle[4]   = { PITCH_SERVO_MIN, PITCH_SERVO_MIN, PITCH_SERVO_MIN, PITCH_SERVO_MIN }; // Default Pitch angle
int   paintRollAngle[4]    = { ROLL_VERTICAL, ROLL_VERTICAL, ROLL_VERTICAL, ROLL_VERTICAL }; // Default Roll angle
float paintSpeed[4]        = { 10000.0f, 10000.0f, 10000.0f, 10000.0f }; // Default painting speed (steps/sec)

// Rotation Positions (degrees relative to Back=0)
// Assuming 0 = Back, 90 = Right, 180 = Front, 270 = Left
const int ROT_POS_BACK_DEG = 0;
const int ROT_POS_RIGHT_DEG = 90;
const int ROT_POS_FRONT_DEG = 180;
const int ROT_POS_LEFT_DEG = 270;
*/

// Homing states
bool x_homed = false;
bool y_left_homed = false;
bool y_right_homed = false;
bool z_homed = false;

// Function forward declarations
void homeAllAxes();
void moveToPositionInches(float targetX_inch, float targetY_inch, float targetZ_inch);
void moveToXYPositionInches(float targetX_inch, float targetY_inch);
void executePickPlaceCycle(); // Might be obsolete
void executeNextPickPlaceStep();
void enterPickPlaceMode();
void exitPickPlaceMode();
void calculateAndSetGridSpacing(int cols, int rows); // Added forward declaration here
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void sendCurrentPositionUpdate(); // Forward declaration for position updates
void sendAllSettingsUpdate(uint8_t specificClientNum, String message); // Helper to send all settings

// Painting functions (Placeholders moved to Painting.cpp)
/*
void startPaintingSequence();
void paintSide(int sideIndex);
*/

// Function to home a single axis (modified slightly for reuse)
// Returns true if homing was successful, false otherwise (timeout or error)
bool homeSingleAxis(FastAccelStepper* stepper, Bounce* debouncer, int home_switch_pin, const char* axis_name) {
    if (!stepper) return false; // Skip if stepper not initialized

    // Serial.print("Homing ");
    // Serial.print(axis_name);
    // Serial.println("...");

    // Ensure switch pin is configured
    pinMode(home_switch_pin, INPUT);
    debouncer->attach(home_switch_pin);
    debouncer->interval(DEBOUNCE_INTERVAL);

    // Set homing speed and acceleration
    stepper->setSpeedInHz(HOMING_SPEED);
    stepper->setAcceleration(HOMING_ACCEL);

    // Start moving towards the home switch (assuming negative direction)
    stepper->runBackward();

    unsigned long startTime = millis();
    bool success = false;

    // Wait for the switch to be triggered or timeout
    while (millis() - startTime <= HOMING_TIMEOUT) {
        debouncer->update();
        webSocket.loop(); // Keep WebSocket responsive during blocking operations

        // Check if the switch is activated (read HIGH directly)
        if (debouncer->read() == HIGH) {
            // Serial.print(axis_name);
            // Serial.println(" switch triggered.");
            stepper->stopMove(); // Stop the motor smoothly first
            stepper->forceStop(); // Ensure it's stopped
            stepper->setCurrentPosition(0); // Set current position as 0
            success = true;
            // Serial.print(axis_name);
            // Serial.println(" homed.");
            break; // Exit the while loop
        }
        yield(); // Allow background tasks
    }

    if (!success) {
        stepper->forceStop(); // Ensure motor is stopped on timeout
        // Serial.print(axis_name);
        // Serial.println(" homing timed out or failed!");
    }
    return success;
}

// Function to home all axes (Kept in main.cpp as it's a core function)
void homeAllAxes() {
    // --- Exit Calibration if Active ---
    if (inCalibrationMode) {
        Serial.println("[DEBUG] homeAllAxes: Exiting calibration mode implicitly."); // DEBUG
        inCalibrationMode = false;
        // Send status update immediately? Or let Homing status override?
        // Let Homing status handle the UI update. 
    }
    // --- Original Checks ---
    // if (inCalibrationMode) { // This check is now redundant
    //     webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Exit calibration mode before homing.\"}");
    //     return;
    // }
    if (isMoving || isHoming) {
        webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Machine is already moving or homing.\"}");
        return;
    }
    // Reset pick/place mode if we are homing
    inPickPlaceMode = false;

    isHoming = true;
    webSocket.broadcastTXT("{\"status\":\"Homing\", \"message\":\"Homing all axes simultaneously...\"}");
    // Serial.println("Starting Full Homing Sequence (all axes simultaneously)...");

    // Reset homed flags
    x_homed = false;
    y_left_homed = false;
    y_right_homed = false;
    z_homed = false;
    allHomed = false;

    // Initialize done flags
    bool x_done = false;
    bool y_left_done = false;
    bool y_right_done = false;
    bool z_done = false;

    // Configure and start all steppers if they exist
    // Z Axis
    if (stepper_z) {
        pinMode(Z_HOME_SWITCH, INPUT);
        debouncer_z_home.attach(Z_HOME_SWITCH);
        debouncer_z_home.interval(DEBOUNCE_INTERVAL);
        stepper_z->setSpeedInHz(HOMING_SPEED);
        stepper_z->setAcceleration(HOMING_ACCEL);
        stepper_z->runBackward();
    } else {
        z_done = true; // Consider done if stepper doesn't exist
    }

    // X Axis
    if (stepper_x) {
        pinMode(X_HOME_SWITCH, INPUT);
        debouncer_x_home.attach(X_HOME_SWITCH);
        debouncer_x_home.interval(DEBOUNCE_INTERVAL);
        stepper_x->setSpeedInHz(HOMING_SPEED);
        stepper_x->setAcceleration(HOMING_ACCEL);
        stepper_x->runBackward();
    } else {
        x_done = true; // Consider done if stepper doesn't exist
    }

    // Y Left Axis
    if (stepper_y_left) {
        pinMode(Y_LEFT_HOME_SWITCH, INPUT);
        debouncer_y_left_home.attach(Y_LEFT_HOME_SWITCH);
        debouncer_y_left_home.interval(DEBOUNCE_INTERVAL);
        stepper_y_left->setSpeedInHz(HOMING_SPEED);
        stepper_y_left->setAcceleration(HOMING_ACCEL);
        stepper_y_left->runBackward();
    } else {
        y_left_done = true; // Consider done if stepper doesn't exist
    }

    // Y Right Axis
    if (stepper_y_right) {
        pinMode(Y_RIGHT_HOME_SWITCH, INPUT);
        debouncer_y_right_home.attach(Y_RIGHT_HOME_SWITCH);
        debouncer_y_right_home.interval(DEBOUNCE_INTERVAL);
        stepper_y_right->setSpeedInHz(HOMING_SPEED);
        stepper_y_right->setAcceleration(HOMING_ACCEL);
        stepper_y_right->runBackward();
    } else {
        y_right_done = true; // Consider done if stepper doesn't exist
    }

    // Wait for all axes to home or timeout
    unsigned long startTime = millis();
    
    // Monitor all switches and stop once triggered
    while (!(x_done && y_left_done && y_right_done && z_done)) {
        if (millis() - startTime > HOMING_TIMEOUT) {
            // Serial.println("Homing timed out!");
            // Stop all motors that haven't homed yet
            if (stepper_x && !x_done) stepper_x->forceStop();
            if (stepper_y_left && !y_left_done) stepper_y_left->forceStop();
            if (stepper_y_right && !y_right_done) stepper_y_right->forceStop();
            if (stepper_z && !z_done) stepper_z->forceStop();
            
            webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Homing timed out!\"}");
            isHoming = false;
            return;
        }

        webSocket.loop(); // Keep WebSocket responsive

        // Check Z axis
        if (stepper_z && !z_done) {
            debouncer_z_home.update();
            if (debouncer_z_home.read() == HIGH) {
                stepper_z->stopMove();
                stepper_z->forceStop();
                stepper_z->setCurrentPosition(0);
                z_done = true;
                z_homed = true;
                // Serial.println("Z axis homed.");
            }
        }

        // Check X axis
        if (stepper_x && !x_done) {
            debouncer_x_home.update();
            if (debouncer_x_home.read() == HIGH) {
                stepper_x->stopMove();
                stepper_x->forceStop();
                stepper_x->setCurrentPosition(0);
                x_done = true;
                x_homed = true;
                // Serial.println("X axis homed.");
            }
        }

        // Check Y Left axis
        if (stepper_y_left && !y_left_done) {
            debouncer_y_left_home.update();
            if (debouncer_y_left_home.read() == HIGH) {
                stepper_y_left->stopMove();
                stepper_y_left->forceStop();
                stepper_y_left->setCurrentPosition(0);
                y_left_done = true;
                y_left_homed = true;
                // Serial.println("Y Left homed.");
            }
        }

        // Check Y Right axis
        if (stepper_y_right && !y_right_done) {
            debouncer_y_right_home.update();
            if (debouncer_y_right_home.read() == HIGH) {
                stepper_y_right->stopMove();
                stepper_y_right->forceStop();
                stepper_y_right->setCurrentPosition(0);
                y_right_done = true;
                y_right_homed = true;
                // Serial.println("Y Right homed.");
            }
        }
        
        yield(); // Allow background tasks
    }

    // Check if all homing was successful
    bool all_success = x_homed && y_left_homed && y_right_homed && z_homed;

    if (all_success) {
        // Serial.println("All axes homed successfully.");
        
        // Add a small delay to allow motor states to settle before move-away
        delay(50); // 50 millisecond delay

        // Move X and Y away from the home position slightly
        webSocket.broadcastTXT("{\"status\":\"Homing\", \"message\":\"Moving away from home switches...\"}");
        long target_steps = (long)(0.5 * STEPS_PER_INCH_XY); // 0.5 inches in steps
        
        if (stepper_x) {
            stepper_x->setSpeedInHz(patternXSpeed); // Use general pattern speed
            stepper_x->setAcceleration(patternXAccel / 5.0); // Use HALF pattern acceleration
            stepper_x->moveTo(target_steps);
        }
        if (stepper_y_left) {
            stepper_y_left->setSpeedInHz(patternYSpeed);
            stepper_y_left->setAcceleration(patternYAccel / 5.0); // Use HALF pattern acceleration
            stepper_y_left->moveTo(target_steps);
        }
        if (stepper_y_right) {
            stepper_y_right->setSpeedInHz(patternYSpeed);
            stepper_y_right->setAcceleration(patternYAccel / 5.0); // Use HALF pattern acceleration
            stepper_y_right->moveTo(target_steps);
        }
        
        // DIAGNOSTIC: Print positions BEFORE move-away
        Serial.printf("*** Positions before move-away: X=%ld, YL=%ld, YR=%ld ***\n", 
                      (stepper_x ? stepper_x->getCurrentPosition() : -1),
                      (stepper_y_left ? stepper_y_left->getCurrentPosition() : -1),
                      (stepper_y_right ? stepper_y_right->getCurrentPosition() : -1));

        // Wait for the move-away to complete
        // Serial.println("Waiting for move away from home...");
        while ((stepper_x && stepper_x->isRunning()) || 
               (stepper_y_left && stepper_y_left->isRunning()) || 
               (stepper_y_right && stepper_y_right->isRunning())) {
            webSocket.loop(); // Keep responsive
            yield();
        }
        // Serial.println("Move away complete.");
        
        allHomed = true;
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"All axes homed successfully.\"}");
    } else {
        // Serial.println("ERROR: Homing Failed!");
        String failedAxes = "";
        if (!x_homed) failedAxes += "X ";
        if (!y_left_homed) failedAxes += "Y-Left ";
        if (!y_right_homed) failedAxes += "Y-Right ";
        if (!z_homed) failedAxes += "Z";
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Homing Failed for: " + failedAxes + "\"}");
        allHomed = false;
    }

    isHoming = false;
}

// --- Movement Logic ---
void moveToPositionInches(float targetX_inch, float targetY_inch, float targetZ_inch) {
    if (inCalibrationMode) {
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot perform general move while in Calibration mode.\"}");
         return;
    }
    if (!allHomed) {
        // Serial.println("Error: Machine not homed. Please home first.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Machine not homed. Please home first.\"}");
        return;
    }
    if (isMoving || isHoming) {
         // Serial.println("Error: Machine is busy.");
         webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Machine is already moving or homing.\"}");
         return;
    }
    if (inPickPlaceMode) {
        // Disallow general moves while in PnP mode (specific PnP moves should be handled separately)
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot perform general move while in Pick/Place mode.\"}");
        return;
    }

    isMoving = true;
    // Serial.printf("Moving to X:%.2f, Y:%.2f, Z:%.2f inches\\n", targetX_inch, targetY_inch, targetZ_inch);
    String msg = "{\"status\":\"Moving\", \"message\":\"Moving to X:" + String(targetX_inch, 2) +
                 ", Y:" + String(targetY_inch, 2) + ", Z:" + String(targetZ_inch, 2) + "\"}";
    webSocket.broadcastTXT(msg);


    // Convert inches to steps
    long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
    long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);
    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z);

    // === Z Limit Check (only for Pick & Place mode) ===
    if (inPickPlaceMode) {
        // Only apply limits in Pick & Place mode
        long min_z_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z);
        long max_z_steps = (long)(Z_HOME_POS_INCH * STEPS_PER_INCH_Z); // Should be 0
        targetZ_steps = max(min_z_steps, targetZ_steps); // Ensure not below min travel
        targetZ_steps = min(max_z_steps, targetZ_steps); // Ensure not above home position (0)
    }
    // === End Z Limit Check ===

    // Move Z axis first (if necessary)
    bool needToMoveZ = (stepper_z && stepper_z->getCurrentPosition() != targetZ_steps);
    if (needToMoveZ) {
        stepper_z->setSpeedInHz(patternZSpeed);
        stepper_z->setAcceleration(patternZAccel);
        stepper_z->moveTo(targetZ_steps);
        // Serial.printf("  Moving Z to %ld steps\\n", targetZ_steps);
    }

    // Wait for Z to finish if it moved
    while (needToMoveZ && stepper_z->isRunning()) {
        webSocket.loop(); // Keep websocket alive
        yield();
    }
    // Serial.println("  Z move complete (or skipped).");


    // Move X and Y axes simultaneously
    bool needToMoveX = (stepper_x && stepper_x->getCurrentPosition() != targetX_steps);
    bool needToMoveY = (stepper_y_left && stepper_y_left->getCurrentPosition() != targetY_steps) ||
                       (stepper_y_right && stepper_y_right->getCurrentPosition() != targetY_steps);

    if (needToMoveX) {
        stepper_x->setSpeedInHz(patternXSpeed);
        stepper_x->setAcceleration(patternXAccel);
        stepper_x->moveTo(targetX_steps);
        // Serial.printf("  Moving X to %ld steps\\n", targetX_steps);
    }
    if (needToMoveY) {
        if (stepper_y_left) {
             stepper_y_left->setSpeedInHz(patternYSpeed);
             stepper_y_left->setAcceleration(patternYAccel);
             stepper_y_left->moveTo(targetY_steps);
             // Serial.printf("  Moving Y Left to %ld steps\\n", targetY_steps);
        }
       if (stepper_y_right) {
             stepper_y_right->setSpeedInHz(patternYSpeed);
             stepper_y_right->setAcceleration(patternYAccel);
             stepper_y_right->moveTo(targetY_steps);
             // Serial.printf("  Moving Y Right to %ld steps\\n", targetY_steps);
       }
    }

    // Wait for X and Y to complete (this part is checked in the main loop now)
}

// Function to move only X and Y axes to a target position in inches
// Assumes Z is already at the desired height
// IMPORTANT: This is for GENERAL moves, not PnP moves.
// PnP moves use moveToXYPositionInches_PnP() in PickPlace.cpp
void moveToXYPositionInches(float targetX_inch, float targetY_inch) {
    // Check if steppers exist
    if (!stepper_x || !stepper_y_left || !stepper_y_right) {
        // Serial.println("ERROR: Cannot move XY - Steppers not initialized.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"XY Steppers not initialized.\"}");
        return;
    }

    // Convert inches to steps
    long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
    long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);

    // Check if already at target
    bool x_at_target = (stepper_x->getCurrentPosition() == targetX_steps);
    bool y_at_target = (stepper_y_left->getCurrentPosition() == targetY_steps) && (stepper_y_right->getCurrentPosition() == targetY_steps);

    if (x_at_target && y_at_target) {
        // Serial.println("Already at target XY position.");
        return; // No move needed
    }

    // Set speeds and accelerations
    if (!x_at_target) {
        stepper_x->setSpeedInHz(patternXSpeed);
        stepper_x->setAcceleration(patternXAccel);
        stepper_x->moveTo(targetX_steps);
    }
    if (!y_at_target) {
        stepper_y_left->setSpeedInHz(patternYSpeed);
        stepper_y_left->setAcceleration(patternYAccel);
        stepper_y_left->moveTo(targetY_steps);

        stepper_y_right->setSpeedInHz(patternYSpeed);
        stepper_y_right->setAcceleration(patternYAccel);
        stepper_y_right->moveTo(targetY_steps);
    }
    // The isMoving flag should be set by the caller, and completion detected in loop()
}


// --- Movement Helpers for Painting ---

// Moves only the Z axis to the target position in inches, waits for completion
void moveZToPositionInches(float targetZ_inch, float speedHz, float accel) {
    // Check if machine is busy or in a conflicting mode
    if (isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
        Serial.printf("[ERROR] moveZToPositionInches denied: Machine state conflict (isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d)\n", isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot move Z, machine busy or in special mode.\"}");
        return;
    }

    if (!stepper_z) {
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Z Stepper not initialized.\"}");
         return;
    }

    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z);
    
    // === Z Limit Check (only for Pick & Place mode) ===
    if (inPickPlaceMode) {
        // Only apply limits in Pick & Place mode
        long min_z_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z);
        long max_z_steps = (long)(Z_HOME_POS_INCH * STEPS_PER_INCH_Z); // Should be 0
        targetZ_steps = max(min_z_steps, targetZ_steps); // Ensure not below min travel
        targetZ_steps = min(max_z_steps, targetZ_steps); // Ensure not above home position (0)
    }
    // === End Z Limit Check ===

    long currentZ_steps = stepper_z->getCurrentPosition();

    if (targetZ_steps == currentZ_steps) {
        // Serial.println("Z axis already at target.");
        return; // Already there
    }

    // Temporarily set moving flag for this blocking move
    isMoving = true;
    // Serial.printf("Moving Z from %.2f to %.2f inches (Steps: %ld to %ld, Speed: %.0f, Accel: %.0f)\n",
    //               (float)currentZ_steps / STEPS_PER_INCH_Z, targetZ_inch, currentZ_steps, targetZ_steps, speedHz, accel);

    stepper_z->setSpeedInHz(speedHz);
    stepper_z->setAcceleration(accel);
    stepper_z->moveTo(targetZ_steps);

    // Wait for Z move completion (blocking)
    while (stepper_z->isRunning()) {
        webSocket.loop(); // Keep websocket alive
        yield();
    }
    // Serial.println("Z move complete.");
    isMoving = false; // Clear flag after blocking move completes
}

// Moves X and Y axes to the target position using specified speed/accel for painting, waits for completion
void moveToXYPositionInches_Paint(float targetX_inch, float targetY_inch, float speedHz, float accel) {
    // No state checking here, assumes caller (paintSide) manages state
    long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
    long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);

    long currentX_steps = stepper_x->getCurrentPosition();
    long currentY_steps_L = stepper_y_left->getCurrentPosition();
    long currentY_steps_R = stepper_y_right->getCurrentPosition(); // Assuming right follows left

    bool x_at_target = (currentX_steps == targetX_steps);
    bool y_at_target = (currentY_steps_L == targetY_steps); // Assume right motor follows

    if (x_at_target && y_at_target) {
        // Serial.println("Already at target paint XY.");
        return; // No move needed
    }

    // Serial.printf("Painting move XY to (%.2f, %.2f) inches (Steps: X %ld, Y %ld, Speed: %.0f, Accel: %.0f)\n",
    //               targetX_inch, targetY_inch, targetX_steps, targetY_steps, speedHz, accel);

    if (!x_at_target) {
        stepper_x->setSpeedInHz(speedHz); // Use provided speed
        stepper_x->setAcceleration(accel); // Use provided accel
        stepper_x->moveTo(targetX_steps);
    }
    if (!y_at_target) {
        // Assuming Y speed/accel are the same for painting moves
        stepper_y_left->setSpeedInHz(speedHz);
        stepper_y_left->setAcceleration(accel);
        stepper_y_left->moveTo(targetY_steps);

        stepper_y_right->setSpeedInHz(speedHz);
        stepper_y_right->setAcceleration(accel);
        stepper_y_right->moveTo(targetY_steps);
    }

    // Wait for XY move completion (blocking for simplicity in painting sequence)
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        yield();
    }
    // Serial.println("Paint XY move complete.");
}


// --- Painting Logic ---

void paintSide(int sideIndex) {
    Serial.printf("Starting paintSide for side %d\n", sideIndex);

    // 1. Check Machine State
    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
        Serial.printf("[ERROR] paintSide denied: Invalid state (allHomed=%d, isMoving=%d, isHoming=%d, inPickPlaceMode=%d, inCalibrationMode=%d)\n",
                      allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot start painting, invalid machine state.\"}");
        return;
    }

    // 2. Set Machine Busy State
    isMoving = true; // Use isMoving flag to indicate painting activity
    char busyMsg[100];
    sprintf(busyMsg, "{\"status\":\"Busy\", \"message\":\"Painting Side %d...\"}", sideIndex);
    webSocket.broadcastTXT(busyMsg);
    Serial.println(busyMsg); // Debug

    // 3. Get Side Parameters
    float zHeight = paintZHeight_inch[sideIndex];
    int pitch = paintPitchAngle[sideIndex];
    int roll = paintRollAngle[sideIndex];
    float speed = paintSpeed[sideIndex]; // Speed in steps/s
    // For now, use pattern accel, maybe add paintAccel later
    float accel = patternXAccel; // Assuming X/Y use same accel for painting

    // 4. Optional: Rotate (Example: Rotate to 0 degrees for Side 0)
    if (sideIndex == 0 && stepper_rot) {
        Serial.println("Rotating to 0 degrees for Side 0 painting...");
        long targetRotSteps = 0; // Assuming 0 steps = 0 degrees
        if (stepper_rot->getCurrentPosition() != targetRotSteps) {
            stepper_rot->setSpeedInHz(patternZSpeed); // Use a reasonable speed/accel for rotation
            stepper_rot->setAcceleration(patternZAccel);
            stepper_rot->moveTo(targetRotSteps);
            while (stepper_rot->isRunning()) {
                yield();
            }
            Serial.println("Rotation complete.");
        } else {
            Serial.println("Already at 0 degrees.");
        }
        delay(100); // Small delay after rotation
    }
     // Add logic for other sides if needed

    // 5. Set Servo Angles
    Serial.printf("Setting servos: Pitch=%d, Roll=%d\n", pitch, roll);
    servo_pitch.write(pitch);
    servo_roll.write(roll);
    delay(300); // Allow servos to settle

    // 6. Define Path Start Point (Absolute PnP Drop Off Location)
    float pathStartX = placeFirstXAbsolute_inch;
    float pathStartY = placeFirstYAbsolute_inch;
    Serial.printf("Path Start (PnP Drop Off): (%.2f, %.2f)\n", pathStartX, pathStartY);
    Serial.printf("Using Gun Offset: (%.2f, %.2f)\n", paintGunOffsetX_inch, paintGunOffsetY_inch);

    // 7. Move to Start Z Height
    Serial.println("Moving to start Z height...");
    moveZToPositionInches(zHeight, patternZSpeed, patternZAccel);

    // 8. Calculate and Move TCP to START of FIRST SWEEP (Top-Right Corner)
    float startTcpX = pathStartX + paintGunOffsetX_inch + 0.25; // Start 0.25 inches to the right
    float startTcpY = pathStartY + paintGunOffsetY_inch;
    Serial.printf("Moving TCP to start of first sweep (Top-Right): (%.2f, %.2f)\n", startTcpX, startTcpY);
    moveToXYPositionInches_Paint(startTcpX, startTcpY, speed, accel);

    // 9. Execute Painting Path (Sweep Left/Down/Sweep Right/Down...)
    Serial.println("Starting painting path...");
    float currentTcpX = startTcpX;
    float currentTcpY = startTcpY;

    // Special pattern for back side (side 0)
    if (sideIndex == 0) {
        float rowSpacing = 3.0 + placeGapY_inch; // Move down by 3 inches + y gap
        Serial.printf("Back side painting: Using row spacing of %.2f (3.0 + gap %.2f)\n", rowSpacing, placeGapY_inch);
        
        // Use roll angle to determine painting orientation
        bool isVertical = (roll == ROLL_VERTICAL);
        Serial.printf("Back side orientation: %s (roll=%d)\n", isVertical ? "VERTICAL" : "HORIZONTAL", roll);
        
        if (isVertical) {
            // VERTICAL ORIENTATION - Sweep horizontally, move down between rows
            bool movingLeft = true; // Start by moving left (negative X)
            
            for (int r = 0; r < placeGridRows; ++r) {
                // Calculate Y position for this row
                float targetTcpY = startTcpY - (r * rowSpacing);
                
                // Move to correct Y position if needed
                if (abs(currentTcpY - targetTcpY) > 0.001) {
                    moveToXYPositionInches_Paint(currentTcpX, targetTcpY, speed, accel);
                    currentTcpY = targetTcpY;
                }
                
                // Calculate target X based on direction
                float targetTcpX;
                if (movingLeft) {
                    // Moving left (negative X direction)
                    targetTcpX = startTcpX - trayWidth_inch;
                    Serial.printf("Row %d: Sweeping Left from X=%.2f to X=%.2f at Y=%.2f\n", 
                                r, currentTcpX, targetTcpX, targetTcpY);
                } else {
                    // Moving right (positive X direction)
                    targetTcpX = startTcpX;
                    Serial.printf("Row %d: Sweeping Right from X=%.2f to X=%.2f at Y=%.2f\n", 
                                r, currentTcpX, targetTcpX, targetTcpY);
                }
                
                // Execute the sweep
                moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
                currentTcpX = targetTcpX;
                
                // Flip direction for next row
                movingLeft = !movingLeft;
            }
        } else {
            // HORIZONTAL ORIENTATION - Sweep vertically, move right between columns
            bool movingDown = true; // Start by moving down (negative Y)
            
            for (int c = 0; c < placeGridCols; ++c) {
                // Calculate X position for this column
                float targetTcpX = startTcpX - (c * (trayWidth_inch / (placeGridCols - 1)));
                
                // Move to correct X position if needed
                if (abs(currentTcpX - targetTcpX) > 0.001) {
                    moveToXYPositionInches_Paint(targetTcpX, currentTcpY, speed, accel);
                    currentTcpX = targetTcpX;
                }
                
                // Calculate target Y based on direction
                float targetTcpY;
                if (movingDown) {
                    // Moving down (negative Y direction)
                    targetTcpY = startTcpY - (placeGridRows - 1) * rowSpacing;
                    Serial.printf("Column %d: Sweeping Down from Y=%.2f to Y=%.2f at X=%.2f\n", 
                                c, currentTcpY, targetTcpY, targetTcpX);
                } else {
                    // Moving up (positive Y direction)
                    targetTcpY = startTcpY;
                    Serial.printf("Column %d: Sweeping Up from Y=%.2f to Y=%.2f at X=%.2f\n", 
                                c, currentTcpY, targetTcpY, targetTcpX);
                }
                
                // Execute the sweep
                moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
                currentTcpY = targetTcpY;
                
                // Flip direction for next column
                movingDown = !movingDown;
            }
        }
    } else {
        // Original pattern for other sides
    for (int r = 0; r < placeGridRows; ++r) {
        float targetTcpX;
        // Corrected Y calculation: Subtract spacing to move down
            float targetTcpY = (pathStartY - r * placeGapY_inch) + paintGunOffsetY_inch;

        // --- Determine Sweep Target X --- 
        if (r % 2 == 0) { // Even row (0, 2, ...): Sweep Left
                targetTcpX = (pathStartX - (placeGridCols - 1) * placeGapX_inch) + paintGunOffsetX_inch;
            Serial.printf("Row %d (Even): Sweeping Left to TCP X=%.2f, Y=%.2f\n", r, targetTcpX, targetTcpY);
        } else { // Odd row (1, 3, ...): Sweep Right
            targetTcpX = pathStartX + paintGunOffsetX_inch;
            Serial.printf("Row %d (Odd): Sweeping Right to TCP X=%.2f, Y=%.2f\n", r, targetTcpX, targetTcpY);
        }
        
        // --- Execute Horizontal Sweep --- 
        // Only move if Y is already correct (it should be from previous step or initial move)
        if (abs(currentTcpY - targetTcpY) < 0.001) { // Check if Y is already correct
             moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
             currentTcpX = targetTcpX; // Update current X
        } else {
             Serial.printf("[WARN] Row %d: Y position mismatch before sweep (Current: %.3f, Target: %.3f). Skipping sweep, moving directly down.\n", r, currentTcpY, targetTcpY);
             // If Y is wrong, just move to the correct Y at the *end* X of this sweep
             moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
             currentTcpX = targetTcpX;
             currentTcpY = targetTcpY; // Update Y as well
        }

        // --- Move Down to Next Row (if not the last row) --- 
        if (r < placeGridRows - 1) {
            // Corrected Y calculation: Subtract spacing to move down
                float nextRowTcpY = (pathStartY - (r + 1) * placeGapY_inch) + paintGunOffsetY_inch;
            Serial.printf("Row %d: Moving Down to Y=%.2f (X=%.2f)\n", r, nextRowTcpY, currentTcpX);
            // Move only Y axis - use moveZToPositionInches for XY? No, use XY function but keep X same.
            // Need a specific Y-only move or use XY with same X
            moveToXYPositionInches_Paint(currentTcpX, nextRowTcpY, speed, accel); // Move Y while keeping X constant
            currentTcpY = nextRowTcpY; // Update current Y
        }
    }
    }
    
    Serial.println("Painting path complete.");

    // 10. Move Z Axis Up (e.g., to safe height 0)
    Serial.println("Moving Z axis up to safe height (0)...");
    moveZToPositionInches(0.0, patternZSpeed, patternZAccel);

    // 11. Return to home position (X=0, Y=0, Z=0) << ADDED
    Serial.println("Returning to home position (X=0, Y=0, Z=0)...");
    // Use moveToPositionInches, which includes state checks and updates.
    // We need to temporarily clear isMoving so moveToPositionInches doesn't block itself.
    isMoving = false; 
    moveToPositionInches(0.0, 0.0, 0.0);
    // Wait for the return-to-home move to complete
    isMoving = true; // Set moving flag again for the wait loop
    while ((stepper_x && stepper_x->isRunning()) || (stepper_y_left && stepper_y_left->isRunning()) || (stepper_y_right && stepper_y_right->isRunning()) || (stepper_z && stepper_z->isRunning())) {
        webSocket.loop(); // Keep responsive
        yield();
    }
    Serial.println("Return to home complete.");

    // 12. Clear Busy State & Send Ready Message
    isMoving = false;
    char readyMsg[100];
    sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Painting Side %d complete. Returned to home.\"}", sideIndex);
    webSocket.broadcastTXT(readyMsg);
    Serial.println(readyMsg);

    sendCurrentPositionUpdate(); // Send final position
}

// --- Web Server and WebSocket Logic ---

void handleRoot() {
  // Serial.println("Serving root page.");
  webServer.send(200, "text/html", HTML_PROGMEM); // Use HTML_PROGMEM from html_content.h
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            // Send initial status and all settings using the helper function
            String welcomeMsg = "Welcome! Connected to Paint + PnP Machine.";
            sendAllSettingsUpdate(num, welcomeMsg);
            
            // Also send initial position update immediately after settings if homed
            if (allHomed) {
                 sendCurrentPositionUpdate();
             }
            }
            break;
        case WStype_TEXT:
            // Serial.printf("[%u] get Text: %s\n", num, payload); // Reduced verbosity
            if (length > 0) {
                String command = String((char*)payload);
                Serial.printf("[DEBUG] WebSocket [%u] Received Command: %s\n", num, command.c_str()); // DEBUG
                
                // --- NEW: Request for current status ---
                if (command == "GET_STATUS") { 
                    Serial.printf("[%u] WebSocket Received: GET_STATUS request.\n", num);
                    sendAllSettingsUpdate(num, "Current status requested"); // Send update only to requesting client
                }
                // --- End NEW ---
                // --- STOP Command Handler --- << ADDED
                else if (command == "STOP") {
                    Serial.println("WebSocket: Received STOP command.");
                    // Force stop all motors immediately
                    if(stepper_x) stepper_x->forceStop();
                    if(stepper_y_left) stepper_y_left->forceStop();
                    if(stepper_y_right) stepper_y_right->forceStop();
                    if(stepper_z) stepper_z->forceStop();
                    if(stepper_rot) stepper_rot->forceStop();
                    
                    // Reset state flags
                    isMoving = false;
                    isHoming = false;
                    if (inPickPlaceMode) exitPickPlaceMode(false); // Exit PnP without homing request
                    inCalibrationMode = false;
                    pendingHomingAfterPnP = false;
                    
                    // Send update
                    webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"STOPPED by user command.\"}");
                    sendCurrentPositionUpdate(); // Send final position after stop
                }
                // --- End STOP Command Handler ---
                
                else if (command == "HOME") { // Changed to else if
                    Serial.println("WebSocket: Received HOME command.");
                    if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot home, machine is busy.\"}");
                    } else {
                        // REMOVED block that checked inPickPlaceMode here.
                        // homeAllAxes() function will handle resetting PnP/Calibration modes internally.
                        Serial.println("[DEBUG] HOME command received while idle. Calling homeAllAxes()."); // Added log
                        homeAllAxes(); // Start homing (this function sends its own status updates and resets modes)
                    }
                } else if (command == "GOTO_5_5_0") {
                    Serial.println("WebSocket: Received GOTO_5_5_0 command.");
                    moveToPositionInches(5.0, 5.0, 0.0); // This function now sends its own status updates
                } else if (command == "GOTO_20_20_0") {
                    Serial.println("WebSocket: Received GOTO_20_20_0 command.");
                    moveToPositionInches(20.0, 20.0, 0.0); // This function now sends its own status updates
                } else if (command == "ENTER_PICKPLACE") {
                     Serial.println("WebSocket: Received ENTER_PICKPLACE command.");
                     if (!allHomed) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Not homed."); // DEBUG
                         // Fixed quotes
                         webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Machine not homed.\"}");
                     } else if (isMoving || isHoming) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Busy."); // DEBUG
                         // Fixed quotes
                         webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Machine is busy.\"}");
                     } else if (inPickPlaceMode) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Already in PnP mode."); // DEBUG
                         // Exit command should be used instead
                         webSocket.sendTXT(num, "{\"status\":\"PickPlaceReady\", \"message\":\"Already in Pick/Place mode. Use Exit button.\"}");
                     } else if (inCalibrationMode) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: In Calibration mode."); // DEBUG
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Exit calibration before entering PnP mode.\"}");
                     } else {
                         // Call the function defined in PickPlace.h
                         enterPickPlaceMode();
                     }
                 } else if (command == "EXIT_PICKPLACE") {
                     Serial.println("WebSocket: Received EXIT_PICKPLACE command.");
                     exitPickPlaceMode(true); // Call function from PickPlace.h, REQUEST homing after exit
                 } else if (command == "PNP_NEXT_STEP") { // Renamed command
                    Serial.println("WebSocket: Received PNP_NEXT_STEP command.");
                    executeNextPickPlaceStep(); // Call function from PickPlace.h
                 } else if (command == "PNP_SKIP_LOCATION") { // ADDED
                    Serial.println("WebSocket: Received PNP_SKIP_LOCATION command.");
                    skipPickPlaceLocation(); // Call function from PickPlace.h
                 } else if (command == "PNP_BACK_LOCATION") { // ADDED
                    Serial.println("WebSocket: Received PNP_BACK_LOCATION command.");
                    goBackPickPlaceLocation(); // Call function from PickPlace.h
                 } else if (command.startsWith("SET_PNP_OFFSET ")) {
                     Serial.println("WebSocket: Received SET_PNP_OFFSET command.");
                     if (isMoving || isHoming || inPickPlaceMode) {
                         Serial.println("[DEBUG] SET_PNP_OFFSET denied: Machine busy or in PnP mode.");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set offset while machine is busy or in PnP mode.\"}");
                     } else {
                         float newX, newY;
                         // Use C-style string for sscanf
                         int parsed = sscanf(command.c_str() + strlen("SET_PNP_OFFSET "), "%f %f", &newX, &newY);
                         if (parsed == 2) {
                             pnpOffsetX_inch = newX;
                             pnpOffsetY_inch = newY;
                             // Save to Preferences
                             preferences.begin("machineCfg", false);
                             preferences.putFloat("pnpOffX", pnpOffsetX_inch);
                             preferences.putFloat("pnpOffY", pnpOffsetY_inch);
                             preferences.end();
                             Serial.printf("[DEBUG] Set PnP Offset to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch);
                             // Send confirmation with all current settings
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"PnP offset updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Failed to parse SET_PNP_OFFSET values.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_PNP_OFFSET. Use: SET_PNP_OFFSET X Y\"}");
                         }
                     }
                 } else if (command.startsWith("SET_SPEED_ACCEL ")) {
                     Serial.println("WebSocket: Received SET_SPEED_ACCEL command.");
                     if (isMoving || isHoming) { // Allow setting even in PnP mode if idle? Maybe not best idea. Only allow when not moving/homing.
                         Serial.println("[DEBUG] SET_SPEED_ACCEL denied: Machine busy.");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set speed/accel while machine is busy.\"}");
                     } else {
                         float receivedXS, receivedXA, receivedYS, receivedYA;
                         // Use C-style string for sscanf - receive actual values (e.g., 20000)
                         int parsed = sscanf(command.c_str() + strlen("SET_SPEED_ACCEL "), "%f %f %f %f", &receivedXS, &receivedXA, &receivedYS, &receivedYA);
                         // Check if parsing was successful AND values are positive
                         if (parsed == 4 && receivedXS > 0 && receivedXA > 0 && receivedYS > 0 && receivedYA > 0) {
                            // NOTE: Assuming the JS now sends the actual values (e.g. 20000)
                            // If JS was sending the displayed values (e.g. 20), we would multiply here:
                            // patternXSpeed = receivedXS * 1000.0;
                            // patternXAccel = receivedXA * 1000.0;
                            // patternYSpeed = receivedYS * 1000.0;
                            // patternYAccel = receivedYA * 1000.0;

                             // Update variables directly with received actual values
                             patternXSpeed = receivedXS;
                             patternXAccel = receivedXA;
                             patternYSpeed = receivedYS;
                             patternYAccel = receivedYA;

                             // Save Speed/Accel to Preferences
                             preferences.begin("machineCfg", false); 
                             preferences.putFloat("patXSpd", patternXSpeed); 
                             preferences.putFloat("patXAcc", patternXAccel); 
                             preferences.putFloat("patYSpd", patternYSpeed); 
                             preferences.putFloat("patYAcc", patternYAccel); 
                             preferences.end();
                             Serial.printf("[DEBUG] Saved Speed/Accel to NVS: X(S:%.0f, A:%.0f), Y(S:%.0f, A:%.0f)\n", patternXSpeed, patternXAccel, patternYSpeed, patternYAccel);

                             // Send confirmation with all current settings (using actual values)
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"Speed/Accel updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Failed to parse SET_SPEED_ACCEL values or values invalid.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format/values for SET_SPEED_ACCEL. Use: SET_SPEED_ACCEL XS XA YS YA (all positive)\"}");
                         }
                     }
                 } else if (command.startsWith("SET_FIRST_PLACE_ABS ")) { // UPDATED
                         Serial.println("[DEBUG] webSocketEvent: Handling SET_FIRST_PLACE_ABS command."); // DEBUG
                         float newX, newY;
                    // Expecting "SET_FIRST_PLACE_ABS X_val Y_val"
                    int parsed = sscanf(command.c_str() + strlen("SET_FIRST_PLACE_ABS "), "%f %f", &newX, &newY);
                         if (parsed == 2) {
                        placeFirstXAbsolute_inch = newX;
                        placeFirstYAbsolute_inch = newY;
                             // Save to Preferences
                             preferences.begin("machineCfg", false);
                        preferences.putFloat("placeFirstXAbs", placeFirstXAbsolute_inch);
                        preferences.putFloat("placeFirstYAbs", placeFirstYAbsolute_inch);
                             preferences.end();
                        Serial.printf("[DEBUG] Set First Place Absolute to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
                             // Send confirmation with all current settings
                        char msgBuffer[400]; // Increased size
                        sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"First Place Absolute Pos updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                            webSocket.broadcastTXT(msgBuffer);
                         } else {
                        Serial.printf("[DEBUG] Failed to parse SET_FIRST_PLACE_ABS command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_FIRST_PLACE_ABS format.\"}");
                    }
                 }
                 // --- Calibration Commands ---
                 else if (command == "ENTER_CALIBRATION") {
                    Serial.println("WebSocket: Received ENTER_CALIBRATION command.");
                    if (!allHomed) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine must be homed before entering calibration.\"}");
                    } else if (isMoving || isHoming || inPickPlaceMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine busy. Cannot enter calibration.\"}");
                 } else {
                        inCalibrationMode = true;
                        webSocket.broadcastTXT("{\"status\":\"CalibrationActive\", \"message\":\"Calibration mode entered.\"}");
                        sendCurrentPositionUpdate(); // Send initial position
                    }
                 } else if (command == "EXIT_CALIBRATION") {
                    Serial.println("WebSocket: Received EXIT_CALIBRATION command.");
                    inCalibrationMode = false;
                    // Send a standard Ready status update when exiting
                    webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Exited calibration mode.\"}");
                 } else if (command.startsWith("SET_OFFSET_FROM_CURRENT")) {
                    Serial.println("WebSocket: Received SET_OFFSET_FROM_CURRENT command.");
                     if (!inCalibrationMode) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode.\"}");
                     } else if (isMoving || isHoming) {
                         webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot set while moving.\"}");
                     } else {
                         if (stepper_x && stepper_y_left) { // Use Y-Left as reference
                            pnpOffsetX_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
                            pnpOffsetY_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
                            
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("pnpOffX", pnpOffsetX_inch);
                            preferences.putFloat("pnpOffY", pnpOffsetY_inch);
                            preferences.end();
                            
                            Serial.printf("[DEBUG] Set PnP Offset from current pos to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch);
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"PnP Offset set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                              webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Steppers not available.\"}");
                         }
                     }
                 } else if (command == "SET_FIRST_PLACE_ABS_FROM_CURRENT") { // UPDATED
                     if (!inCalibrationMode) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in Calibration Mode to set First Place position from current.\"}");
                     } else {
                         if (stepper_x && stepper_y_left) { // Use Y-Left as reference
                             placeFirstXAbsolute_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
                             placeFirstYAbsolute_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
                            
                             preferences.begin("machineCfg", false);
                             preferences.putFloat("placeFirstXAbs", placeFirstXAbsolute_inch);
                             preferences.putFloat("placeFirstYAbs", placeFirstYAbsolute_inch);
                             preferences.end();
                            
                             Serial.printf("[DEBUG] Set First Place Absolute from current pos to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
                              char msgBuffer[400]; // Increased size
                               sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"First Place Absolute Pos set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                       pnpOffsetX_inch, pnpOffsetY_inch,
                                       placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                       patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                       placeGridCols, placeGridRows,
                                       placeGapX_inch, placeGapY_inch);
                              webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Stepper X or Y_Left not initialized when setting First Place Abs from current.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Internal stepper error.\"}");
                         }
                     }
                 } else if (command.startsWith("JOG ")) {
                    // Serial.println("WebSocket: Received JOG command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}");
                    } else {
                        char axis;
                        float distance_inch;
                        int parsed = sscanf(command.c_str() + strlen("JOG "), "%c %f", &axis, &distance_inch);
                        
                        if (parsed == 2) {
                            long current_steps = 0;
                            long jog_steps = 0;
                            FastAccelStepper* stepper_to_move = NULL;
                            float speed = 0, accel = 0;

                            if (axis == 'X' && stepper_x) {
                                stepper_to_move = stepper_x;
                                current_steps = stepper_x->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternXSpeed;
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\n", axis);
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                                
                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\n", axis, distance_inch, jog_steps, current_steps, target_steps);

                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);
                                stepper_to_move->moveTo(target_steps);

                                if (axis == 'Y' && stepper_y_right) { // Command right Y too
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps);
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y distance\"}");
                        }
                    }
                 } else if (command.startsWith("MOVE_TO_COORDS ")) {
                    Serial.println("WebSocket: Received MOVE_TO_COORDS command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to move to coordinates.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot move while machine is busy.\"}");
                    } else {
                        float targetX_inch, targetY_inch;
                        int parsed = sscanf(command.c_str() + strlen("MOVE_TO_COORDS "), "%f %f", &targetX_inch, &targetY_inch);
                        
                        if (parsed == 2) {
                            // Validate coordinates (optional, add bounds checking if needed)
                            if (targetX_inch < 0 || targetY_inch < 0) {
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid coordinates. Must be non-negative.\"}");
                                return;
                            }
                            
                            // Set up the move
                            isMoving = true; // Set flag, loop() will handle completion
                            webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Moving to coordinates...\"}");
                            
                            // Convert to steps
                            long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
                            long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);
                            
                            // Command the move
                            if (stepper_x) {
                                stepper_x->setSpeedInHz(patternXSpeed);
                                stepper_x->setAcceleration(patternXAccel);
                                stepper_x->moveTo(targetX_steps);
                            }
                            
                            if (stepper_y_left && stepper_y_right) {
                                stepper_y_left->setSpeedInHz(patternYSpeed);
                                stepper_y_left->setAcceleration(patternYAccel);
                                stepper_y_left->moveTo(targetY_steps);
                                
                                stepper_y_right->setSpeedInHz(patternYSpeed);
                                stepper_y_right->setAcceleration(patternYAccel);
                                stepper_y_right->moveTo(targetY_steps);
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse MOVE_TO_COORDS command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format. Use: MOVE_TO_COORDS X Y\"}");
                         }
                     }
                 } else if (command.startsWith("SET_GRID_SPACING ")) { // UPDATED
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_GRID_SPACING command."); // DEBUG
                        int newCols, newRows;
                    // Expecting "SET_GRID_SPACING C R"
                    int parsed = sscanf(command.c_str() + strlen("SET_GRID_SPACING "), "%d %d", &newCols, &newRows);
                    if (parsed == 2 && newCols > 0 && newRows > 0) {
                        // Call the new calculation function
                        calculateAndSetGridSpacing(newCols, newRows);
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_GRID_SPACING command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_GRID_SPACING format. Use C R (integers > 0)\"}");
                    }
                 } else if (command.startsWith("SET_TRAY_SIZE ")) { // NEW
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_TRAY_SIZE command."); // DEBUG
                    float newW, newH;
                    // Expecting "SET_TRAY_SIZE W H"
                    int parsed = sscanf(command.c_str() + strlen("SET_TRAY_SIZE "), "%f %f", &newW, &newH);
                    if (parsed == 2 && newW > 0 && newH > 0) {
                        Serial.printf("[DEBUG] New tray dimensions: width=%.2f, height=%.2f (old: width=%.2f, height=%.2f)\n", 
                                    newW, newH, trayWidth_inch, trayHeight_inch);
                        
                        // Save the new tray dimensions
                        trayWidth_inch = newW;
                        trayHeight_inch = newH;
                        
                        // Open preferences, save values, and close immediately
                        preferences.begin("machineCfg", false);
                        preferences.putFloat("trayWidth", trayWidth_inch);
                        preferences.putFloat("trayHeight", trayHeight_inch);
                        preferences.end();
                        
                        Serial.printf("[DEBUG] Set Tray Size to W=%.2f, H=%.2f\n", trayWidth_inch, trayHeight_inch);
                        
                        // Recalculate grid spacing with current grid dimensions to reflect new tray size
                        float oldGapX = placeGapX_inch;
                        float oldGapY = placeGapY_inch;
                        calculateAndSetGridSpacing(placeGridCols, placeGridRows);
                        
                        Serial.printf("[DEBUG] Gap values changed: X: %.3f → %.3f, Y: %.3f → %.3f\n", 
                                    oldGapX, placeGapX_inch, oldGapY, placeGapY_inch);
                        
                        // Instead of using a redundant call to sendAllSettingsUpdate, create a more specific message
                        // about the changes that were made
                        char msgBuffer[200];
                        sprintf(msgBuffer, "Tray Size updated to W=%.2f, H=%.2f. Gap recalculated: X=%.3f, Y=%.3f", 
                               trayWidth_inch, trayHeight_inch, placeGapX_inch, placeGapY_inch);
                        
                        // Send settings update to the specific client that requested the change
                        sendAllSettingsUpdate(num, msgBuffer);
                        // Also broadcast to all other clients
                        if (num != 255) {
                            sendAllSettingsUpdate(255, msgBuffer);
                        }
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_TRAY_SIZE command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_TRAY_SIZE format.\"}");
                    }
                 } else if (command.startsWith("SET_PAINT_PATTERN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_PATTERN_OFFSET command.");
                    if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_PATTERN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintPatternOffsetX_inch = newX;
                            paintPatternOffsetY_inch = newY;
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("paintPatOffX", paintPatternOffsetX_inch);
                            preferences.putFloat("paintPatOffY", paintPatternOffsetY_inch);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Paint Pattern Offset to X: %.2f, Y: %.2f\n", paintPatternOffsetX_inch, paintPatternOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Pattern Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_PATTERN_OFFSET format. Use: SET_PAINT_PATTERN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_GUN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_GUN_OFFSET command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_GUN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintGunOffsetX_inch = newX;
                            paintGunOffsetY_inch = newY;
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("paintGunOffX", paintGunOffsetX_inch);
                            preferences.putFloat("paintGunOffY", paintGunOffsetY_inch);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Paint Gun Offset to X: %.2f, Y: %.2f\n", paintGunOffsetX_inch, paintGunOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Gun Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_GUN_OFFSET format. Use: SET_PAINT_GUN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_SIDE_SETTINGS ")) {
                    Serial.println("WebSocket: Received SET_PAINT_SIDE_SETTINGS command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set side settings while machine is busy.\"}");
                    } else {
                        int sideIndex;
                        float newZ, newSpeed;
                        int newPitch, newRoll;
                        // Format: SET_PAINT_SIDE_SETTINGS sideIndex zHeight pitch roll speed
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_SIDE_SETTINGS "), "%d %f %d %d %f",
                                           &sideIndex, &newZ, &newPitch, &newRoll, &newSpeed);

                        if (parsed == 5 && sideIndex >= 0 && sideIndex < 4) {
                            // Validate UI pitch value first (0-90)
                            if (newPitch < 0 || newPitch > 90) {
                                Serial.printf("[ERROR] Invalid pitch value %d from UI (must be 0-90)\n", newPitch);
                                webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Pitch Angle for side " + String(sideIndex) + " must be between 0 and 90.\"}");
                                return;
                            }
                            
                            // Map the UI pitch value (0-90) to servo value (PITCH_SERVO_MIN to PITCH_SERVO_MAX)
                            // For example: 0 -> PITCH_SERVO_MIN, 90 -> PITCH_SERVO_MAX
                            newPitch = map(newPitch, 0, 90, PITCH_SERVO_MIN, PITCH_SERVO_MAX);
                            Serial.printf("[DEBUG] Mapped UI pitch %d to servo pitch %d\n", newPitch, newPitch);
                            
                            // Map the UI values (0=Vertical, 90=Horizontal) to the actual servo values
                            if (newRoll == 0) {
                                newRoll = ROLL_VERTICAL;  // Map 0 from UI to the actual vertical servo position
                            } else if (newRoll == 90) {
                                newRoll = ROLL_HORIZONTAL; // Map 90 from UI to the actual horizontal servo position
                        } else {
                                // For any unexpected values, default to vertical for safety
                                Serial.printf("[WARN] Invalid Roll value %d from UI, defaulting to VERTICAL\n", newRoll);
                                newRoll = ROLL_VERTICAL;
                            }
                            
                            newSpeed = constrain(newSpeed, 5000.0f, 20000.0f);

                            paintZHeight_inch[sideIndex] = newZ;
                            paintPitchAngle[sideIndex] = newPitch;
                            paintRollAngle[sideIndex] = newRoll;
                            paintSpeed[sideIndex] = newSpeed;

                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            String keyZ = "paintZ_" + String(sideIndex);
                            String keyP = "paintP_" + String(sideIndex);
                            String keyR = "paintR_" + String(sideIndex);
                            String keyS = "paintS_" + String(sideIndex);
                            preferences.putFloat(keyZ.c_str(), paintZHeight_inch[sideIndex]);
                            preferences.putInt(keyP.c_str(), paintPitchAngle[sideIndex]);
                            preferences.putInt(keyR.c_str(), paintRollAngle[sideIndex]);
                            preferences.putFloat(keyS.c_str(), paintSpeed[sideIndex]);
                            preferences.end();

                            Serial.printf("[DEBUG] Set Side %d Settings: Z=%.2f, P=%d, R=%d, S=%.0f\n",
                                          sideIndex, paintZHeight_inch[sideIndex], paintPitchAngle[sideIndex],
                                          paintRollAngle[sideIndex], paintSpeed[sideIndex]);

                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Side " + String(sideIndex) + " settings updated.");
                        } else {
                             Serial.printf("[ERROR] Failed to parse SET_PAINT_SIDE_SETTINGS. Parsed=%d, Side=%d\n", parsed, sideIndex);
                             webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_SIDE_SETTINGS format/values.\"}");
                        }
                    }
                 } else if (command.startsWith("JOG ")) {
                    // Serial.println("WebSocket: Received JOG command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}");
                    } else {
                        char axis;
                        float distance_inch;
                        int parsed = sscanf(command.c_str() + strlen("JOG "), "%c %f", &axis, &distance_inch);
                        
                        if (parsed == 2) {
                            long current_steps = 0;
                            long jog_steps = 0;
                            FastAccelStepper* stepper_to_move = NULL;
                            float speed = 0, accel = 0;

                            if (axis == 'X' && stepper_x) {
                                stepper_to_move = stepper_x;
                                current_steps = stepper_x->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternXSpeed;
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\n", axis);
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                                
                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\n", axis, distance_inch, jog_steps, current_steps, target_steps);

                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);
                                stepper_to_move->moveTo(target_steps);

                                if (axis == 'Y' && stepper_y_right) { // Command right Y too
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps);
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y distance\"}");
                        }
                    }
                 } else if (command.startsWith("MOVE_TO_COORDS ")) {
                    Serial.println("WebSocket: Received MOVE_TO_COORDS command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to move to coordinates.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot move while machine is busy.\"}");
                    } else {
                        float targetX_inch, targetY_inch;
                        int parsed = sscanf(command.c_str() + strlen("MOVE_TO_COORDS "), "%f %f", &targetX_inch, &targetY_inch);
                        
                        if (parsed == 2) {
                            // Validate coordinates (optional, add bounds checking if needed)
                            if (targetX_inch < 0 || targetY_inch < 0) {
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid coordinates. Must be non-negative.\"}");
                                return;
                            }
                            
                            // Set up the move
                            isMoving = true; // Set flag, loop() will handle completion
                            webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Moving to coordinates...\"}");
                            
                            // Convert to steps
                            long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
                            long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);
                            
                            // Command the move
                            if (stepper_x) {
                                stepper_x->setSpeedInHz(patternXSpeed);
                                stepper_x->setAcceleration(patternXAccel);
                                stepper_x->moveTo(targetX_steps);
                            }
                            
                            if (stepper_y_left && stepper_y_right) {
                                stepper_y_left->setSpeedInHz(patternYSpeed);
                                stepper_y_left->setAcceleration(patternYAccel);
                                stepper_y_left->moveTo(targetY_steps);
                                
                                stepper_y_right->setSpeedInHz(patternYSpeed);
                                stepper_y_right->setAcceleration(patternYAccel);
                                stepper_y_right->moveTo(targetY_steps);
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse MOVE_TO_COORDS command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format. Use: MOVE_TO_COORDS X Y\"}");
                         }
                     }
                 } else if (command.startsWith("SET_GRID_SPACING ")) { // UPDATED
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_GRID_SPACING command."); // DEBUG
                    int newCols, newRows;
                    // Expecting "SET_GRID_SPACING C R"
                    int parsed = sscanf(command.c_str() + strlen("SET_GRID_SPACING "), "%d %d", &newCols, &newRows);
                    if (parsed == 2 && newCols > 0 && newRows > 0) {
                        // Call the new calculation function
                        calculateAndSetGridSpacing(newCols, newRows);
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_GRID_SPACING command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_GRID_SPACING format. Use C R (integers > 0)\"}");
                    }
                 } else if (command.startsWith("SET_TRAY_SIZE ")) { // NEW
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_TRAY_SIZE command."); // DEBUG
                    float newW, newH;
                    // Expecting "SET_TRAY_SIZE W H"
                    int parsed = sscanf(command.c_str() + strlen("SET_TRAY_SIZE "), "%f %f", &newW, &newH);
                    if (parsed == 2 && newW > 0 && newH > 0) {
                        Serial.printf("[DEBUG] New tray dimensions: width=%.2f, height=%.2f (old: width=%.2f, height=%.2f)\n", 
                                    newW, newH, trayWidth_inch, trayHeight_inch);
                        
                        // Save the new tray dimensions
                        trayWidth_inch = newW;
                        trayHeight_inch = newH;
                        
                        // Open preferences, save values, and close immediately
                        preferences.begin("machineCfg", false);
                        preferences.putFloat("trayWidth", trayWidth_inch);
                        preferences.putFloat("trayHeight", trayHeight_inch);
                        preferences.end();
                        
                        Serial.printf("[DEBUG] Set Tray Size to W=%.2f, H=%.2f\n", trayWidth_inch, trayHeight_inch);
                        
                        // Recalculate grid spacing with current grid dimensions to reflect new tray size
                        float oldGapX = placeGapX_inch;
                        float oldGapY = placeGapY_inch;
                        calculateAndSetGridSpacing(placeGridCols, placeGridRows);
                        
                        Serial.printf("[DEBUG] Gap values changed: X: %.3f → %.3f, Y: %.3f → %.3f\n", 
                                    oldGapX, placeGapX_inch, oldGapY, placeGapY_inch);
                        
                        // Instead of using a redundant call to sendAllSettingsUpdate, create a more specific message
                        // about the changes that were made
                        char msgBuffer[200];
                        sprintf(msgBuffer, "Tray Size updated to W=%.2f, H=%.2f. Gap recalculated: X=%.3f, Y=%.3f", 
                               trayWidth_inch, trayHeight_inch, placeGapX_inch, placeGapY_inch);
                        
                        // Send settings update to the specific client that requested the change
                        sendAllSettingsUpdate(num, msgBuffer);
                        // Also broadcast to all other clients
                        if (num != 255) {
                            sendAllSettingsUpdate(255, msgBuffer);
                        }
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_TRAY_SIZE command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_TRAY_SIZE format.\"}");
                    }
                 } else if (command.startsWith("SET_PAINT_PATTERN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_PATTERN_OFFSET command.");
                    if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_PATTERN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintPatternOffsetX_inch = newX;
                            paintPatternOffsetY_inch = newY;
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("paintPatOffX", paintPatternOffsetX_inch);
                            preferences.putFloat("paintPatOffY", paintPatternOffsetY_inch);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Paint Pattern Offset to X: %.2f, Y: %.2f\n", paintPatternOffsetX_inch, paintPatternOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Pattern Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_PATTERN_OFFSET format. Use: SET_PAINT_PATTERN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_GUN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_GUN_OFFSET command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_GUN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintGunOffsetX_inch = newX;
                            paintGunOffsetY_inch = newY;
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("paintGunOffX", paintGunOffsetX_inch);
                            preferences.putFloat("paintGunOffY", paintGunOffsetY_inch);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Paint Gun Offset to X: %.2f, Y: %.2f\n", paintGunOffsetX_inch, paintGunOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Gun Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_GUN_OFFSET format. Use: SET_PAINT_GUN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_SIDE_SETTINGS ")) {
                    Serial.println("WebSocket: Received SET_PAINT_SIDE_SETTINGS command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set side settings while machine is busy.\"}");
                    } else {
                        int sideIndex;
                        float newZ, newSpeed;
                        int newPitch, newRoll;
                        // Format: SET_PAINT_SIDE_SETTINGS sideIndex zHeight pitch roll speed
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_SIDE_SETTINGS "), "%d %f %d %d %f",
                                           &sideIndex, &newZ, &newPitch, &newRoll, &newSpeed);

                        if (parsed == 5 && sideIndex >= 0 && sideIndex < 4) {
                            // Validate UI pitch value first (0-90)
                            if (newPitch < 0 || newPitch > 90) {
                                Serial.printf("[ERROR] Invalid pitch value %d from UI (must be 0-90)\n", newPitch);
                                webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Pitch Angle for side " + String(sideIndex) + " must be between 0 and 90.\"}");
                                return;
                            }
                            
                            // Map the UI pitch value (0-90) to servo value (PITCH_SERVO_MIN to PITCH_SERVO_MAX)
                            // For example: 0 -> PITCH_SERVO_MIN, 90 -> PITCH_SERVO_MAX
                            newPitch = map(newPitch, 0, 90, PITCH_SERVO_MIN, PITCH_SERVO_MAX);
                            Serial.printf("[DEBUG] Mapped UI pitch %d to servo pitch %d\n", newPitch, newPitch);
                            
                            // Map the UI values (0=Vertical, 90=Horizontal) to the actual servo values
                            if (newRoll == 0) {
                                newRoll = ROLL_VERTICAL;  // Map 0 from UI to the actual vertical servo position
                            } else if (newRoll == 90) {
                                newRoll = ROLL_HORIZONTAL; // Map 90 from UI to the actual horizontal servo position
                        } else {
                                // For any unexpected values, default to vertical for safety
                                Serial.printf("[WARN] Invalid Roll value %d from UI, defaulting to VERTICAL\n", newRoll);
                                newRoll = ROLL_VERTICAL;
                            }
                            
                            newSpeed = constrain(newSpeed, 5000.0f, 20000.0f);

                            paintZHeight_inch[sideIndex] = newZ;
                            paintPitchAngle[sideIndex] = newPitch;
                            paintRollAngle[sideIndex] = newRoll;
                            paintSpeed[sideIndex] = newSpeed;

                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            String keyZ = "paintZ_" + String(sideIndex);
                            String keyP = "paintP_" + String(sideIndex);
                            String keyR = "paintR_" + String(sideIndex);
                            String keyS = "paintS_" + String(sideIndex);
                            preferences.putFloat(keyZ.c_str(), paintZHeight_inch[sideIndex]);
                            preferences.putInt(keyP.c_str(), paintPitchAngle[sideIndex]);
                            preferences.putInt(keyR.c_str(), paintRollAngle[sideIndex]);
                            preferences.putFloat(keyS.c_str(), paintSpeed[sideIndex]);
                            preferences.end();

                            Serial.printf("[DEBUG] Set Side %d Settings: Z=%.2f, P=%d, R=%d, S=%.0f\n",
                                          sideIndex, paintZHeight_inch[sideIndex], paintPitchAngle[sideIndex],
                                          paintRollAngle[sideIndex], paintSpeed[sideIndex]);

                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Side " + String(sideIndex) + " settings updated.");
                        } else {
                             Serial.printf("[ERROR] Failed to parse SET_PAINT_SIDE_SETTINGS. Parsed=%d, Side=%d\n", parsed, sideIndex);
                             webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_SIDE_SETTINGS format/values.\"}");
                        }
                    }
                 } else if (command.startsWith("JOG ")) {
                    // Serial.println("WebSocket: Received JOG command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}");
                    } else {
                        char axis;
                        float distance_inch;
                        int parsed = sscanf(command.c_str() + strlen("JOG "), "%c %f", &axis, &distance_inch);
                        
                        if (parsed == 2) {
                            long current_steps = 0;
                            long jog_steps = 0;
                            FastAccelStepper* stepper_to_move = NULL;
                            float speed = 0, accel = 0;

                            if (axis == 'X' && stepper_x) {
                                stepper_to_move = stepper_x;
                                current_steps = stepper_x->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternXSpeed;
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\n", axis);
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                                
                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\n", axis, distance_inch, jog_steps, current_steps, target_steps);

                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);
                                stepper_to_move->moveTo(target_steps);

                                if (axis == 'Y' && stepper_y_right) { // Command right Y too
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps);
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y distance\"}");
                        }
                    }
                 } else if (command == "START_PAINTING") {
                    Serial.println("WebSocket: Received START_PAINTING command.");
                    // startPaintingSequence(); // Call the (placeholder) function // COMMENTED OUT FOR NOW
                 } else if (command.startsWith("ROTATE ")) {
                    Serial.println("WebSocket: Received ROTATE command.");
                    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("DEBUG: Cannot rotate - Machine busy or not in correct mode");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot rotate: Machine busy or not in correct mode.\"}");
                    } else if (!stepper_rot) {
                        Serial.println("DEBUG: Rotation stepper not available");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation stepper not available.\"}");
                    } else {
                        float degrees;
                        int parsed = sscanf(command.c_str() + strlen("ROTATE "), "%f", &degrees);
                        if (parsed == 1) {
                            // Get current angle before we start
                            float currentAngle = (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE;
                            // Calculate target angle
                            float targetAngle = currentAngle + degrees;
                            
                            // Round to nearest step to avoid floating point errors
                            long relative_steps = round(degrees * STEPS_PER_DEGREE);
                            
                            Serial.printf("DEBUG: Rotating by %.1f degrees (%.4f steps per degree, %ld total steps)\n", 
                                        degrees, STEPS_PER_DEGREE, relative_steps);
                            Serial.printf("DEBUG: Current angle: %.2f degrees, Target angle: %.2f degrees\n", 
                                        currentAngle, targetAngle);
                            Serial.printf("DEBUG: Current position before rotation: %ld steps\n", stepper_rot->getCurrentPosition());
                            
                            isMoving = true;
                            webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Rotating tray...\"}");
                            
                            // Use different speeds for large vs. small movements
                            if (abs(degrees) > 10.0) {
                                stepper_rot->setSpeedInHz(patternRotSpeed); // Use full speed for larger moves
                            } else {
                                stepper_rot->setSpeedInHz(patternRotSpeed / 2); // Use reduced speed for precision
                            }
                            stepper_rot->setAcceleration(patternRotAccel);
                            stepper_rot->move(relative_steps); // Use move() for relative rotation
                            
                            Serial.printf("DEBUG: move() called with %ld steps\n", relative_steps);
                        } else {
                            Serial.println("DEBUG: Invalid ROTATE format");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid ROTATE format. Use: ROTATE degrees\"}");
                        }
                    }
                 } else if (command.startsWith("JOG ROT ")) {
                     Serial.println("WebSocket: Received JOG ROT command.");
                     if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("DEBUG: Cannot jog rotation - Machine busy or not in correct mode");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot jog rotation: Machine busy or not in correct mode.\"}");
                     } else if (!stepper_rot) {
                        Serial.println("DEBUG: Rotation stepper not available");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation stepper not available.\"}");
                     } else {
                        float degrees;
                        int parsed = sscanf(command.c_str() + strlen("JOG ROT "), "%f", &degrees);
                        if (parsed == 1) {
                            // Get current angle before we start
                            float currentAngle = (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE;
                            // Calculate target angle
                            float targetAngle = currentAngle + degrees;
                            
                            // Round to nearest step to avoid floating point errors
                            long relative_steps = round(degrees * STEPS_PER_DEGREE);
                            
                            Serial.printf("DEBUG: Jogging rotation by %.1f degrees (%.4f steps per degree, %ld total steps)\n", 
                                        degrees, STEPS_PER_DEGREE, relative_steps);
                            Serial.printf("DEBUG: Current angle: %.2f degrees, Target angle: %.2f degrees\n", 
                                        currentAngle, targetAngle);
                            Serial.printf("DEBUG: Current position before jog: %ld steps\n", stepper_rot->getCurrentPosition());
                            
                            isMoving = true; // Set isMoving flag, let loop() handle completion
                            webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Jogging rotation...\"}");
                            
                            // Use reduced speed for precision jogging (consistent with ROTATE small moves)
                            stepper_rot->setSpeedInHz(patternRotSpeed / 2); // Half speed for precision
                            stepper_rot->setAcceleration(patternRotAccel); // Use full accel like ROTATE small moves
                            stepper_rot->move(relative_steps);
                            
                            Serial.printf("DEBUG: move() called with %ld steps\n", relative_steps);
                        } else {
                            Serial.println("DEBUG: Invalid JOG ROT format");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG ROT format. Use: JOG ROT degrees\"}");
                        }
                    }
                 } else if (command == "SET_ROT_ZERO") { // UPDATED COMMAND NAME
                    Serial.println("WebSocket: Received SET_ROT_ZERO command.");
                    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("[DEBUG] SET_ROT_ZERO denied: Invalid state.");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set zero while machine is busy or in special mode.\"}");
                    } else if (!stepper_rot) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation stepper not enabled.\"}");
                    } else {
                         Serial.println("[DEBUG] Setting current rotation position as zero.");
                         stepper_rot->setCurrentPosition(0);
                         // Save to Preferences? Not currently implemented
                         webSocket.sendTXT(num, "{\"status\":\"Ready\", \"message\":\"Current rotation set to zero.\"}");
                         sendCurrentPositionUpdate(); // Update display
                    }
                } else if (command == "PAINT_SIDE_0") { // <<< NEW COMMAND HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_0 command.");
                    paintSide(0); // Call the painting function for Side 0
                } else {
                    // Handle unknown commands
                    Serial.printf("[DEBUG] WebSocket [%u] Unknown command received: %s\n", num, command.c_str());
                    // Fixed quotes and backslashes
                    webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Unknown command\"}");
                }
            }
            break;
        case WStype_BIN:
            Serial.printf("[%u] get binary length: %u\n", num, length);
            // hexdump(payload, length);
            // echo data back to browser
            // webSocket.sendBIN(num, payload, length);
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

// Function to send current position via WebSocket
void sendCurrentPositionUpdate() {
    if (!allHomed) return; // Don't send if not homed

    float currentX_inch = 0.0;
    float currentY_inch = 0.0;
    float currentZ_inch = 0.0;
    // Rotation angle removed

    if (stepper_x) {
        currentX_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
    }
    if (stepper_y_left) { // Use Y-Left as reference for Y position
        currentY_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
    }
     if (stepper_z) {
        currentZ_inch = (float)stepper_z->getCurrentPosition() / STEPS_PER_INCH_Z;
    }
    // Rotation angle calculation removed

    char posBuffer[150]; // Buffer size reduced slightly
    // Removed rotAngle field from JSON
    sprintf(posBuffer, "{\"type\":\"positionUpdate\", \"posX\":%.3f, \"posY\":%.3f, \"posZ\":%.3f}", 
            currentX_inch, currentY_inch, currentZ_inch);
    webSocket.broadcastTXT(posBuffer);
    // Serial.printf("[DEBUG] Sent Position Update (No Angle): %s\n", posBuffer); // Optional debug
}

// Helper function to send ALL current settings
// NOTE: Also does not include rotation angle
void sendAllSettingsUpdate(uint8_t specificClientNum, String message) {
    // JsonDocument doc(2048); // Incorrect constructor for v7+
    JsonDocument doc; // Declare JsonDocument, let library manage memory

    // Base status and message
    doc["status"] = "Ready"; // Assume Ready unless overridden
    if (isMoving) doc["status"] = "Busy";
    if (isHoming) doc["status"] = "Homing";
    if (inPickPlaceMode) doc["status"] = "PickPlaceReady"; // More specific PnP status
    if (inCalibrationMode) doc["status"] = "CalibrationActive";
    // TODO: Add other relevant states if needed (e.g., painting)

    // ADD EXPLICIT HOMED STATUS FIELD
    doc["homed"] = allHomed; // Add explicit field for homed status

    doc["message"] = message;

    // System Limits (example)
    // doc["maxX"] = X_MAX_TRAVEL_POS_INCH;
    // doc["maxY"] = Y_MAX_TRAVEL_POS_INCH;
    // doc["maxZ"] = Z_HOME_POS_INCH;

    // Speed/Accel Settings (showing displayed values)
    doc["patXSpeed"] = patternXSpeed; // Send actual value
    doc["patXAccel"] = patternXAccel; // Send actual value
    doc["patYSpeed"] = patternYSpeed; // Send actual value
    doc["patYAccel"] = patternYAccel; // Send actual value

    // Pick and Place Settings
    doc["pnpOffsetX"] = pnpOffsetX_inch;
    doc["pnpOffsetY"] = pnpOffsetY_inch;
    doc["placeFirstXAbs"] = placeFirstXAbsolute_inch; // Key updated
    doc["placeFirstYAbs"] = placeFirstYAbsolute_inch; // Key updated
    doc["gridCols"] = placeGridCols;
    doc["gridRows"] = placeGridRows;
    doc["gapX"] = placeGapX_inch; // Key updated
    doc["gapY"] = placeGapY_inch; // Key updated
    // doc["itemWidth"] = pnpItemWidth_inch; // Commented out: Item size not directly settable
    // doc["itemHeight"] = pnpItemHeight_inch;// Commented out: Item size not directly settable
    // doc["borderWidth"] = pnpBorderWidth_inch; // Commented out: Border not directly settable
    doc["trayWidth"] = trayWidth_inch; // Added
    doc["trayHeight"] = trayHeight_inch; // Added

    // Paint Settings
    doc["paintPatOffX"] = paintPatternOffsetX_inch;
    doc["paintPatOffY"] = paintPatternOffsetY_inch;
    doc["paintGunOffX"] = paintGunOffsetX_inch;
    doc["paintGunOffY"] = paintGunOffsetY_inch;

    // Paint Side Settings
    for (int i = 0; i < 4; ++i) {
        String keyZ = "paintZ_" + String(i);
        String keyP = "paintP_" + String(i);
        String keyR = "paintR_" + String(i);
        String keyS = "paintS_" + String(i);
        doc[keyZ] = paintZHeight_inch[i];
        
        // Map the actual servo pitch values back to UI values (0-90)
        int uiPitchValue = map(paintPitchAngle[i], PITCH_SERVO_MIN, PITCH_SERVO_MAX, 0, 90);
        doc[keyP] = uiPitchValue;
        
        // Map the actual servo position values back to UI values
        // UI expects 0 for vertical, 90 for horizontal
        if (paintRollAngle[i] == ROLL_VERTICAL) {
            doc[keyR] = 0; // Vertical in UI
        } else if (paintRollAngle[i] == ROLL_HORIZONTAL) {
            doc[keyR] = 90; // Horizontal in UI
    } else {
            // If the value is neither expected value, default to vertical in UI
            Serial.printf("[WARN] Unexpected Roll value %d for side %d, mapping to VERTICAL (0) for UI\n", 
                         paintRollAngle[i], i);
            doc[keyR] = 0;
        }
        
        doc[keyS] = paintSpeed[i]; // Send the raw speed value (e.g., 5000-20000)
    }

    // Serialize JSON to string
    String output;
    serializeJson(doc, output);

    // Send to specific client or broadcast
    if (specificClientNum < 255) {
        // Serial.printf("[DEBUG] Sending settings update to client %d: %s\n", specificClientNum, output.c_str()); // DEBUG
        webSocket.sendTXT(specificClientNum, output);
    } else {
        // Serial.printf("[DEBUG] Broadcasting settings update: %s\n", output.c_str()); // DEBUG
        webSocket.broadcastTXT(output);
    }
}

void setupWebServer() {
    webServer.on("/", HTTP_GET, handleRoot); // Route for root page
    webServer.begin(); // Start HTTP server
    // Serial.println("HTTP server started.");

    webSocket.begin(); // Start WebSocket server
    webSocket.onEvent(webSocketEvent); // Assign event handler
    // Serial.println("WebSocket server started on port 81.");
}


// --- Arduino Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("Booting... Loading settings...");

    // --- Load Persistent Settings ---
    preferences.begin("machineCfg", false); // Open NVS Read/Write

    // Load PnP Offsets (provide defaults if keys don't exist)
    pnpOffsetX_inch = preferences.getFloat("pnpOffX", 15.0);
    pnpOffsetY_inch = preferences.getFloat("pnpOffY", 0.0);

    // Load Speed/Accel (provide defaults if keys don't exist)
    patternXSpeed = preferences.getFloat("patXSpd", 20000.0); // Actual value
    patternXAccel = preferences.getFloat("patXAcc", 20000.0); // Actual value
    patternYSpeed = preferences.getFloat("patYSpd", 20000.0); // Actual value
    patternYAccel = preferences.getFloat("patYAcc", 20000.0); // Actual value
    // Note: Z and Rotation speeds/accels are not user-settable via UI currently,
    // so we don't load/save them, they use the defaults defined earlier.

    // Load First Place Absolute Position (provide defaults)
    placeFirstXAbsolute_inch = preferences.getFloat("placeFirstXAbs", 20.0);
    placeFirstYAbsolute_inch = preferences.getFloat("placeFirstYAbs", 20.0);

    // Load Grid Dimensions (provide defaults)
    placeGridCols = preferences.getInt("gridCols", 4);
    placeGridRows = preferences.getInt("gridRows", 5);
    
    // Load existing gap values if they exist
    placeGapX_inch = preferences.getFloat("gapX", 0.0f);
    placeGapY_inch = preferences.getFloat("gapY", 0.0f);
    
    Serial.printf("[DEBUG] Loaded from NVS - Grid: %d x %d, Gap: X=%.3f, Y=%.3f\n", 
                 placeGridCols, placeGridRows, placeGapX_inch, placeGapY_inch);
    
    // Load Tray Dimensions (provide defaults) (NEW)
    trayWidth_inch = preferences.getFloat("trayWidth", 24.0f);
    trayHeight_inch = preferences.getFloat("trayHeight", 18.0f);
    
    // Validate the loaded tray dimensions
    if (trayWidth_inch <= 0 || trayHeight_inch <= 0) {
        Serial.printf("[ERROR] Invalid tray dimensions loaded: width=%.2f, height=%.2f. Using defaults.\n", 
                     trayWidth_inch, trayHeight_inch);
        trayWidth_inch = 24.0f;
        trayHeight_inch = 18.0f;
        
        // Immediately save the corrected values
        preferences.putFloat("trayWidth", trayWidth_inch);
        preferences.putFloat("trayHeight", trayHeight_inch);
    }
    
    Serial.printf("[DEBUG] Loaded tray dimensions: width=%.2f, height=%.2f\n", 
                 trayWidth_inch, trayHeight_inch);

    // Calculate initial grid gap based on loaded dimensions
    calculateAndSetGridSpacing(placeGridCols, placeGridRows);
    // Note: calculateAndSetGridSpacing also saves cols/rows/gaps back to NVS,
    // which is slightly redundant here but harmless.

    // Load Painting Offsets
    paintPatternOffsetX_inch = preferences.getFloat("paintPatOffX", 0.0f);
    paintPatternOffsetY_inch = preferences.getFloat("paintPatOffY", 0.0f);
    paintGunOffsetX_inch = preferences.getFloat("paintGunOffX", 0.0f);
    paintGunOffsetY_inch = preferences.getFloat("paintGunOffY", 1.5f);

    // Load Painting Side Settings (using side index in key)
    for (int i = 0; i < 4; i++) {
        String keyZ = "paintZ_" + String(i);
        String keyP = "paintP_" + String(i);
        String keyR = "paintR_" + String(i);
        String keyS = "paintS_" + String(i);
        paintZHeight_inch[i] = preferences.getFloat(keyZ.c_str(), 1.0f);
        paintPitchAngle[i] = preferences.getInt(keyP.c_str(), PITCH_SERVO_MIN); // Default to min
        paintRollAngle[i] = preferences.getInt(keyR.c_str(), ROLL_VERTICAL);   // Default to vertical
        paintSpeed[i] = preferences.getFloat(keyS.c_str(), 10000.0f);       // Default to 10k steps/sec

        // Clamp loaded servo values to their limits just in case
        paintPitchAngle[i] = constrain(paintPitchAngle[i], PITCH_SERVO_MIN, PITCH_SERVO_MAX);
        
        // Note: We don't constrain paintRollAngle here since it will be mapped in the sendAllSettingsUpdate function
        // This ensures the correct values are sent to the UI
         
         paintSpeed[i] = constrain(paintSpeed[i], 5000.0f, 20000.0f); // Clamp speed 5k-20k
    }

    preferences.end(); // Close NVS
    Serial.printf("Loaded Settings: Offset(%.2f, %.2f), FirstPlaceAbs(%.2f, %.2f), X(S:%.0f, A:%.0f), Y(S:%.0f, A:%.0f), Grid(%d x %d), Spacing(%.2f, %.2f)\n",
                  pnpOffsetX_inch, pnpOffsetY_inch,
                  placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                  patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                  placeGridCols, placeGridRows,
                  placeGapX_inch, placeGapY_inch);
    Serial.printf("Loaded Tray Size: W=%.2f, H=%.2f\n", trayWidth_inch, trayHeight_inch); // Added Tray Size log
    Serial.printf("Loaded Paint Settings: PatOffset(%.2f, %.2f), GunOffset(%.2f, %.2f)\n",
                  paintPatternOffsetX_inch, paintPatternOffsetY_inch,
                  paintGunOffsetX_inch, paintGunOffsetY_inch);
    for (int i = 0; i < 4; i++) {
        Serial.printf("  Side %d: Z=%.2f, Pitch=%d, Roll=%d, Speed=%.0f\n",
                      i, paintZHeight_inch[i], paintPitchAngle[i], paintRollAngle[i], paintSpeed[i]);
    }
    // ---

    // Serial.println("Booting...");

    // --- Servo Setup ---
    // Serial.println("Initializing Servos...");
    // Allow allocation of all timers
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	servo_pitch.setPeriodHertz(50);    // Standard 50Hz servo frequency
    servo_roll.setPeriodHertz(50);

    // Attach servos to pins using default min/max pulse widths (500, 2400 us)
    servo_pitch.attach(PITCH_SERVO_PIN);
    servo_roll.attach(ROLL_SERVO_PIN);

    // Move servos to initial max positions
    // Serial.printf("Setting Pitch Servo to %d\\n", PITCH_SERVO_MAX);
    servo_pitch.write(PITCH_SERVO_MAX); // Use defined max value
    // Serial.printf("Setting Roll Servo to %d\\n", ROLL_HORIZONTAL);
    servo_roll.write(ROLL_HORIZONTAL); // Use defined horizontal value as 'max'

    delay(500); // Give servos time to reach position
    // Serial.println("Servos Initialized.");
    // --- End Servo Setup ---

    // --- WiFi Connection ---
    // Serial.print("Connecting to ");
    // Serial.println(ssid);
    WiFi.setHostname(hostname); // Set hostname
    WiFi.mode(WIFI_STA); // Set WiFi to station mode
    WiFi.begin(ssid, password);
    int wifi_retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        // Serial.print(".");
        wifi_retries++;
        if (wifi_retries > 40) { // Timeout after ~20 seconds
            // Serial.println("\nFailed to connect to WiFi!");
            break; // Continue without WiFi if timeout
        }
    }

    bool wifiConnected = (WiFi.status() == WL_CONNECTED);

    if (wifiConnected) {
        // Serial.println("\nWiFi connected.");
        // Serial.print("IP address: ");
        // Serial.println(WiFi.localIP());

        // --- OTA Setup ---
        ArduinoOTA.setHostname(hostname);
        // ArduinoOTA.setPassword("your_ota_password"); // Optional: set password
        ArduinoOTA
            .onStart([]() {
                String type;
                isMoving = true; // Prevent web commands during OTA
                isHoming = true; // Prevent web commands during OTA
                if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
                else type = "filesystem";
                // Serial.println("Start updating " + type);
                if(stepper_x) stepper_x->forceStopAndNewPosition(stepper_x->getCurrentPosition());
                if(stepper_y_left) stepper_y_left->forceStopAndNewPosition(stepper_y_left->getCurrentPosition());
                if(stepper_y_right) stepper_y_right->forceStopAndNewPosition(stepper_y_right->getCurrentPosition());
                if(stepper_z) stepper_z->forceStopAndNewPosition(stepper_z->getCurrentPosition());
            })
            .onEnd([]() { /* Serial.println("\nEnd"); */ })
            .onProgress([](unsigned int progress, unsigned int total) { /* Serial.printf("Progress: %u%%\r", (progress / (total / 100))); */ })
            .onError([](ota_error_t error) { /* Error handling */ });
        ArduinoOTA.begin();
        // Serial.println("OTA Initialized");

        // --- Web Server Setup ---
        setupWebServer(); // Setup web server only if WiFi connected

    } else {
        // Serial.println("Proceeding without WiFi/OTA/Web Interface.");
    }
    // --- End WiFi/OTA/Web Setup ---

    // Serial.println("Initializing Steppers...");
    // Initialize Stepper Engine
    engine.init();

    // Setup Steppers
    stepper_x = engine.stepperConnectToPin(X_STEP_PIN);
    if (stepper_x) {
        stepper_x->setDirectionPin(X_DIR_PIN);
        stepper_x->setEnablePin(-1);
        stepper_x->setAutoEnable(false);
    } // else { Serial.println("ERROR: Failed to connect to X stepper"); }

    stepper_y_left = engine.stepperConnectToPin(Y_LEFT_STEP_PIN);
    if (stepper_y_left) {
        stepper_y_left->setDirectionPin(Y_LEFT_DIR_PIN);
        stepper_y_left->setEnablePin(-1);
        stepper_y_left->setAutoEnable(false);
    } // else { Serial.println("ERROR: Failed to connect to Y Left stepper"); }

    stepper_y_right = engine.stepperConnectToPin(Y_RIGHT_STEP_PIN);
    if (stepper_y_right) {
        stepper_y_right->setDirectionPin(Y_RIGHT_DIR_PIN);
        stepper_y_right->setEnablePin(-1);
        stepper_y_right->setAutoEnable(false);
    } // else { Serial.println("ERROR: Failed to connect to Y Right stepper"); }

    stepper_z = engine.stepperConnectToPin(Z_STEP_PIN);
    if (stepper_z) {
        stepper_z->setDirectionPin(Z_DIR_PIN);
        stepper_z->setEnablePin(-1);
        stepper_z->setAutoEnable(false);
    } // else { Serial.println("ERROR: Failed to connect to Z stepper"); }

    // Setup Rotation Stepper with Debug
    Serial.println("DEBUG: Initializing Rotation Stepper...");
    Serial.printf("DEBUG: Using ROTATION_STEP_PIN: %d, ROTATION_DIR_PIN: %d\n", ROTATION_STEP_PIN, ROTATION_DIR_PIN);
    
    stepper_rot = engine.stepperConnectToPin(ROTATION_STEP_PIN);
    if (stepper_rot) {
        Serial.println("DEBUG: Rotation stepper created successfully");
        stepper_rot->setDirectionPin(ROTATION_DIR_PIN);
        stepper_rot->setEnablePin(-1); // Assuming no enable pin used or needed
        stepper_rot->setAutoEnable(false);
        stepper_rot->setCurrentPosition(0); // Assume starts at 0 steps (Back side)
        // Test rotation stepper
        Serial.println("DEBUG: Testing rotation stepper with 90 degree movement");
        stepper_rot->setSpeedInHz(patternRotSpeed/2); // Lower speed for test
        stepper_rot->setAcceleration(patternRotAccel/2); // Lower accel for test
        // stepper_rot->move((long)(90 * STEPS_PER_DEGREE)); // Move 90 degrees // <<< COMMENTED OUT
    } else {
        Serial.println("DEBUG: ERROR - Failed to initialize rotation stepper!");
    }

    // --- Initial Homing on Boot ---
     // Serial.println("Performing initial homing sequence...");
    homeAllAxes(); // Call the refactored homing function

    // Note: allHomed flag is set within homeAllAxes()
     if (allHomed) {
         // Serial.println("Initial homing successful.");
     } else {
         // Serial.println("WARNING: Initial homing failed. Web commands requiring homing will be disabled.");
     }

    // --- Setup Physical Buttons ---
    pinMode(PNP_CYCLE_BUTTON_PIN, INPUT); // Use renamed define
    debouncer_pnp_cycle_button.attach(PNP_CYCLE_BUTTON_PIN); // Use renamed define
    debouncer_pnp_cycle_button.interval(DEBOUNCE_INTERVAL); // Use same debounce as limit switches
    // Serial.println("Physical buttons initialized.");

    // --- Setup Actuator Pins ---
    pinMode(PICK_CYLINDER_PIN, OUTPUT);
    pinMode(SUCTION_PIN, OUTPUT);
    digitalWrite(PICK_CYLINDER_PIN, LOW); // Start retracted
    digitalWrite(SUCTION_PIN, LOW);       // Start suction off
    Serial.println("Actuator pins initialized.");
}

// --- Arduino Loop ---
void loop() {
    // Handle OTA requests if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        webSocket.loop();       // Handle WebSocket connections
        webServer.handleClient(); // Handle HTTP requests
    }

    // Update switch debouncers (less critical now, mainly for potential future use)
    debouncer_x_home.update();
    debouncer_y_left_home.update();
    debouncer_y_right_home.update();
    debouncer_z_home.update();
    debouncer_pnp_cycle_button.update();

    // --- Handle Physical Button Press ---
    if (debouncer_pnp_cycle_button.rose()) { // Trigger on button press (rising edge)
        Serial.println("[DEBUG] Physical Button Pressed! (rose)"); // DEBUG
        // Only trigger the cycle if in PnP mode and not already doing something
        if (inPickPlaceMode && !isMoving && !isHoming) {
             Serial.println("[DEBUG] Triggering PnP cycle from physical button."); // DEBUG
             executeNextPickPlaceStep(); // Call the correct function
        } else {
             Serial.printf("[DEBUG] Ignoring physical button: inPnP=%d, isMoving=%d, isHoming=%d\n", inPickPlaceMode, isMoving, isHoming); // DEBUG
            // Serial.println("Ignoring physical button press (not in PnP mode or busy).");
            // Optional: Provide feedback? (e.g., short LED blink)
        }
    }

    // --- Handle Homing Request after PnP --- 
    if (pendingHomingAfterPnP && !isMoving && !isHoming) {
        Serial.println("[DEBUG] Handling pending homing request after PnP sequence.");
        pendingHomingAfterPnP = false; // Clear the flag
        homeAllAxes(); // Initiate homing
    }

    // Check if a move command (GOTO or JOG) is active and has finished
    if (isMoving) { 
        // Remember which motors were running at the start of this loop iteration
        bool rot_was_running = stepper_rot && stepper_rot->isRunning();

        bool x_running = stepper_x && stepper_x->isRunning();
        bool yl_running = stepper_y_left && stepper_y_left->isRunning();
        bool yr_running = stepper_y_right && stepper_y_right->isRunning();
        bool z_running = stepper_z && stepper_z->isRunning(); // Include Z check
        bool rot_running = stepper_rot && stepper_rot->isRunning(); // Keep this check for the condition

        // Add detailed rotation debugging every 500ms
        static unsigned long lastRotDebugTime = 0;
        unsigned long currentTime = millis();
        if (rot_running && currentTime - lastRotDebugTime > 500) {
            lastRotDebugTime = currentTime;
            Serial.printf("DEBUG ROTATION: Current pos: %ld steps, Target pos: %ld, Speed: %.2f us\n", 
                        stepper_rot->getCurrentPosition(),
                        stepper_rot->targetPos(),
                        stepper_rot->getCurrentSpeedInUs());
        }
        
        if (!x_running && !yl_running && !yr_running && !z_running && !rot_running) { // Added rot_running
            Serial.printf("[DEBUG] loop(): Detected move completion. Clearing isMoving flag (was %d).\n", isMoving); // DEBUG
            isMoving = false;
            
            if (inCalibrationMode) {
                // If calibration is active, send position update and CalibrationActive status
                // Serial.println("Jog move complete.");
                webSocket.broadcastTXT("{\"status\":\"CalibrationActive\", \"message\":\"Jog complete.\"}");
                sendCurrentPositionUpdate(); // Send updated position (without angle)
            } else if (!inPickPlaceMode) { // Includes rotation moves now
                 // If it was a general move (not PnP), send Ready status
            // Serial.println("Generic move complete.");
            webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Move complete.\"}");
                 // If the rotation motor just finished, send a position update
                 if (rot_was_running) {
                    Serial.printf("DEBUG: Rotation move completed. Final position: %ld steps\n", 
                                stepper_rot->getCurrentPosition());
                    sendCurrentPositionUpdate(); // Send updated position (without angle)
                 }
            }
            // Note: PnP move completion is handled within the PnP logic itself
        }
    }

    // Small delay to prevent excessive polling if nothing else is happening
    // But ensure yield() or delay() is called frequently enough for background tasks
    // If WiFi/servers are active, their loop/handle calls provide yielding.
    // If not connected, add a small delay.
    if (WiFi.status() != WL_CONNECTED) {
         delay(1);
    }
} 

// NEW: Function to calculate and set grid spacing automatically
void calculateAndSetGridSpacing(int cols, int rows) {
    Serial.printf("[DEBUG] calculateAndSetGridSpacing with cols=%d, rows=%d, trayWidth=%.2f, trayHeight=%.2f\n", 
                 cols, rows, trayWidth_inch, trayHeight_inch);
                 
    // Input validation for columns and rows
    if (cols <= 0 || rows <= 0) {
        Serial.printf("[ERROR] Invalid grid dimensions: cols=%d, rows=%d. Must be positive.\n", cols, rows);
        return; // Exit without calculations
    }
    
    if (trayWidth_inch <= 0 || trayHeight_inch <= 0) {
        Serial.printf("[ERROR] Invalid tray dimensions: width=%.2f, height=%.2f. Must be positive.\n", 
                     trayWidth_inch, trayHeight_inch);
        // Use defaults to prevent crashes
        trayWidth_inch = max(trayWidth_inch, 24.0f);
        trayHeight_inch = max(trayHeight_inch, 18.0f);
        Serial.printf("[WARN] Using default tray dimensions: width=%.2f, height=%.2f\n", 
                     trayWidth_inch, trayHeight_inch);
    }
    
    // UPDATED X GAP CALCULATION FORMULA:
    // X gap = (trayWidth - (.25 border * 2) - (3 inch squares * # of columns)) / (# of columns - 1)
    // Y gap = (trayHeight - (borderWidth * 2) - (itemHeight * rows)) / (rows - 1)

    float placeGapX_inch_old = placeGapX_inch; // Store old value for comparison

    // X-direction gap calculation with the formula
    if (cols > 1) {
        placeGapX_inch = (trayWidth_inch - (2 * pnpBorderWidth_inch) - (pnpItemWidth_inch * cols)) / (cols - 1);
    } else {
        placeGapX_inch = 0; // No gap needed for 1 column
    }

    // Y-direction gap calculation (unchanged)
    // For Y: traySize = trayHeight, itemSize = pnpItemHeight_inch, numItems = rows
    float totalYSpace = trayHeight_inch - (2 * pnpBorderWidth_inch); // Total available space minus borders
    float totalItemYSpace = rows * pnpItemHeight_inch; // Total space occupied by items
    float totalGapYSpace = totalYSpace - totalItemYSpace; // Total space for gaps
    
    Serial.printf("[DEBUG] Using tray dimensions: Width=%.2f, Height=%.2f\n", trayWidth_inch, trayHeight_inch);
    Serial.printf("[DEBUG] Border width: %.2f, Item width: %.2f, Item height: %.2f\n", 
                 pnpBorderWidth_inch, pnpItemWidth_inch, pnpItemHeight_inch);
    Serial.printf("[DEBUG] Using formula: (%.2f - (.25*2) - (3*%d))/%d for X gap\n", 
                 trayWidth_inch, cols, (cols-1));
    Serial.printf("[DEBUG] X gap changed from %.3f to %.3f\n", placeGapX_inch_old, placeGapX_inch);

    if (rows > 1) {
        placeGapY_inch = totalGapYSpace / (rows - 1);
    } else {
        placeGapY_inch = 0; // No gap needed for 1 row
    }

    // Basic validation: Check if calculated gap is negative
    bool fitError = false;
    if (placeGapX_inch < 0) {
        Serial.printf("[WARN] Calculated X Gap is negative: %.3f. Items don't fit in fixed width of 26 inches.\n", 
                     placeGapX_inch);
        fitError = true;
        placeGapX_inch = 0; // Clamp gap to 0 if negative
    }
    if (totalGapYSpace < 0) {
        Serial.printf("[WARN] Items (%.2f) + borders (%.2f) exceed tray height (%.2f). Calculated Y Gap=%.3f\n", 
                     totalItemYSpace, 2*pnpBorderWidth_inch, trayHeight_inch, placeGapY_inch);
        fitError = true;
        placeGapY_inch = 0; // Clamp gap to 0 if negative
    }

    // Update global column/row count
    placeGridCols = cols;
    placeGridRows = rows;

    Serial.printf("[INFO] Final calculated gap values: X=%.3f, Y=%.3f\n", placeGapX_inch, placeGapY_inch);

    // Save Cols, Rows, Gap, and tray dimensions to Preferences
    preferences.begin("machineCfg", false);
    preferences.putInt("gridCols", placeGridCols);
    preferences.putInt("gridRows", placeGridRows);
    preferences.putFloat("gapX", placeGapX_inch);
    preferences.putFloat("gapY", placeGapY_inch);
    
    // Also save tray dimensions to ensure consistency
    preferences.putFloat("trayWidth", trayWidth_inch);
    preferences.putFloat("trayHeight", trayHeight_inch);
    preferences.end();
    
    Serial.printf("[DEBUG] Saved to preferences: gridCols=%d, gridRows=%d, gapX=%.3f, gapY=%.3f, trayWidth=%.2f, trayHeight=%.2f\n",
                 placeGridCols, placeGridRows, placeGapX_inch, placeGapY_inch, trayWidth_inch, trayHeight_inch);

    // Send update to UI
    String message = "Grid Columns/Rows updated. Gap calculated.";
    if (fitError) {
        message += " Warning: Items may not fit within tray dimensions!";
    }
    
    Serial.printf("[DEBUG] Sending update to UI with message: %s\n", message.c_str());
    sendAllSettingsUpdate(255, message); // Send to all clients
} 