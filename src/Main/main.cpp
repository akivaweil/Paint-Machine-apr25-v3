#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include <WiFi.h>        // Added for WiFi
#include <ArduinoOTA.h>  // Added for OTA
#include <Preferences.h> // Added for NVS settings
#include "GeneralSettings_PinDef.h" // Updated include
#include "../PickPlace/PickPlace.h" // Include the new PnP header
#include "../Painting/Painting.h" // Include the new Painting header
#include "Web/WebHandler.h" // Include the new Web Handler header

// === Global Variable Definitions ===

// Painting Specific Settings (Arrays) - DEFINITIONS
float paintZHeight_inch[4] = {1.0, 1.0, 1.0, 1.0};
int paintPitchAngle[4] = {SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH}; // Use defined constant from header (Replaced PITCH_SERVO_MAX)
int paintPatternType[4] = {0, 90, 0, 90}; // Default: Back/Front=Up-Down(0), Left/Right=Left-Right(90)
float paintSpeed[4] = {10000.0, 10000.0, 10000.0, 10000.0};

// Define WiFi credentials (Reverted to hardcoded for now)
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";
const char* hostname = "paint-machine"; // ADDED Definition

// Preferences object for NVS
Preferences preferences;

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

// State Variables
bool allHomed = false; // Tracks if initial homing completed successfully
volatile bool isMoving = false; // Tracks if a move command is active
volatile bool isHoming = false; // Tracks if homing sequence is active
volatile bool inPickPlaceMode = false; // Tracks if PnP sequence is active
volatile bool pendingHomingAfterPnP = false; // Flag to home after exiting PnP
volatile bool inCalibrationMode = false; // Tracks if calibration mode is active
volatile bool stopRequested = false; // <<< ADDED: Flag to signal stop request

// NEW: Tray Dimension Variables
float trayWidth_inch = 18.0; // Default tray width - UPDATED
float trayHeight_inch = 26.0; // Default tray height - UPDATED

// Pattern/General Speed/Accel Variables (declared extern in GeneralSettings_PinDef.h)
float patternXSpeed = 20000.0; // Actual value
float patternXAccel = 20000.0; // Actual value
float patternYSpeed = 20000.0; // Actual value
float patternYAccel = 20000.0; // Actual value
float patternZSpeed = 5000.0; // Changed from 500.0
float patternZAccel = 13000.0; // Changed from 1300.0 (scaled 10x)
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
float paintPatternOffsetX_inch = 0.0f;
float paintPatternOffsetY_inch = 0.0f;
float paintGunOffsetX_inch = 0.0f;   // Offset of nozzle from TCP X
float paintGunOffsetY_inch = 1.5f;   // Offset of nozzle from TCP Y (e.g., 1.5 inches forward)

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
void calculateAndSetGridSpacing(int cols, int rows); // Defined later in this file
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void sendCurrentPositionUpdate(); // Forward declaration for position updates
void sendAllSettingsUpdate(uint8_t specificClientNum, String message); // Helper to send all settings
void saveSettings(); // Defined above
void loadSettings(); // Defined above
void setPitchServoAngle(int angle);
void movePitchServoSmoothly(int targetAngle);

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
        // Z-axis moves UP to home (Z=0). Since positive steps move DOWN, use runBackward().
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
            if (debouncer_z_home.read() == HIGH) { // Switch is active high
                stepper_z->stopMove();
                stepper_z->forceStop();
                stepper_z->setCurrentPosition(0); // Correct: Home position is 0
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

    // === Z Limit Check (Apply constraints BEFORE converting to steps) ===
    // Constrain target_Z_inch between 0.0 and 2.75
    targetZ_inch = max(Z_HOME_POS_INCH, targetZ_inch);          // Don't go above Z=0
    targetZ_inch = min(Z_MAX_TRAVEL_POS_INCH, targetZ_inch); // Don't go below Z=2.75
    // === End Z Limit Check ===

    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z); // Convert constrained value

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
    // REMOVED Check: if (isMoving || isHoming || inPickPlaceMode || inCalibrationMode)
    // The calling function (e.g., paintSide) is responsible for managing the overall machine state.

    if (!stepper_z) {
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Z Stepper not initialized.\"}");
         return;
    }

    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z);
    
    // === Z Limit Check (only for Pick & Place mode - though this func is mainly for painting now) ===
    // This check might be less relevant if this function is only called during painting (where inPickPlaceMode is false)
    // Kept for potential other uses, but review if Z limits are needed outside PnP.
    if (inPickPlaceMode) { 
        long min_z_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z); // Should be 0
        long max_z_steps = (long)(Z_MAX_TRAVEL_POS_INCH * STEPS_PER_INCH_Z);
        targetZ_steps = max(min_z_steps, targetZ_steps); // Ensure not below min travel (which is 0)
        targetZ_steps = min(max_z_steps, targetZ_steps); // Ensure not above max travel (e.g., 2.75)
    }
    // === End Z Limit Check ===

    long currentZ_steps = stepper_z->getCurrentPosition();

    if (targetZ_steps == currentZ_steps) {
        // Serial.println("Z axis already at target.");
        return; // Already there
    }

    // REMOVED internal isMoving = true;
    // Serial.printf("Moving Z from %.2f to %.2f inches (Steps: %ld to %ld, Speed: %.0f, Accel: %.0f)\n",
    //               (float)currentZ_steps / STEPS_PER_INCH_Z, targetZ_inch, currentZ_steps, targetZ_steps, speedHz, accel);

    stepper_z->setSpeedInHz(speedHz);
    stepper_z->setAcceleration(accel);
    stepper_z->moveTo(targetZ_steps);

    // Wait for Z move completion (blocking)
    while (stepper_z->isRunning() && !stopRequested) { // <<< MODIFIED: Added !stopRequested check
        webSocket.loop(); // Keep websocket alive
        yield();
    }
    // Serial.println("Z move complete.");
    // REMOVED internal isMoving = false;
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
    while ((stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) && !stopRequested) { // <<< MODIFIED: Added !stopRequested check
        yield();
    }
    // Serial.println("Paint XY move complete.");
}

// --- NEW: Rotation Function ---
// Moves the rotation axis to a specific absolute degree position and waits
void rotateToAbsoluteDegree(int targetDegree) {
    if (!stepper_rot) {
        Serial.println("[WARN] rotateToAbsoluteDegree: Rotation stepper not available.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Rotation control unavailable (pin conflict?)\"}");
        return;
    }
    if (isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
        Serial.printf("[WARN] rotateToAbsoluteDegree: Cannot rotate while busy (state: Mv=%d, Hm=%d, PnP=%d, Cal=%d).\n",
                      isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot rotate: Machine is busy.\"}");
        return;
    }

    long targetSteps = (long)round(targetDegree * STEPS_PER_DEGREE);
    long currentSteps = stepper_rot->getCurrentPosition();
    float currentAngle = (float)currentSteps / STEPS_PER_DEGREE;

    if (abs(targetSteps - currentSteps) < 2) { // Check if already at target (within tolerance)
        Serial.printf("Rotation already at target %.1f degrees.\n", (float)targetSteps / STEPS_PER_DEGREE);
        return; // Already there
    }

    Serial.printf("Rotating from %.1f deg to %d deg (Steps: %ld to %ld)\n", currentAngle, targetDegree, currentSteps, targetSteps);
    isMoving = true; // Set machine busy
    char rotMsg[100];
    sprintf(rotMsg, "{\"status\":\"Moving\", \"message\":\"Rotating tray to %d degrees...\"}", targetDegree);
    webSocket.broadcastTXT(rotMsg);

    // Set speed and acceleration (use defaults from GeneralSettings_PinDef.h)
    stepper_rot->setSpeedInHz(patternRotSpeed); 
    stepper_rot->setAcceleration(patternRotAccel);
    stepper_rot->moveTo(targetSteps); // Move to absolute step position

    // Wait for rotation completion (blocking)
    while (stepper_rot->isRunning() && !stopRequested) { 
        webSocket.loop(); // Keep WebSocket responsive
        yield();
    }

    if (stopRequested) {
        Serial.println("STOP requested during rotation.");
        // isMoving will be reset by the main STOP handler
        // webSocket status will be updated by the main STOP handler
    } else {
        Serial.printf("Rotation to %d degrees complete. Final steps: %ld\n", targetDegree, stepper_rot->getCurrentPosition());
        isMoving = false; // Clear busy flag ONLY if not stopped
        // Send 'Ready' status after rotation is complete (unless stopped)
        char readyMsg[100];
        sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Rotation to %d degrees complete.\"}", targetDegree);
        webSocket.broadcastTXT(readyMsg); 
        sendCurrentPositionUpdate(); // Update position display
    }
}
// --- End NEW Rotation Function ---

// --- Painting Logic ---

void paintSide(int sideIndex) {
    Serial.printf("Starting paintSide for side %d\n", sideIndex);
    stopRequested = false; // <<< ADDED: Reset stop flag at start

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

    // 3. Get Side Parameters (Use parameters for the *requested* sideIndex)
    float zHeight = paintZHeight_inch[sideIndex];
    int pitch = paintPitchAngle[sideIndex];
    float speed = paintSpeed[sideIndex]; // Speed in steps/s
    float accel = patternXAccel; // Assuming X/Y use same accel for painting

    // 4. Optional: Rotate (Example: Rotate based on sideIndex if needed - ADD LATER IF REQUIRED)
    // ... Rotation logic if necessary ...
    // delay(100); // Small delay after rotation

    // 5. Set Servo Angles
    Serial.printf("Setting servos for Side %d: Pitch=%d\n", sideIndex, pitch);
    servo_pitch.write(pitch);
    delay(300); // Allow servos to settle

    // 6. Define Path Reference Points and Spacing
    float backStartX = placeFirstXAbsolute_inch + paintGunOffsetX_inch + 0.25; // Top-right corner ref for Back
    float backStartY = placeFirstYAbsolute_inch + paintGunOffsetY_inch;       // Top-right corner ref for Back
    float rowSpacing = 3.0 + placeGapY_inch; // Vertical distance between rows (fixed for back/front)
    float colSpacing = (placeGridCols > 1) ? (trayWidth_inch / (placeGridCols - 1)) : 0; // Horizontal distance between columns

    Serial.printf("Path References: BackStart(%.2f, %.2f), GunOffset(%.2f, %.2f), RowSpacing(%.2f), ColSpacing(%.2f)\n",
                  backStartX, backStartY, paintGunOffsetX_inch, paintGunOffsetY_inch, rowSpacing, colSpacing);

    // 7. Move to Start Z Height
    Serial.println("Moving to start Z height...");
    moveZToPositionInches(zHeight, patternZSpeed, patternZAccel); // Use sideIndex's zHeight
    // <<< ADDED: Check for stop request after initial Z move >>>
    if (stopRequested) {
        Serial.println("STOP requested. Aborting paintSide.");
        isMoving = false; 
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}"); 
        return;
    }

    // 8. Calculate Start Point and Execute Painting Path based on selected pattern

    float startTcpX, startTcpY; // Where the painting pattern *actually* starts for this side
    float currentTcpX, currentTcpY; // Current TCP position during pattern execution

    // Decide start point - same for both patterns initially
    // Start at the calculated top-right corner reference point
    startTcpX = backStartX;
    startTcpY = backStartY;
    Serial.printf("Moving TCP to start of pattern (Top-Right Ref): (%.2f, %.2f)\n", startTcpX, startTcpY);
    moveToXYPositionInches_Paint(startTcpX, startTcpY, speed, accel);
    // <<< Check for stop request >>>
    if (stopRequested) {
        Serial.println("STOP requested. Aborting paintSide.");
        isMoving = false; 
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}"); 
        return;
    }

    currentTcpX = startTcpX;
    currentTcpY = startTcpY;

    // Execute painting path based on selected pattern type for this side
    int pattern = paintPatternType[sideIndex];
    
    if (pattern == 0) { // --- Up-Down Pattern (Back/Front sides) ---
        Serial.printf("[DEBUG paintSide] Setting up for Up-Down Pattern (Side %d)\n", sideIndex);
        // Spacing
        colSpacing = pnpItemWidth_inch + placeGapX_inch; // Horizontal shift distance
        rowSpacing = pnpItemHeight_inch + placeGapY_inch; // Vertical sweep distance

        // --- Determine Start Point and Direction based on Side ---
        bool movingDown; // Vertical direction
        float startRefX; // Horizontal reference for loop

        if (sideIndex == 2) { // FRONT SIDE (Reverse of Back)
            // Start at the calculated *end* X of the back pattern
            startRefX = placeFirstXAbsolute_inch + paintGunOffsetX_inch + 0.25 - ((placeGridCols - 1) * colSpacing);
            startTcpX = startRefX; 
            startTcpY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // Start at the top Y
            movingDown = false; // Start moving UP (opposite of Back)
            Serial.printf("  FRONT Pattern (Side 2): Start(%.3f, %.3f), ColSpace=%.3f, RowSpace=%.3f\n", startTcpX, startTcpY, colSpacing, rowSpacing);
        } else { // BACK SIDE (or default Up-Down)
            // Original start point (Top-right reference)
            startRefX = placeFirstXAbsolute_inch + paintGunOffsetX_inch + 0.25;
            startTcpX = startRefX;
            startTcpY = placeFirstYAbsolute_inch + paintGunOffsetY_inch;
            movingDown = true; // Start moving DOWN
            Serial.printf("  BACK Pattern (Side 0): Start(%.3f, %.3f), ColSpace=%.3f, RowSpace=%.3f\n", startTcpX, startTcpY, colSpacing, rowSpacing);
        }

        // Move to calculated start XY
        Serial.printf("Moving TCP to start of Up-Down pattern: (%.3f, %.3f)\n", startTcpX, startTcpY);
        moveToXYPositionInches_Paint(startTcpX, startTcpY, speed, accel);
        if (stopRequested) { /* Abort */ isMoving = false; webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}"); return; }

        float currentTcpX = startTcpX;
        float currentTcpY = startTcpY;

        // Execute Up-Down Path - Directly move, no pre-calculation
        Serial.printf("Executing Up-Down Pattern for Side %d\n", sideIndex);
        for (int c = 0; c < placeGridCols; ++c) {
            float targetTcpX;
            if (sideIndex == 2) { // FRONT - Move Right
                targetTcpX = startRefX + (c * colSpacing);
            } else { // BACK - Move Left
                targetTcpX = startRefX - (c * colSpacing);
            }

            // Move horizontally if needed (avoid moving for the very first column)
            if (c > 0 && abs(currentTcpX - targetTcpX) > 0.001) {
                Serial.printf("  Col %d: Moving Horizontally to X=%.3f\n", c, targetTcpX);
                moveToXYPositionInches_Paint(targetTcpX, currentTcpY, speed, accel);
                if (stopRequested) { /* Abort */ isMoving = false; webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}"); return; }
                currentTcpX = targetTcpX;
            }
            
            // Determine vertical target Y (full sweep)
            float targetTcpY = movingDown ? (startTcpY - (placeGridRows - 1) * rowSpacing) : startTcpY;
            
            // Move vertically only if not the first pass OR if the first pass needs to move (e.g., if starting at bottom)
            // Simplified: Always move vertically after the horizontal positioning for the column.
            Serial.printf("  Col %d (%s): Sweeping Vertically to Y=%.3f\n", c, movingDown ? "Down" : "Up", targetTcpY);
            moveToXYPositionInches_Paint(currentTcpX, targetTcpY, speed, accel);
            if (stopRequested) { /* Abort */ isMoving = false; webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}"); return; }
            currentTcpY = targetTcpY;
            
            // Flip vertical direction for the next pass
            movingDown = !movingDown; 
        }
    } else { // --- Left-Right Pattern (Left/Right sides) ---
        Serial.printf("[DEBUG paintSide] Setting up for Left-Right Pattern (Side %d)\n", sideIndex);
        // Start Point (NEW logic requested by user)
        // ... existing code ...
    }

    // 9. Painting Complete - Return Z to home
    Serial.println("Painting pattern complete. Returning Z to home.");

    // 10. Move Z Axis Up (e.g., to safe height 0)
    Serial.println("Moving Z axis up to safe height (0)...");
    moveZToPositionInches(0.0, patternZSpeed, patternZAccel);
    if (stopRequested) {
        Serial.println("STOP requested. Aborting paintSide after Z move.");
        isMoving = false;
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}");
        return;
    }

    // 11. Return to home position (X=0, Y=0, Z=0)
    Serial.println("Returning to home position (X=0, Y=0, Z=0)...");
    isMoving = false; // Temporarily clear flag for the move function
    moveToPositionInches(0.0, 0.0, 0.0);
    isMoving = true; // Set moving flag again for the wait loop
    while ((stepper_x && stepper_x->isRunning()) || (stepper_y_left && stepper_y_left->isRunning()) || (stepper_y_right && stepper_y_right->isRunning()) || (stepper_z && stepper_z->isRunning())) {
        webSocket.loop();
        yield();
        if (stopRequested) {
            Serial.println("STOP requested. Aborting paintSide during return to home.");
            isMoving = false;
            webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}");
            return;
        }
    }
    Serial.println("Return to home complete.");

    // 12. Clear Busy State & Send Ready Message
    isMoving = false; // <<< Ensure isMoving is false before sending final status
    char readyMsg[100];
    sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Painting Side %d complete. Returned to home.\"}", sideIndex);
    webSocket.broadcastTXT(readyMsg);
    Serial.println(readyMsg);

    sendCurrentPositionUpdate(); // Send final position
}

// --- Arduino Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Booting Paint + PnP Machine ==="); // Clearer boot message
    Serial.println("Loading settings from NVS...");

    // Load settings from NVS
    loadSettings();

    // Calculate initial grid gap based on potentially loaded dimensions
    Serial.printf("[DEBUG] setup: pnpOffsetX after loadSettings() = %.2f\n", pnpOffsetX_inch);
    calculateAndSetGridSpacing(placeGridCols, placeGridRows);
    
    // Print loaded settings
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
        Serial.printf("  Side %d: Z=%.2f, Pitch=%d, Speed=%.0f\n",
                      i, paintZHeight_inch[i], paintPitchAngle[i], paintSpeed[i]);
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

    // Attach servos to pins using default min/max pulse widths (500, 2400 us)
    Serial.printf("[DEBUG Setup] Attaching pitch servo to pin %d\n", PITCH_SERVO_PIN); // DEBUG
    servo_pitch.attach(PITCH_SERVO_PIN);
    Serial.printf("[DEBUG Setup] Servo attached status: %d\n", servo_pitch.attached()); // <<< ADDED DEBUG

    // Move servos to initial max positions
    Serial.printf("[DEBUG Setup] Setting initial Pitch Servo position to %d\n", SERVO_INIT_POS_PITCH); // DEBUG (Replaced PITCH_SERVO_MAX)
    servo_pitch.write(SERVO_INIT_POS_PITCH); // Use defined init position (Replaced PITCH_SERVO_MAX)
    delay(500); // Allow servo to reach position

    // Add a simple servo test sequence if needed
    int currentPitch = servo_pitch.read();
    // Serial.printf("[DEBUG Setup] Current actual pitch read: %d\n", currentPitch);
    // int testDownPosition = currentPitch + 20; // Example: Try to move down 20 deg
    // int testUpPosition = currentPitch - 20;   // Example: Try to move up 20 deg
    // testDownPosition = min(180, testDownPosition); // Use 180 as general max
    // // testUpPosition = max(PITCH_SERVO_MIN, testUpPosition); // << COMMENTED OUT - Limit removed
    // testUpPosition = max(0, testUpPosition); // Use 0 as general min
    // 
    // Serial.printf("[DEBUG Setup] Testing servo: moving towards %d...\n", testDownPosition);
    // servo_pitch.write(testDownPosition);
    // delay(1000);
    // Serial.printf("[DEBUG Setup] Testing servo: moving towards %d...\n", testUpPosition);
    // servo_pitch.write(testUpPosition);
    // delay(1000);
    // Serial.printf("[DEBUG Setup] Testing servo: returning to init %d...\n", SERVO_INIT_POS_PITCH);
    // servo_pitch.write(SERVO_INIT_POS_PITCH); // Return to init pos (Replaced PITCH_SERVO_MAX)
    // delay(1000);
    // Serial.println("[DEBUG Setup] Servo test complete.");

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

        // --- Web Server & WebSocket Setup ---
        setupWebServerAndWebSocket(); // Setup web server only if WiFi connected (function in WebHandler.cpp)

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
    
    // Check for pin conflict with Z Step Pin
    if (ROTATION_STEP_PIN == Z_STEP_PIN) {
        Serial.println("[WARN] Rotation Step Pin conflicts with Z Step Pin! Disabling rotation stepper.");
        stepper_rot = NULL; // Keep stepper_rot as NULL to disable it
    } else {
        // Initialize stepper_rot only if pins do NOT conflict
        stepper_rot = engine.stepperConnectToPin(ROTATION_STEP_PIN);
        if (stepper_rot) {
            Serial.println("DEBUG: Rotation stepper created successfully");
            stepper_rot->setDirectionPin(ROTATION_DIR_PIN);
            stepper_rot->setEnablePin(-1);
            stepper_rot->setAutoEnable(false);
            stepper_rot->setCurrentPosition(0);
        } else {
            Serial.println("DEBUG: ERROR - Failed to initialize rotation stepper (non-conflict scenario)!");
            stepper_rot = NULL;
        }
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

    // Send update to UI
    String message = "Grid Columns/Rows updated. Gap calculated.";
    if (fitError) {
        message += " Warning: Items may not fit within tray dimensions!";
    }
    
    Serial.printf("[DEBUG] Sending update to UI with message: %s\n", message.c_str());
    sendAllSettingsUpdate(255, message); // Send to all clients
} 

// Function to save all configurable settings to NVS
void saveSettings() {
    Serial.println("[DEBUG] saveSettings() started.");
    preferences.begin("machineCfg", false); // Open namespace in read/write mode

    // PnP Settings
    preferences.putFloat("pnpOffsetX", pnpOffsetX_inch);
    preferences.putFloat("pnpOffsetY", pnpOffsetY_inch);
    preferences.putFloat("placeFirstX", placeFirstXAbsolute_inch);
    preferences.putFloat("placeFirstY", placeFirstYAbsolute_inch);
    preferences.putInt("gridCols", placeGridCols);
    preferences.putInt("gridRows", placeGridRows);
    preferences.putFloat("trayWidth", trayWidth_inch);
    preferences.putFloat("trayHeight", trayHeight_inch);
    // Note: Gap is calculated, not saved directly

    // Speed/Accel Settings
    preferences.putFloat("patXSpeed", patternXSpeed);
    preferences.putFloat("patXAccel", patternXAccel);
    preferences.putFloat("patYSpeed", patternYSpeed);
    preferences.putFloat("patYAccel", patternYAccel);
    // Z/Rot Speed/Accel are not currently set via UI, so not saving

    // Painting Offsets
    preferences.putFloat("paintPatOffX", paintPatternOffsetX_inch);
    preferences.putFloat("paintPatOffY", paintPatternOffsetY_inch);
    preferences.putFloat("paintGunOffX", paintGunOffsetX_inch);
    preferences.putFloat("paintGunOffY", paintGunOffsetY_inch);

    // Paint Side Settings
    for (int i = 0; i < 4; ++i) {
        String keyZ = "paintZ_" + String(i);
        String keyP = "paintP_" + String(i);
        String keyR = "paintR_" + String(i); // Use R for Pattern
        String keyS = "paintS_" + String(i);
        preferences.putFloat(keyZ.c_str(), paintZHeight_inch[i]);
        preferences.putInt(keyP.c_str(), paintPitchAngle[i]);
        preferences.putInt(keyR.c_str(), paintPatternType[i]);
        preferences.putFloat(keyS.c_str(), paintSpeed[i]);
    }

    preferences.end();
    Serial.println("[INFO] Settings saved to NVS.");
    Serial.println("[DEBUG] saveSettings() finished.");
}

// Function to load all configurable settings from NVS
void loadSettings() {
    Serial.println("[DEBUG] loadSettings() started.");
    preferences.begin("machineCfg", true); // Open namespace in read-only mode

    // PnP Settings
    pnpOffsetX_inch = preferences.getFloat("pnpOffsetX", 15.0f);
    pnpOffsetY_inch = preferences.getFloat("pnpOffsetY", 0.0f);
    placeFirstXAbsolute_inch = preferences.getFloat("placeFirstX", 20.0f);
    placeFirstYAbsolute_inch = preferences.getFloat("placeFirstY", 20.0f);
    placeGridCols = preferences.getInt("gridCols", 4);
    placeGridRows = preferences.getInt("gridRows", 5);
    trayWidth_inch = preferences.getFloat("trayWidth", 24.0f);
    trayHeight_inch = preferences.getFloat("trayHeight", 18.0f);

    // Speed/Accel Settings
    patternXSpeed = preferences.getFloat("patXSpeed", 20000.0f);
    patternXAccel = preferences.getFloat("patXAccel", 20000.0f);
    patternYSpeed = preferences.getFloat("patYSpeed", 20000.0f);
    patternYAccel = preferences.getFloat("patYAccel", 20000.0f);

    // Painting Offsets
    paintPatternOffsetX_inch = preferences.getFloat("paintPatOffX", 0.0f);
    paintPatternOffsetY_inch = preferences.getFloat("paintPatOffY", 0.0f);
    paintGunOffsetX_inch = preferences.getFloat("paintGunOffX", 0.0f);
    paintGunOffsetY_inch = preferences.getFloat("paintGunOffY", 1.5f);

    // Paint Side Settings
    for (int i = 0; i < 4; ++i) {
        String keyZ = "paintZ_" + String(i);
        String keyP = "paintP_" + String(i);
        String keyR = "paintR_" + String(i); // Use R for Pattern
        String keyS = "paintS_" + String(i);
        paintZHeight_inch[i] = preferences.getFloat(keyZ.c_str(), 1.0f);
        paintPitchAngle[i] = preferences.getInt(keyP.c_str(), SERVO_INIT_POS_PITCH); // Load mapped servo value (Replaced PITCH_SERVO_MAX w/ init pos)
        paintPatternType[i] = preferences.getInt(keyR.c_str(), (i % 2 == 0) ? 0 : 90); // Load pattern, default based on side
        paintSpeed[i] = preferences.getFloat(keyS.c_str(), 10000.0f);
    }

    preferences.end();
    Serial.println("[INFO] Settings loaded from NVS.");
    // Log a sample value
    Serial.printf("[DEBUG] loadSettings: Loaded pnpOffsetX = %.2f\n", pnpOffsetX_inch);
    Serial.println("[DEBUG] loadSettings() finished.");
}

// Function to set the pitch servo angle directly
void setPitchServoAngle(int angle) {
    Serial.printf("[DEBUG setPitchServoAngle] Received angle: %d\n", angle); // <<< ADDED DEBUG
    // Ensure angle is within the servo's physical limits (0-180 typical)
    // Even without software limits, it's good practice to clamp to the standard range
    int targetAngle = constrain(angle, 0, 180); 
    Serial.printf("[DEBUG setPitchServoAngle] Clamped targetAngle: %d\n", targetAngle); // <<< ADDED DEBUG

    if (servo_pitch.attached()) {
        Serial.printf("[DEBUG setPitchServoAngle] Servo IS attached. Writing angle %d\n", targetAngle); // <<< ADDED DEBUG
        servo_pitch.write(targetAngle);
        delay(15); // Small delay to allow servo to start moving
    } else {
        Serial.println("[ERROR setPitchServoAngle] Pitch servo NOT attached!"); // <<< MODIFIED DEBUG
    }
}

// Function to smoothly move the pitch servo to a target angle
void movePitchServoSmoothly(int targetAngle) {
    // ... existing code ...
}

