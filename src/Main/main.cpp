#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include <WiFi.h>        // Added for WiFi
#include <ArduinoOTA.h>  // Added for OTA
#include <WebServer.h>   // Added for HTTP Server
#include <WebSocketsServer.h> // Added for WebSocket Server
#include <ESP32Servo.h>     // Added for Servos
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

// Pattern/General Speed/Accel Variables (declared extern in GeneralSettings_PinDef.h)
float patternXSpeed = 20000.0; // Actual value
float patternXAccel = 20000.0; // Actual value
float patternYSpeed = 20000.0; // Actual value
float patternYAccel = 20000.0; // Actual value
float patternZSpeed = 500.0;
float patternZAccel = 1300.0;
float patternRotSpeed = 2000.0; // Reduced from 3000 for more reliable movement
float patternRotAccel = 1000.0; // Reduced from 2000 for more reliable movement

// Tray Dimensions (NEW)
float trayHeight_inch = 26.0; // Default height
float trayWidth_inch = 18.0; // Default width

// PnP variables (declared extern in PickPlace.h, defined in PickPlace.cpp)
/*
float pnpOffsetX_inch = 15.0; // Default X offset
float pnpOffsetY_inch = 0.0;  // Default Y offset
float placeFirstXAbsolute_inch = 20.0; // Default ABSOLUTE X
float placeFirstYAbsolute_inch = 20.0; // Default ABSOLUTE Y

// PnP Grid/Spacing Variables (declared extern in PickPlace.h, defined in PickPlace.cpp)
int placeGridCols = 4;        // Default Columns
int placeGridRows = 5;        // Default Rows
float placeSpacingX_inch = 4.0f; // Default X Spacing
float placeSpacingY_inch = 4.0f; // Default Y Spacing
*/

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

    // === Z Limit Check ===
    long min_z_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z);
    long max_z_steps = (long)(Z_HOME_POS_INCH * STEPS_PER_INCH_Z); // Should be 0
    targetZ_steps = max(min_z_steps, targetZ_steps); // Ensure not below min travel
    targetZ_steps = min(max_z_steps, targetZ_steps); // Ensure not above home position (0)
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
    // Note: Removed the isMoving check here as paintSide now manages the overall state
    if (isHoming || inPickPlaceMode || inCalibrationMode) { 
        Serial.printf("[ERROR] moveZToPositionInches denied: Machine state conflict (isHoming=%d, inPnP=%d, inCalib=%d)\\n", isHoming, inPickPlaceMode, inCalibrationMode);
        // Corrected escaping for the JSON string
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot move Z, machine busy or in special mode.\"}");
        return;
    }

    if (!stepper_z) {
         // Corrected escaping for the JSON string
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Z Stepper not initialized.\"}");
         return;
    }

    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z);
    // === Z Limit Check ===
    long min_z_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z);
    long max_z_steps = (long)(Z_HOME_POS_INCH * STEPS_PER_INCH_Z); // Should be 0
    targetZ_steps = max(min_z_steps, targetZ_steps); // Ensure not below min travel
    targetZ_steps = min(max_z_steps, targetZ_steps); // Ensure not above home position (0)
    // === End Z Limit Check ===

    long currentZ_steps = stepper_z->getCurrentPosition();

    if (targetZ_steps == currentZ_steps) {
        // Serial.println("Z axis already at target.");
        return; // Already there
    }

    // Temporarily set moving flag for this blocking move // <<< REMOVED isMoving = true;
    // Serial.printf("Moving Z from %.2f to %.2f inches (Steps: %ld to %ld, Speed: %.0f, Accel: %.0f)\\n",
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
    // isMoving = false; // Clear flag after blocking move completes // <<< REMOVED isMoving = false;
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

    // Serial.printf("Painting move XY to (%.2f, %.2f) inches (Steps: X %ld, Y %ld, Speed: %.0f, Accel: %.0f)\\n",
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
    Serial.printf("Starting paintSide for side %d\\n", sideIndex);

    // 1. Check Machine State
    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
        Serial.printf("[ERROR] paintSide denied: Invalid state (allHomed=%d, isMoving=%d, isHoming=%d, inPickPlaceMode=%d, inCalibrationMode=%d)\\n",
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

    // 4. Rotate Tray Based on Side Index
    if (stepper_rot) {
        long targetRotSteps = 0;
        float targetAngleDeg = 0.0f;

        switch (sideIndex) {
            case 0: // Back
                targetAngleDeg = 0.0f;
                break;
            case 1: // Right
                targetAngleDeg = 90.0f;
                break;
            case 2: // Front
                targetAngleDeg = 180.0f;
                break;
            case 3: // Left
                targetAngleDeg = 270.0f; // Or -90?
                break;
            default:
                targetAngleDeg = 0.0f; // Default to 0
                break;
        }
        targetRotSteps = round(targetAngleDeg * STEPS_PER_DEGREE);

        Serial.printf("Painting Side %d: Rotating to %.1f degrees (%ld steps)...\\n", sideIndex, targetAngleDeg, targetRotSteps);
        if (stepper_rot->getCurrentPosition() != targetRotSteps) {
            // Use slightly slower speed for potentially larger rotation moves
            stepper_rot->setSpeedInHz(patternRotSpeed); 
            stepper_rot->setAcceleration(patternRotAccel);
            stepper_rot->moveTo(targetRotSteps);
            while (stepper_rot->isRunning()) {
                yield();
                webSocket.loop(); // Keep responsive during rotation
            }
            Serial.println("Rotation complete.");
        } else {
            Serial.printf("Already at target rotation (%.1f degrees).\\n", targetAngleDeg);
        }
        delay(100); // Small delay after rotation
    } else {
        Serial.println("[WARN] Rotation stepper not available, skipping rotation.");
    }

    // 5. Set Servo Angles
    Serial.printf("Setting servos: Pitch=%d, Roll=%d\\n", pitch, roll);
    servo_pitch.write(pitch);
    servo_roll.write(roll);
    delay(300); // Allow servos to settle

    // 6. Define Path Start Point & Calculate Grid Spacing
    float pathStartX = placeFirstXAbsolute_inch;
    float pathStartY = placeFirstYAbsolute_inch;
    float center_spacing_x_paint = (placeGridCols > 1) ? (trayWidth_inch - 0.5f - 3.0f) / (placeGridCols - 1) : 0.0f;
    float center_spacing_y_paint = (placeGridRows > 1) ? (trayHeight_inch - 0.5f - 3.0f) / (placeGridRows - 1) : 0.0f;
    Serial.printf("[DEBUG-Paint] Path Corner (Top-Right of Item Grid): (%.2f, %.2f)\\n", pathStartX, pathStartY); // This is the top-right corner of the *item* grid
    Serial.printf("[DEBUG-Paint] Gun Offset: (%.2f, %.2f)\\n", paintGunOffsetX_inch, paintGunOffsetY_inch);
    Serial.printf("[DEBUG-Paint] Calculated Center Spacing: X=%.2f, Y=%.2f\\n", center_spacing_x_paint, center_spacing_y_paint); // Debug

    // 7. Move to Start Z Height
    Serial.println("Moving to start Z height...");
    moveZToPositionInches(zHeight, patternZSpeed, patternZAccel);

    // 8. Calculate and Move TCP to START of FIRST SWEEP
    float startSweepTcpX, startSweepTcpY;
    if (sideIndex == 2) { // Front Side: Start at Bottom-Left of TCP path
        startSweepTcpX = (pathStartX - (placeGridCols - 1) * center_spacing_x_paint) + paintGunOffsetX_inch;
        startSweepTcpY = (pathStartY - (placeGridRows - 1) * center_spacing_y_paint) + paintGunOffsetY_inch;
        Serial.printf("Side 2 (Front): Moving TCP to start of first sweep (Bottom-Left): (%.2f, %.2f)\\n", startSweepTcpX, startSweepTcpY);
    } else { // Default (Side 0/Back and others for now): Start at Top-Right of TCP path
        startSweepTcpX = pathStartX + paintGunOffsetX_inch;
        startSweepTcpY = pathStartY + paintGunOffsetY_inch;
        Serial.printf("Side %d: Moving TCP to start of first sweep (Top-Right): (%.2f, %.2f)\n", sideIndex, startSweepTcpX, startSweepTcpY);
    }
    moveToXYPositionInches_Paint(startSweepTcpX, startSweepTcpY, speed, accel);

    // 9. Execute Painting Path
    Serial.println("Starting painting path...");
    float currentTcpX = startSweepTcpX;
    float currentTcpY = startSweepTcpY;

    for (int r = 0; r < placeGridRows; ++r) {
        float targetTcpX, targetTcpY;

        // Calculate Target Y for the *end* of this row's horizontal sweep
        if (sideIndex == 2) { // Front side: Move UP
            targetTcpY = (pathStartY - (placeGridRows - 1 - r) * center_spacing_y_paint) + paintGunOffsetY_inch;
        } else { // Default (Back): Move DOWN
            targetTcpY = (pathStartY - r * center_spacing_y_paint) + paintGunOffsetY_inch;
        }

        // --- Determine Sweep Target X --- 
        if (r % 2 == 0) { // Even row (0, 2, ...):
            if (sideIndex == 2) { // Front: Sweep RIGHT on even rows (starting from left)
                 targetTcpX = pathStartX + paintGunOffsetX_inch;
                 Serial.printf("Row %d (Even - Side 2): Sweeping Right to TCP X=%.2f, Y=%.2f\n", r, targetTcpX, targetTcpY);
            } else { // Default (Back): Sweep LEFT on even rows (starting from right)
                 targetTcpX = (pathStartX - (placeGridCols - 1) * center_spacing_x_paint) + paintGunOffsetX_inch; 
                 Serial.printf("Row %d (Even - Side %d): Sweeping Left to TCP X=%.2f, Y=%.2f\n", r, sideIndex, targetTcpX, targetTcpY);
            }
        } else { // Odd row (1, 3, ...):
             if (sideIndex == 2) { // Front: Sweep LEFT on odd rows (starting from right)
                 targetTcpX = (pathStartX - (placeGridCols - 1) * center_spacing_x_paint) + paintGunOffsetX_inch; 
                 Serial.printf("Row %d (Odd - Side 2): Sweeping Left to TCP X=%.2f, Y=%.2f\n", r, targetTcpX, targetTcpY);
            } else { // Default (Back): Sweep RIGHT on odd rows (starting from left)
                 targetTcpX = pathStartX + paintGunOffsetX_inch;
                 Serial.printf("Row %d (Odd - Side %d): Sweeping Right to TCP X=%.2f, Y=%.2f\n", r, sideIndex, targetTcpX, targetTcpY);
            }
        }
        
        // --- Execute Horizontal Sweep ---
        // Ensure Y is correct before sweeping X (It should be, due to the vertical move below)
        if (abs(currentTcpY - targetTcpY) < 0.001) { // Check if Y is already correct for the sweep target
             moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
             currentTcpX = targetTcpX; // Update current X
        } else {
             // This case should ideally not happen if the vertical move logic is correct
             Serial.printf("[WARN] Row %d: Y position mismatch before sweep (Current: %.3f, Target: %.3f). Moving directly to target.\n", r, currentTcpY, targetTcpY);
             moveToXYPositionInches_Paint(targetTcpX, targetTcpY, speed, accel);
             currentTcpX = targetTcpX;
             currentTcpY = targetTcpY; // Update Y as well
        }

        // --- Move Vertically to Next Row's starting Y (if not the last row) ---
        if (r < placeGridRows - 1) {
            float nextRowStartY;
            if (sideIndex == 2) { // Front side: Move UP to next row's Y
                nextRowStartY = (pathStartY - (placeGridRows - 1 - (r + 1)) * center_spacing_y_paint) + paintGunOffsetY_inch;
                 Serial.printf("Row %d: Moving UP to Y=%.2f (X=%.2f)\n", r, nextRowStartY, currentTcpX);
            } else { // Default (Back): Move DOWN to next row's Y
                 nextRowStartY = (pathStartY - (r + 1) * center_spacing_y_paint) + paintGunOffsetY_inch;
                 Serial.printf("Row %d: Moving DOWN to Y=%.2f (X=%.2f)\n", r, nextRowStartY, currentTcpX);
            }
            // Move only Y axis vertically
            moveToXYPositionInches_Paint(currentTcpX, nextRowStartY, speed, accel); // Move Y while keeping X constant
            currentTcpY = nextRowStartY; // Update current Y
        }
    }
    Serial.println("Painting path complete.");

    // 10. Move Z Axis Up (e.g., to safe height 0)
    Serial.println("Moving Z axis up to safe height (0)...");
    moveZToPositionInches(0.0, patternZSpeed, patternZAccel);

    // 10.5 Move XY to Home (0,0)
    Serial.println("Returning XY to home (0,0)...");
    moveToXYPositionInches_Paint(0.0, 0.0, patternXSpeed, patternXAccel); // Use general pattern speeds

    // 11. Clear Busy State & Send Ready Message
    isMoving = false;
    char readyMsg[100];
    sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Painting Side %d complete.\"}", sideIndex);
    webSocket.broadcastTXT(readyMsg);
    Serial.println(readyMsg);

    sendCurrentPositionUpdate(); // Send final position
}

// --- Web Server and WebSocket Logic ---

void handleRoot() {
  Serial.println("Serving root page."); // Add debug print
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
            // Send initial status including current settings
             String initialStatusStr = (allHomed ? (isMoving ? "Busy" : "Ready") : "Needs Homing");
             char initialMsgBuffer[400]; // Increased buffer size for grid/spacing
             sprintf(initialMsgBuffer, "{\"status\":\"%s\",\"message\":\"Welcome! System status: %s\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                     initialStatusStr.c_str(), initialStatusStr.c_str(),
                     pnpOffsetX_inch, pnpOffsetY_inch,
                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                     placeGridCols, placeGridRows,
                     trayHeight_inch, trayWidth_inch); // Added tray dimensions
             Serial.printf("[DEBUG] WebSocket [%u] Sending Initial State: %s\n", num, initialMsgBuffer); // DEBUG
             webSocket.sendTXT(num, initialMsgBuffer);
             // Also send initial position update immediately after settings
             if(allHomed) {
                 sendCurrentPositionUpdate();
             }
            }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);
            if (length > 0) {
                String command = String((char*)payload);
                Serial.printf("[DEBUG] WebSocket [%u] Received Command: %s\n", num, command.c_str()); // DEBUG
                if (command == "HOME") {
                    Serial.println("WebSocket: Received HOME command.");
                    if (isMoving || isHoming) {
                        // Fixed quotes
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot home, machine is busy.\"}");
                    } else {
                        if (inPickPlaceMode) {
                            Serial.println("[DEBUG] Homing requested while in PnP mode. Exiting PnP mode first.");
                            // Force isMoving to false if it's still set for some reason when exiting PnP mode
                            if (isMoving) {
                                Serial.println("[WARNING] isMoving flag was still true while trying to exit PnP mode. Forcing to false.");
                                isMoving = false;
                            }
                            inPickPlaceMode = false; // Exit PnP mode
                            // pnpSequenceComplete = false; // Reset PnP sequence status - This variable is defined in PickPlace.cpp, main doesn't need to set it directly.
                            // Fixed quotes
                            webSocket.broadcastTXT("{\"status\":\"Busy\",\"message\":\"Exiting PnP mode and starting homing...\"}"); // Inform user
                        }
                        homeAllAxes(); // Start homing (this function sends its own status updates)
                    }
                } else if (command == "GET_STATUS") {
                    Serial.println("WebSocket: Received GET_STATUS command.");
                    // Send current machine state
                    if (isMoving) {
                        webSocket.sendTXT(num, "{\"status\":\"Moving\",\"message\":\"Machine is currently moving.\"}");
                    } else if (isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Homing\",\"message\":\"Machine is currently homing.\"}");
                    } else if (inPickPlaceMode) {
                        webSocket.sendTXT(num, "{\"status\":\"PickPlaceReady\",\"message\":\"Pick and Place mode active.\"}");
                    } else if (inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"CalibrationActive\",\"message\":\"Manual Move mode active.\"}");
                    } else if (allHomed) {
                        webSocket.sendTXT(num, "{\"status\":\"Ready\",\"message\":\"Machine ready.\"}");
                    } else {
                        webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Machine not homed.\"}");
                    }
                    
                    // Also send current settings
                    sendAllSettingsUpdate(num, "Settings updated.");
                    
                    // Send current position
                    sendCurrentPositionUpdate();
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
                 } else if (command == "PNP_NEXT_SQUARE") {
                    Serial.println("WebSocket: Received PNP_NEXT_SQUARE command.");
                    moveToNextPickPlaceSquare(); // Call function to update grid position only
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
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"PnP offset updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     trayHeight_inch, trayWidth_inch); // Added tray dimensions
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
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"Speed/Accel updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     trayHeight_inch, trayWidth_inch); // Added tray dimensions
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
                        sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"First Place Absolute Pos updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     trayHeight_inch, trayWidth_inch); // Added tray dimensions
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
                             sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"PnP Offset set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     trayHeight_inch, trayWidth_inch); // Added tray dimensions
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
                               sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"First Place Absolute Pos set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                       pnpOffsetX_inch, pnpOffsetY_inch,
                                       placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                       patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                       placeGridCols, placeGridRows,
                                       trayHeight_inch, trayWidth_inch); // Added tray dimensions
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
                 } else if (command.startsWith("SET_GRID_SPACING ")) {
                    Serial.println("WebSocket: Received SET_GRID_SPACING command.");
                    if (isMoving || isHoming || inPickPlaceMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Cannot set grid/spacing while machine is busy or in PnP mode.\"}");
                    } else {
                        int newCols, newRows;
                        // Removed float newSX, newSY;
                        // MODIFIED sscanf to only parse Cols and Rows
                        int parsed = sscanf(command.c_str() + strlen("SET_GRID_SPACING "), "%d %d", &newCols, &newRows);
                        // MODIFIED validation check
                        if (parsed == 2 && newCols > 0 && newRows > 0) { // REMOVED sxVal, syVal checks
                            placeGridCols = newCols;
                            placeGridRows = newRows;
                            // REMOVED placeSpacingX_inch, placeSpacingY_inch assignments
 
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putInt("gridCols", placeGridCols);
                            preferences.putInt("gridRows", placeGridRows);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Grid/Spacing to Cols=%d, Rows=%d\n", placeGridCols, placeGridRows);

                            // Send confirmation with all current settings
                            char msgBuffer[400]; // Increased size
                            sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"Grid/Spacing updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f}",
                                    pnpOffsetX_inch, pnpOffsetY_inch,
                                    placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                    patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                    placeGridCols, placeGridRows,
                                    trayHeight_inch, trayWidth_inch); // Added tray dimensions
                            webSocket.broadcastTXT(msgBuffer);
                        } else {
                            Serial.println("[ERROR] Failed to parse SET_GRID_SPACING values or values invalid.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_GRID_SPACING format/values. Use: SET_GRID_SPACING C R (all positive)\"}");
                        }
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
                            // Validate servo angles and speed
                            newPitch = constrain(newPitch, PITCH_SERVO_MIN, PITCH_SERVO_MAX);
                            newRoll = constrain(newRoll, ROLL_VERTICAL, ROLL_HORIZONTAL); // Assuming VERTICAL=min, HORIZONTAL=max
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
                } else if (command == "PAINT_SIDE_2") { // <<< NEW COMMAND HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_2 command.");
                    paintSide(2); // Call the painting function for Side 2
                } else if (command.startsWith("SET_TRAY_SIZE ")) { // NEW Handler
                    Serial.println("WebSocket: Received SET_TRAY_SIZE command.");
                    if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set tray size while machine is busy.\"}");
                    } else {
                        float newH, newW;
                        int parsed = sscanf(command.c_str() + strlen("SET_TRAY_SIZE "), "%f %f", &newH, &newW);
                        if (parsed == 2 && newH > 0 && newW > 0) {
                            trayHeight_inch = newH;
                            trayWidth_inch = newW;
                            // Save to Preferences
                            preferences.begin("machineCfg", false);
                            preferences.putFloat("trayH", trayHeight_inch);
                            preferences.putFloat("trayW", trayWidth_inch);
                            preferences.end();
                            Serial.printf("[DEBUG] Set Tray Size to H: %.1f, W: %.1f\n", trayHeight_inch, trayWidth_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Tray Size updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_TRAY_SIZE format/values. Use: SET_TRAY_SIZE H W (positive)\"}");
                        }
                    }
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
    char settingsBuffer[700]; // Increased buffer size substantially AGAIN
    sprintf(settingsBuffer, "{\"status\":\"Ready\",\"message\":\"%s\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"trayHeight\":%.1f,\"trayWidth\":%.1f,\"paintPatOffX\":%.2f,\"paintPatOffY\":%.2f,\"paintGunOffX\":%.2f,\"paintGunOffY\":%.2f,\"paintZ_0\":%.2f,\"paintP_0\":%d,\"paintR_0\":%d,\"paintS_0\":%.0f,\"paintZ_1\":%.2f,\"paintP_1\":%d,\"paintR_1\":%d,\"paintS_1\":%.0f,\"paintZ_2\":%.2f,\"paintP_2\":%d,\"paintR_2\":%d,\"paintS_2\":%.0f,\"paintZ_3\":%.2f,\"paintP_3\":%d,\"paintR_3\":%d,\"paintS_3\":%.0f}",
            message.c_str(),
            pnpOffsetX_inch, pnpOffsetY_inch,
            placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
            patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
            placeGridCols, placeGridRows,
            trayHeight_inch, trayWidth_inch, // Added tray dimensions
            paintPatternOffsetX_inch, paintPatternOffsetY_inch,
            paintGunOffsetX_inch, paintGunOffsetY_inch,
            paintZHeight_inch[0], paintPitchAngle[0], paintRollAngle[0], paintSpeed[0],
            paintZHeight_inch[1], paintPitchAngle[1], paintRollAngle[1], paintSpeed[1],
            paintZHeight_inch[2], paintPitchAngle[2], paintRollAngle[2], paintSpeed[2],
            paintZHeight_inch[3], paintPitchAngle[3], paintRollAngle[3], paintSpeed[3]
    );
    if (specificClientNum < 255) { // Send only to specific client if num is valid
         webSocket.sendTXT(specificClientNum, settingsBuffer);
         // Also broadcast to others, maybe with a generic message?
         // For simplicity, let's just broadcast the full update to everyone.
         webSocket.broadcastTXT(settingsBuffer);
    } else {
        webSocket.broadcastTXT(settingsBuffer); // Broadcast if num is not specific
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

    // Load Tray Dimensions (NEW)
    trayHeight_inch = preferences.getFloat("trayH", 26.0); // Default if key doesn't exist
    trayWidth_inch = preferences.getFloat("trayW", 18.0); // Default if key doesn't exist

    // Load First Place Absolute Position (provide defaults)
    placeFirstXAbsolute_inch = preferences.getFloat("placeFirstXAbs", 20.0);
    placeFirstYAbsolute_inch = preferences.getFloat("placeFirstYAbs", 20.0);

    // Load Grid Dimensions and Spacing (provide defaults)
    placeGridCols = preferences.getInt("gridCols", 4);
    placeGridRows = preferences.getInt("gridRows", 5);

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
        paintRollAngle[i] = constrain(paintRollAngle[i], ROLL_VERTICAL, ROLL_HORIZONTAL); // Assuming VERTICAL is min, HORIZONTAL is max
         paintSpeed[i] = constrain(paintSpeed[i], 5000.0f, 20000.0f); // Clamp speed 5k-20k
    }

    preferences.end(); // Close NVS
    Serial.printf("Loaded Settings: Offset(%.2f, %.2f), FirstPlaceAbs(%.2f, %.2f), X(S:%.0f, A:%.0f), Y(S:%.0f, A:%.0f), Grid(%d x %d)\n", // REMOVED: , Spacing(%.2f, %.2f)
                  pnpOffsetX_inch, pnpOffsetY_inch,
                  placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                  patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                  placeGridCols, placeGridRows
                  // REMOVED spacingX_inch, spacingY_inch arguments
                  );
    Serial.printf("Loaded Paint Settings: PatOffset(%.2f, %.2f), GunOffset(%.2f, %.2f)\n",
                  paintPatternOffsetX_inch, paintPatternOffsetY_inch,
                  paintGunOffsetX_inch, paintGunOffsetY_inch);
    Serial.printf("Loaded Tray Size: H=%.1f, W=%.1f\n", trayHeight_inch, trayWidth_inch); // Added log for tray size
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
    // Handle OTA first, if active
        ArduinoOTA.handle();
    
    // Handle HTTP Server requests
    webServer.handleClient();
    
    // Handle websocket events
    webSocket.loop();

    // Update switch debouncers
    debouncer_x_home.update();
    debouncer_y_left_home.update();
    debouncer_y_right_home.update();
    debouncer_z_home.update();
    debouncer_pnp_cycle_button.update();

    // Static variables to track state changes for logging
    static bool lastMovingState = false;
    static bool lastHomingState = false;
    static bool lastPnPMode = false;
    
    // Check if any state has changed
    if (lastMovingState != isMoving || lastHomingState != isHoming || lastPnPMode != inPickPlaceMode) {
        Serial.printf("[STATE-DEBUG] State change: isMoving:%d->%d, isHoming:%d->%d, inPickPlaceMode:%d->%d\n",
                    lastMovingState, isMoving, lastHomingState, isHoming, lastPnPMode, inPickPlaceMode);
        lastMovingState = isMoving;
        lastHomingState = isHoming;
        lastPnPMode = inPickPlaceMode;
    }

    // PnP move tracking variable
    static bool stepMovementInProgress = false;
    static unsigned long lastStepMoveCheck = 0;
    static bool statusSent = false;

    // Check stepper motors (if they've completed their moves)
    // Only check every 200ms to avoid excessive serial output
    if (millis() - lastStepMoveCheck > 200) {
        lastStepMoveCheck = millis();
        
        // X/Y movement checking - non-calibration mode
        if (isMoving && !inCalibrationMode) {
            // Check if steppers are still running
            bool xRunning = stepper_x->isRunning();
            bool yLeftRunning = stepper_y_left->isRunning();
            bool yRightRunning = stepper_y_right->isRunning();
            bool zRunning = stepper_z->isRunning();
            bool rotRunning = stepper_rot ? stepper_rot->isRunning() : false;
            
            if (inPickPlaceMode && stepMovementInProgress) {
                bool stillRunning = xRunning || yLeftRunning || yRightRunning || zRunning;
                
                if (!stillRunning && !statusSent) {
                    Serial.println("[DEBUG-LOOP] PnP move completed. Motors stopped.");
                    stepMovementInProgress = false;
                    
                    // This is commented out because the isMoving flag should be managed by the executeNextPickPlaceStep function
                    // But if it's not being cleared, this might be necessary
                    // isMoving = false; 
                }
            }
            else if (!xRunning && !yLeftRunning && !yRightRunning && !zRunning && !rotRunning) {
                // All steppers have stopped
                Serial.println("[DEBUG-LOOP] Movement completed. All motors stopped.");
                isMoving = false;  // Clear moving flag
                
                if (inPickPlaceMode) {
                    webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"Ready for next PnP step.\"}");
                } else {
                    webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Move complete. Machine ready.\"}");
                }
                
                // Send position update after move completes
                sendCurrentPositionUpdate();
                statusSent = true;
            } else {
                // At least one motor is still running
                statusSent = false;
                if (inPickPlaceMode) {
                    stepMovementInProgress = true;
                }
            }
        }
        
        // Check for end of homing
        if (isHoming) {
            if (!stepper_x->isRunning() && !stepper_y_left->isRunning() && !stepper_y_right->isRunning() && !stepper_z->isRunning()) {
                isHoming = false;
                Serial.println("[DEBUG-LOOP] Homing sequence completed.");
                allHomed = true;
                
                // Send ready status
                webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Homing complete.\"}");
                
                // Send position update after homing
                sendCurrentPositionUpdate();
            }
        }
    }
    
    // Check physical button press for PnP cycle (if implemented)
    if (debouncer_pnp_cycle_button.rose()) { // Button pressed (active HIGH - LOW to HIGH edge)
        if (inPickPlaceMode && !isMoving && !isHoming) {
            Serial.println("Physical PnP Cycle button pressed, executing next step");
            executeNextPickPlaceStep();
        }
    }

    // Check for auto-retry of homing after exiting PnP mode
    if (pendingHomingAfterPnP && !isMoving && !isHoming && !inPickPlaceMode) {
        Serial.println("Auto-homing after PnP exit");
        pendingHomingAfterPnP = false; // Clear flag first to avoid recursion
        homeAllAxes();
    }
} 