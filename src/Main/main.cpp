#include <Arduino.h>
#include <FastAccelStepper.h>
#include <Bounce2.h>
#include <WiFi.h>        // Added for WiFi
#include <ArduinoOTA.h>  // Added for OTA
#include <Preferences.h> // Added for NVS settings
#include "GeneralSettings_PinDef.h" // Updated include
#include "../PickPlace/PickPlace.h" // Include the new PnP header
#include "../Painting/Painting.h" // Include the new Painting header
#include "../Painting/PaintGunControl.h" // Include the Paint Gun Control header
#include "Web/WebHandler.h" // Include the new Web Handler header
#include <ArduinoJson.h>
#include "../Painting/Patterns/PredefinedPatterns.h" // <<< ADDED NEW INCLUDE
#include "SharedGlobals.h" // <<< ADDED INCLUDE
#include "../PickPlace/PickPlace.h" // <<< CORRECTED PATH
#include "../Painting/Patterns/PatternActions.h"
#include "../Painting/Patterns/PaintPatterns_SideSpecific.h" // <<< INCLUDE NEW HEADER
#include "../Web/WebHandler.h"

// === Global Variable Definitions ===

// PnP Item Dimensions (NEW - Defined here, declared extern in SharedGlobals.h)
const float pnpItemWidth_inch = 3.0f;
const float pnpItemHeight_inch = 3.0f;
const float pnpBorderWidth_inch = 0.25f; // Border around grid

// Rotation constants defined in Painting.h - extern declared in SharedGlobals.h

// Painting Specific Settings (Arrays) - DEFINITIONS
float paintZHeight_inch[4] = {1.0, 1.0, 1.0, 1.0};
int paintPitchAngle[4] = {SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH}; // Use defined constant from header (Replaced PITCH_SERVO_MAX)
int paintPatternType[4] = {0, 90, 0, 90}; // Default: Back/Front=Up-Down(0), Left/Right=Sideways(90)
float paintSpeed[4] = {10000.0, 10000.0, 10000.0, 10000.0};

// NEW: Painting Start Positions (X, Y) for each side [Back, Right, Front, Left]
float paintStartX[4] = { 11.5f, 29.5f, 11.5f, 8.0f }; // Defaults based on current logic
float paintStartY[4] = { 20.5f, 20.0f, 0.5f,  6.5f }; // Defaults based on current logic

// Painting State Machine Variables (NEW)
volatile bool isPainting = false;      // Flag indicating active painting
volatile int currentPaintStep = 0;     // Current step in the painting process
volatile int currentPaintSide = -1;    // Current side being painted (-1 = none)
volatile bool isPaintSequence = false; // Flag for multi-side painting sequence
volatile bool paintNextSide = false;   // Signal to move to the next side

// Pick and Place Specific Locations - DEFINITIONS
float pnpPickLocationX_inch = 2.0f; // Default pick location X
float pnpPickLocationY_inch = 2.0f; // Default pick location Y
float pnpPickLocationZ_inch = 2.0f; // Default pick location Z (clearance height)
float pnpPlaceHeight_inch   = 0.5f; // Default place Z height

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
void setDefaultSettings(); // New helper function to set defaults
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
    
    // Ensure paint gun is deactivated before homing begins
    deactivatePaintGun(true);
    
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
    bool rot_done = false;  // Add rotation done flag

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

    // Rotation Axis - Set to zero position
    if (stepper_rot) {
        // For rotation, we just set it to position 0 rather than using a home switch
        stepper_rot->setSpeedInHz(patternRotSpeed);
        stepper_rot->setAcceleration(patternRotAccel);
        
        // If rotation motor is not already at zero, move it to zero
        long currentPos = stepper_rot->getCurrentPosition();
        if (currentPos != 0) {
            Serial.printf("Homing rotation motor from position %ld to 0\n", currentPos);
            stepper_rot->moveTo(0);
        } else {
            rot_done = true; // Already at zero position
        }
    } else {
        rot_done = true; // No rotation stepper, so consider it done
    }

    // Wait for all axes to home or timeout
    unsigned long startTime = millis();
    
    // Monitor all switches and stop once triggered
    while (!(x_done && y_left_done && y_right_done && z_done && rot_done)) {
        if (millis() - startTime > HOMING_TIMEOUT) {
            // Serial.println("Homing timed out!");
            if (stepper_x && !x_done) stepper_x->forceStop();
            if (stepper_y_left && !y_left_done) stepper_y_left->forceStop();
            if (stepper_y_right && !y_right_done) stepper_y_right->forceStop();
            if (stepper_z && !z_done) stepper_z->forceStop();
            if (stepper_rot && !rot_done) stepper_rot->forceStop();
            
            webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Homing timed out!\"}");
            break;
        }

        debouncer_x_home.update();
        debouncer_y_left_home.update();
        debouncer_y_right_home.update();
        debouncer_z_home.update();
        webSocket.loop(); // Keep WebSocket responsive during blocking operations

        // X Axis
        if (!x_done && stepper_x) {
            if (debouncer_x_home.read() == HIGH) {
                stepper_x->stopMove();
                stepper_x->forceStop();
                stepper_x->setCurrentPosition(0);
                x_homed = true;
                x_done = true;
                // Serial.println("X axis homed.");
            } else if (!stepper_x->isRunning()) {
                x_done = true; // X is done, but not properly homed
                // Serial.println("X axis stopped without hitting switch.");
            }
        }

        // Y Left Axis
        if (!y_left_done && stepper_y_left) {
            if (debouncer_y_left_home.read() == HIGH) {
                stepper_y_left->stopMove();
                stepper_y_left->forceStop();
                stepper_y_left->setCurrentPosition(0);
                y_left_homed = true;
                y_left_done = true;
                // Serial.println("Y Left axis homed.");
            } else if (!stepper_y_left->isRunning()) {
                y_left_done = true; // Y Left is done, but not properly homed
                // Serial.println("Y Left axis stopped without hitting switch.");
            }
        }

        // Y Right Axis
        if (!y_right_done && stepper_y_right) {
            if (debouncer_y_right_home.read() == HIGH) {
                stepper_y_right->stopMove();
                stepper_y_right->forceStop();
                stepper_y_right->setCurrentPosition(0);
                y_right_homed = true;
                y_right_done = true;
                // Serial.println("Y Right axis homed.");
            } else if (!stepper_y_right->isRunning()) {
                y_right_done = true; // Y Right is done, but not properly homed
                // Serial.println("Y Right axis stopped without hitting switch.");
            }
        }

        // Z Axis
        if (!z_done && stepper_z) {
            if (debouncer_z_home.read() == HIGH) {
                stepper_z->stopMove();
                stepper_z->forceStop();
                stepper_z->setCurrentPosition(0);
                z_homed = true;
                z_done = true;
                // Serial.println("Z axis homed.");
            } else if (!stepper_z->isRunning()) {
                z_done = true; // Z is done, but not properly homed
                // Serial.println("Z axis stopped without hitting switch.");
            }
        }
        
        // Rotation Axis (no switch, just check if it's done moving)
        if (!rot_done && stepper_rot) {
            if (!stepper_rot->isRunning()) {
                // Rotation has stopped, mark as done
                rot_done = true;
                Serial.println("Rotation axis homed to position 0.");
            }
        }

        yield(); // Allow background tasks
    }

    // Check if all homing was successful
    bool all_success = x_homed && y_left_homed && y_right_homed && z_homed && rot_done;

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
        if (!rot_done) failedAxes += " Rotation";
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Homing Failed for: " + failedAxes + "\"}");
        allHomed = false;
    }

    isHoming = false;

    // Set pitch servo to initial position
    // This servo controls the paint gun direction/angle
    Serial.println("Setting pitch servo to initial position");
    servo_pitch.write(SERVO_INIT_POS_PITCH);
    delay(300); // Allow servo to settle
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

    // Move Z axis (if necessary)
    bool needToMoveZ = (stepper_z && stepper_z->getCurrentPosition() != targetZ_steps);
    if (needToMoveZ) {
        stepper_z->setSpeedInHz(patternZSpeed);
        stepper_z->setAcceleration(patternZAccel);
        stepper_z->moveTo(targetZ_steps);
        // Serial.printf("  Moving Z to %ld steps\\n", targetZ_steps);
    }

    // *** REMOVED: Wait for Z to finish if it moved ***
    // Now non-blocking: loop() will detect when all motors have stopped

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

    // *** REMOVED: Wait for movement completion - now non-blocking ***
    // The loop() function will detect movement completion and reset the isMoving flag
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
    while (stepper_z->isRunning()) { // <<< REMOVED: !stopRequested check
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
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) { // <<< REMOVED: !stopRequested check
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
    
    // REMOVED CONDITION: We no longer check for isMoving which was preventing automated sequences
    // Only check for homing, pick/place and calibration modes which truly should prevent rotation
    if (isHoming || inPickPlaceMode || inCalibrationMode) {
        Serial.printf("[WARN] rotateToAbsoluteDegree: Cannot rotate while in special mode (Hm=%d, PnP=%d, Cal=%d).\n",
                     isHoming, inPickPlaceMode, inCalibrationMode);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot rotate: Machine is in special mode.\"}");
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
    
    // Set a local moving flag but don't interfere with the global isMoving flag
    // which may be set by the painting sequence
    bool wasMovingBefore = isMoving;
    if (!wasMovingBefore) {
        isMoving = true; // Only set if it wasn't already set
        char rotMsg[100];
        sprintf(rotMsg, "{\"status\":\"Moving\", \"message\":\"Rotating tray to %d degrees...\"}", targetDegree);
        webSocket.broadcastTXT(rotMsg);
    }

    // Always set speed and acceleration - ensuring the motor has proper acceleration profile
    stepper_rot->setSpeedInHz(patternRotSpeed);
    stepper_rot->setAcceleration(patternRotAccel);
    
    // Check if the move can be properly executed
    if (!stepper_rot->moveTo(targetSteps)) {
        Serial.println("[ERROR] Rotation move failed - stepper may be disabled");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Rotation failed. Motor might be disabled.\"}");
        
        // Only reset isMoving if we set it (not if it was already set)
        if (!wasMovingBefore) {
            isMoving = false;
        }
        return;
    }

    // Wait for rotation completion (blocking)
    while (stepper_rot->isRunning()) {
        webSocket.loop(); // Keep WebSocket responsive
        yield();
    }

    // After move completes
    long finalPos = stepper_rot->getCurrentPosition();
    float finalAngle = (float)finalPos / STEPS_PER_DEGREE;
    
    // Verify if move completed successfully
    if (abs(finalPos - targetSteps) > 2) { // More than 2 steps off (> 0.18 degrees)
        Serial.printf("[WARN] Rotation didn't reach exact target. Final: %ld steps (%.2f°), Target: %ld steps (%d°)\n",
                    finalPos, finalAngle, targetSteps, targetDegree);
    }

    Serial.printf("Rotation to %d degrees complete. Final steps: %ld (%.2f°)\n", 
                targetDegree, finalPos, finalAngle);
                
    // Only reset isMoving if we set it (not if it was already set)
    if (!wasMovingBefore) {
        isMoving = false;
        
        // Send 'Ready' status after rotation is complete, but only if we weren't in a sequence
        char readyMsg[100];
        sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Rotation to %d degrees complete.\"}", targetDegree);
        webSocket.broadcastTXT(readyMsg);
    }
    
    sendCurrentPositionUpdate(); // Update position display
}
// --- End NEW Rotation Function ---

// --- Painting Logic ---

// --- Start a painting operation on a specific side (can be part of a sequence)
void startPaintingSide(int sideIndex, bool isSequence) {
    Serial.printf("Starting painting for side %d (sequence: %s)\n", sideIndex, isSequence ? "true" : "false");
    stopRequested = false; // Reset stop flag at start
    
    // Validate painting request
    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode || isPainting) {
        Serial.printf("[ERROR] startPaintingSide denied: Invalid state (allHomed=%d, isMoving=%d, isHoming=%d, inPickPlaceMode=%d, inCalibrationMode=%d, isPainting=%d)\n",
                      allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode, isPainting);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Cannot start painting, invalid machine state.\"}");
        return;
    }
    
    // Validate side index
    if (sideIndex < 0 || sideIndex > 3) {
        Serial.printf("[ERROR] startPaintingSide denied: Invalid side index %d\n", sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Invalid side index provided for painting.\"}");
        return;
    }
    
    // Set new flags
    isPainting = true;
    isMoving = true;
    currentPaintSide = sideIndex;
    currentPaintStep = 0;
    isPaintSequence = isSequence;
    paintNextSide = false;
    
    // Send status message
    char busyMsg[100];
    sprintf(busyMsg, "{\"status\":\"Busy\", \"message\":\"Starting Paint Sequence for Side %d...\"}", sideIndex);
    webSocket.broadcastTXT(busyMsg);
    Serial.println(busyMsg);
}

// --- Main painting state machine - called from loop()
void processPaintingStateMachine() {
    // Only process if painting is active
    if (!isPainting) return;
    
    // Check for stop request
    if (stopRequested) {
        Serial.println("Painting operation stopped by user request");
        deactivatePaintGun(true);
        isPainting = false;
        isMoving = false;
        currentPaintSide = -1;
        currentPaintStep = 0;
        isPaintSequence = false;
        paintNextSide = false;
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting stopped by user.\"}");
        return;
    }
    
    // Check if we're waiting for motors to stop moving
    if ((stepper_x && stepper_x->isRunning()) || 
        (stepper_y_left && stepper_y_left->isRunning()) || 
        (stepper_y_right && stepper_y_right->isRunning()) || 
        (stepper_z && stepper_z->isRunning()) || 
        (stepper_rot && stepper_rot->isRunning())) {
        // Motors still moving, wait
        return;
    }
    
    // Check if we need to move to next side in sequence
    if (paintNextSide && isPaintSequence) {
        paintNextSide = false;
        int nextSide = -1;
        
        // Determine next side based on the predefined order: 0,2,3,1
        switch (currentPaintSide) {
            case 0: nextSide = 2; break; // Back -> Front
            case 2: nextSide = 3; break; // Front -> Left
            case 3: nextSide = 1; break; // Left -> Right
            case 1:                      // Right -> Done
                isPainting = false;
                isMoving = false;
                currentPaintSide = -1;
                currentPaintStep = 0;
                isPaintSequence = false;
                Serial.println("Paint All Sides sequence complete!");
                webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Paint All Sides sequence completed successfully.\"}");
                return;
            default:
                nextSide = 0; // Start with Back if current is invalid
        }
        
        if (nextSide >= 0) {
            // Start the next side
            currentPaintSide = nextSide;
            currentPaintStep = 0;
            char busyMsg[100];
            sprintf(busyMsg, "{\"status\":\"Busy\", \"message\":\"Moving to next side in sequence: Side %d\"}", nextSide);
            webSocket.broadcastTXT(busyMsg);
            Serial.println(busyMsg);
        }
    }
    
    // Process current step for the current side
    switch (currentPaintStep) {
        case 0: // Initial setup - set servo angle
            {
                int pitch = paintPitchAngle[currentPaintSide];
                Serial.printf("Setting servo pitch to %d for side %d\n", pitch, currentPaintSide);
                servo_pitch.write(pitch);
                delay(300); // Allow servo to settle
                currentPaintStep = 1;
            }
            break;
            
        case 1: // Start pattern execution
            {
                float speed = paintSpeed[currentPaintSide];
                float accel = patternXAccel;
                bool patternSuccessful = false;
                
                Serial.printf("Executing paint pattern for side %d\n", currentPaintSide);
                
                // Call the appropriate side-specific pattern function
                switch (currentPaintSide) {
                    case 0: // Back
                        patternSuccessful = !executePaintPatternBack(speed, accel);
                        break;
                    case 1: // Right
                        patternSuccessful = !executePaintPatternRight(speed, accel);
                        break;
                    case 2: // Front
                        patternSuccessful = !executePaintPatternFront(speed, accel);
                        break;
                    case 3: // Left
                        patternSuccessful = !executePaintPatternLeft(speed, accel);
                        break;
                }
                
                // Check if pattern was successful
                if (patternSuccessful) {
                    Serial.println("Paint pattern executed successfully");
                    currentPaintStep = 2;
                } else {
                    Serial.println("Paint pattern execution failed or stopped");
                    deactivatePaintGun(true);
                    isPainting = false;
                    isMoving = false;
                    currentPaintSide = -1;
                    currentPaintStep = 0;
                    isPaintSequence = false;
                    webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Pattern execution failed or stopped.\"}");
                }
            }
            break;
            
        case 2: // Post-pattern actions - ensure paint gun is off
            deactivatePaintGun(true);
            digitalWrite(PAINT_GUN_PIN, LOW);      // Double check directly with pins
            digitalWrite(PRESSURE_POT_PIN, LOW);   // for safety
            Serial.println("Deactivated paint gun after pattern completion");
            currentPaintStep = 3;
            break;
            
        case 3: // Move Z to safe height
            Serial.println("Moving Z axis to safe height (0)");
            moveZToPositionInches(0.0, patternZSpeed, patternZAccel);
            currentPaintStep = 4;
            break;
            
        case 4: // Move to home XY position
            Serial.println("Returning to home XY position (0,0)");
            moveToXYPositionInches(0.0, 0.0);
            currentPaintStep = 5;
            break;
            
        case 5: // Return rotation to 0
            if (stepper_rot) {
                Serial.println("Returning rotation to 0 position");
                rotateToAbsoluteDegree(0);
                currentPaintStep = 6;
            } else {
                // Skip rotation step if no rotation stepper
                currentPaintStep = 6;
            }
            break;
            
        case 6: // Complete painting of this side
            {
                char readyMsg[100];
                
                if (isPaintSequence) {
                    // If in a sequence, signal to move to the next side
                    Serial.printf("Side %d painting complete. Ready for next side.\n", currentPaintSide);
                    paintNextSide = true;
                } else {
                    // If single side, complete the painting operation
                    sprintf(readyMsg, "{\"status\":\"Ready\", \"message\":\"Painting Side %d complete.\"}", currentPaintSide);
                    webSocket.broadcastTXT(readyMsg);
                    Serial.println(readyMsg);
                    sendCurrentPositionUpdate();
                    
                    isPainting = false;
                    isMoving = false;
                    currentPaintSide = -1;
                    currentPaintStep = 0;
                }
            }
            break;
    }
}

// --- Legacy blocking paintSide function (kept for reference and fallback)
// This function is now replaced by the non-blocking startPaintingSide + processPaintingStateMachine
void paintSide(int sideIndex) {
    // Make sure we're not already in a painting state
    if (isPainting) {
        Serial.println("[WARN] Attempting to start paintSide while already painting - resetting state first");
        isPainting = false;
        isMoving = false;
        currentPaintSide = -1;
        currentPaintStep = 0;
        isPaintSequence = false;
        paintNextSide = false;
    }
    
    // Start the painting process using the new non-blocking approach
    startPaintingSide(sideIndex, false);
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
    // The pitch servo controls the paint gun direction/angle (0-180 degrees)
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

    // --- Paint Gun Control Initialization ---
    initializePaintGunControl();
    // --- End Paint Gun Control Initialization ---

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
            
            // Set default speed and acceleration for the rotation stepper
            stepper_rot->setSpeedInHz(patternRotSpeed);
            stepper_rot->setAcceleration(patternRotAccel);
            Serial.printf("DEBUG: Rotation stepper configured with speed=%.2f Hz, accel=%.2f steps/s^2\n", 
                        patternRotSpeed, patternRotAccel);
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
    // Handle Wi-Fi and OTA
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        webServer.handleClient();
        webSocket.loop();
    }
    
    // NEW: Process painting state machine for non-blocking operation
    processPaintingStateMachine();
    
    // Handle the PnP mode request button (if pressed)
    debouncer_pnp_cycle_button.update();
    if (debouncer_pnp_cycle_button.rose()) {
        Serial.println("PnP cycle button pressed!");
        // Handle PnP button press (future feature)
    }
    
    // Handle pending homing after PnP sequence completes
    if (pendingHomingAfterPnP && !isMoving && !isHoming) {
        Serial.println("Executing pending homing after PnP exit");
        pendingHomingAfterPnP = false; // Clear the flag first to prevent re-trigger
        homeAllAxes(); // Home all axes after PnP sequence completes
    }
    
    // IMPROVED FIX: Check for movement completion and reset flags
    // This ensures we don't get stuck in a "busy" state when movements complete
    // Also checks if we're stuck in a painting state without actual painting happening
    
    // Check if machine is stuck in painting mode
    static unsigned long lastPaintStepChangeTime = 0;
    static int lastPaintStep = -1;
    
    if (isPainting) {
        if (currentPaintStep != lastPaintStep) {
            lastPaintStep = currentPaintStep;
            lastPaintStepChangeTime = millis();
        } else if (millis() - lastPaintStepChangeTime > 10000) { // 10 seconds timeout
            // Machine is stuck in same painting step for too long
            Serial.println("WARNING: Machine appears stuck in painting mode - resetting state");
            deactivatePaintGun(true);
            isPainting = false;
            isMoving = false;
            currentPaintSide = -1;
            currentPaintStep = 0;
            isPaintSequence = false;
            paintNextSide = false;
            webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Painting operation reset due to inactivity.\"}");
            lastPaintStep = -1;
        }
    } else {
        lastPaintStep = -1;
    }
    
    // Check for normal movement completion
    if (isMoving) {
        // Check if all motors have stopped
        bool anyMotorRunning = false;
        if (stepper_x && stepper_x->isRunning()) anyMotorRunning = true;
        if (stepper_y_left && stepper_y_left->isRunning()) anyMotorRunning = true;
        if (stepper_y_right && stepper_y_right->isRunning()) anyMotorRunning = true;
        if (stepper_z && stepper_z->isRunning()) anyMotorRunning = true;
        if (stepper_rot && stepper_rot->isRunning()) anyMotorRunning = true;
        
        // If no motors are running but we still have the isMoving flag set, clear it
        if (!anyMotorRunning) {
            isMoving = false;
            Serial.println("Movement completed - resetting busy flag");
            sendCurrentPositionUpdate();
            
            // Only send the ready message if we're not in a special mode
            if (!isPainting && !isHoming && !inPickPlaceMode && !inCalibrationMode) {
                webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Movement completed.\"}");
            }
        }
    }
    
    // Small delay to prevent CPU from maxing out
    delay(1);
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
    // sendAllSettingsUpdate(255, message); // OLD: Call to old function
    sendCurrentSettings(255); // NEW: Call the renamed function (sends to all clients)
} 

// Function to save all configurable settings to NVS
void saveSettings() {
    Serial.println("[DEBUG] saveSettings() started.");
    
    // Log some key values before saving
    Serial.println("[DEBUG] Values being saved:");
    Serial.printf("  Grid: %d cols, %d rows\n", placeGridCols, placeGridRows);
    Serial.printf("  Tray: %.2f x %.2f inches\n", trayWidth_inch, trayHeight_inch);
    Serial.printf("  PnP Pick: (%.2f, %.2f, %.2f)\n", pnpPickLocationX_inch, pnpPickLocationY_inch, pnpPickLocationZ_inch);
    Serial.printf("  Place First: (%.2f, %.2f)\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
    
    // Log paint start positions
    Serial.println("  Paint Start X positions:");
    for (int i = 0; i < 4; i++) {
        Serial.printf("    Side %d: %.2f\n", i, paintStartX[i]);
    }
    Serial.println("  Paint Start Y positions:");
    for (int i = 0; i < 4; i++) {
        Serial.printf("    Side %d: %.2f\n", i, paintStartY[i]);
    }
    
    // Open NVS in R/W mode with error checking
    if (!preferences.begin("paint-machine", false)) {
        Serial.println("[ERROR] Failed to open NVS namespace for writing!");
        return;
    }
    
    // Save all settings with error checking
    
    // Save PnP/Grid Settings
    preferences.putInt("gridCols", placeGridCols);
    preferences.putInt("gridRows", placeGridRows);
    preferences.putFloat("trayWidth", trayWidth_inch);
    preferences.putFloat("trayHeight", trayHeight_inch);
    preferences.putFloat("gunOffsetX", paintGunOffsetX_inch);
    preferences.putFloat("gunOffsetY", paintGunOffsetY_inch);
    
    // Save PnP positions
    preferences.putFloat("pnpPickX", pnpPickLocationX_inch);
    preferences.putFloat("pnpPickY", pnpPickLocationY_inch);
    preferences.putFloat("pnpPickZ", pnpPickLocationZ_inch);
    preferences.putFloat("pnpPlaceZ", pnpPlaceHeight_inch);
    preferences.putFloat("firstPlaceX", placeFirstXAbsolute_inch);
    preferences.putFloat("firstPlaceY", placeFirstYAbsolute_inch);
    
    // Save Painting Side Settings
    size_t bytesWritten = preferences.putBytes("paintZ", paintZHeight_inch, sizeof(paintZHeight_inch));
    if (bytesWritten != sizeof(paintZHeight_inch)) {
        Serial.printf("[ERROR] Failed to save paintZ: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintZHeight_inch), bytesWritten);
    }
    
    bytesWritten = preferences.putBytes("paintP", paintPitchAngle, sizeof(paintPitchAngle));
    if (bytesWritten != sizeof(paintPitchAngle)) {
        Serial.printf("[ERROR] Failed to save paintP: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintPitchAngle), bytesWritten);
    }
    
    bytesWritten = preferences.putBytes("paintPat", paintPatternType, sizeof(paintPatternType));
    if (bytesWritten != sizeof(paintPatternType)) {
        Serial.printf("[ERROR] Failed to save paintPat: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintPatternType), bytesWritten);
    }
    
    bytesWritten = preferences.putBytes("paintS", paintSpeed, sizeof(paintSpeed));
    if (bytesWritten != sizeof(paintSpeed)) {
        Serial.printf("[ERROR] Failed to save paintS: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintSpeed), bytesWritten);
    }

    // NEW: Save Painting Start Positions with error checking
    bytesWritten = preferences.putBytes("paintStartX", paintStartX, sizeof(paintStartX));
    if (bytesWritten != sizeof(paintStartX)) {
        Serial.printf("[ERROR] Failed to save paintStartX: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintStartX), bytesWritten);
    }
    
    bytesWritten = preferences.putBytes("paintStartY", paintStartY, sizeof(paintStartY));
    if (bytesWritten != sizeof(paintStartY)) {
        Serial.printf("[ERROR] Failed to save paintStartY: expected %d bytes, wrote %d bytes\n", 
                     sizeof(paintStartY), bytesWritten);
    }

    // Verification - check a few key values
    int verifyGridCols = preferences.getInt("gridCols", -1);
    float verifyFirstPlaceX = preferences.getFloat("firstPlaceX", -999.0);
    
    if (verifyGridCols != placeGridCols) {
        Serial.printf("[ERROR] Verification failed for gridCols: expected %d, got %d\n", 
                      placeGridCols, verifyGridCols);
    }
    
    if (abs(verifyFirstPlaceX - placeFirstXAbsolute_inch) > 0.001) {
        Serial.printf("[ERROR] Verification failed for firstPlaceX: expected %.2f, got %.2f\n", 
                      placeFirstXAbsolute_inch, verifyFirstPlaceX);
    }
    
    // Close preferences
    preferences.end();
    Serial.println("[INFO] Settings saved to NVS and verified.");
    Serial.println("[DEBUG] saveSettings() finished.");
}

// Function to load all configurable settings from NVS
void loadSettings() {
    Serial.println("[DEBUG] loadSettings() started.");
    
    // Open NVS in read-only mode with error checking
    if (!preferences.begin("paint-machine", true)) {
        Serial.println("[ERROR] Failed to open NVS namespace for reading!");
        // Set default values since we can't read from NVS
        setDefaultSettings();
        return;
    }

    // Load PnP/Grid Settings (with defaults if not found)
    placeGridCols = preferences.getInt("gridCols", 4);
    placeGridRows = preferences.getInt("gridRows", 5);
    trayWidth_inch = preferences.getFloat("trayWidth", 24.0f);
    trayHeight_inch = preferences.getFloat("trayHeight", 18.0f); 
    paintGunOffsetX_inch = preferences.getFloat("gunOffsetX", 0.0f);
    paintGunOffsetY_inch = preferences.getFloat("gunOffsetY", 1.5f);
    
    // Load PnP positions (using defaults)
    pnpPickLocationX_inch = preferences.getFloat("pnpPickX", 2.0f);
    pnpPickLocationY_inch = preferences.getFloat("pnpPickY", 2.0f);
    pnpPickLocationZ_inch = preferences.getFloat("pnpPickZ", 2.0f);
    pnpPlaceHeight_inch = preferences.getFloat("pnpPlaceZ", 0.5f);
    placeFirstXAbsolute_inch = preferences.getFloat("firstPlaceX", 5.0f);
    placeFirstYAbsolute_inch = preferences.getFloat("firstPlaceY", 5.0f);

    // Load Painting Side Settings with error checking
    float defaultZ[4] = {1.0, 1.0, 1.0, 1.0};
    int defaultP[4] = {SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH, SERVO_INIT_POS_PITCH};
    int defaultPat[4] = {0, 90, 0, 90};
    float defaultS[4] = {10000.0, 10000.0, 10000.0, 10000.0};
    
    size_t bytesRead = preferences.getBytes("paintZ", paintZHeight_inch, sizeof(paintZHeight_inch));
    if (bytesRead != sizeof(paintZHeight_inch)) {
        Serial.printf("[WARN] Failed to load paintZ: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintZHeight_inch), bytesRead);
        memcpy(paintZHeight_inch, defaultZ, sizeof(paintZHeight_inch));
    }
    
    bytesRead = preferences.getBytes("paintP", paintPitchAngle, sizeof(paintPitchAngle));
    if (bytesRead != sizeof(paintPitchAngle)) {
        Serial.printf("[WARN] Failed to load paintP: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintPitchAngle), bytesRead);
        memcpy(paintPitchAngle, defaultP, sizeof(paintPitchAngle));
    }
    
    bytesRead = preferences.getBytes("paintPat", paintPatternType, sizeof(paintPatternType));
    if (bytesRead != sizeof(paintPatternType)) {
        Serial.printf("[WARN] Failed to load paintPat: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintPatternType), bytesRead);
        memcpy(paintPatternType, defaultPat, sizeof(paintPatternType));
    }
    
    bytesRead = preferences.getBytes("paintS", paintSpeed, sizeof(paintSpeed));
    if (bytesRead != sizeof(paintSpeed)) {
        Serial.printf("[WARN] Failed to load paintS: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintSpeed), bytesRead);
        memcpy(paintSpeed, defaultS, sizeof(paintSpeed));
    }

    // Load Painting Start Positions with error checking
    float defaultStartX[4] = { 11.5f, 29.5f, 11.5f, 8.0f };
    float defaultStartY[4] = { 20.5f, 20.0f, 0.5f,  6.5f };
    
    bytesRead = preferences.getBytes("paintStartX", paintStartX, sizeof(paintStartX));
    if (bytesRead != sizeof(paintStartX)) {
        Serial.printf("[WARN] Failed to load paintStartX: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintStartX), bytesRead);
        memcpy(paintStartX, defaultStartX, sizeof(paintStartX));
    }
    
    bytesRead = preferences.getBytes("paintStartY", paintStartY, sizeof(paintStartY));
    if (bytesRead != sizeof(paintStartY)) {
        Serial.printf("[WARN] Failed to load paintStartY: expected %d bytes, read %d bytes. Using defaults.\n", 
                     sizeof(paintStartY), bytesRead);
        memcpy(paintStartY, defaultStartY, sizeof(paintStartY));
    }
    
    preferences.end();

    // Log the loaded values
    Serial.println("[DEBUG] Values loaded from NVS:");
    Serial.printf("  Grid: %d cols, %d rows\n", placeGridCols, placeGridRows);
    Serial.printf("  Tray: %.2f x %.2f inches\n", trayWidth_inch, trayHeight_inch);
    Serial.printf("  PnP Pick: (%.2f, %.2f, %.2f)\n", pnpPickLocationX_inch, pnpPickLocationY_inch, pnpPickLocationZ_inch);
    Serial.printf("  Place First: (%.2f, %.2f)\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
    
    // Log paint start positions
    Serial.println("  Paint Start X positions:");
    for (int i = 0; i < 4; i++) {
        Serial.printf("    Side %d: %.2f\n", i, paintStartX[i]);
    }
    Serial.println("  Paint Start Y positions:");
    for (int i = 0; i < 4; i++) {
        Serial.printf("    Side %d: %.2f\n", i, paintStartY[i]);
    }

    // Recalculate gaps after loading grid/tray settings
    calculateAndSetGridSpacing(placeGridCols, placeGridRows);
    Serial.println("[INFO] Settings loaded from NVS and verified.");
    Serial.println("[DEBUG] loadSettings() finished.");
}

// Helper function to set default values for all settings
void setDefaultSettings() {
    Serial.println("[INFO] Setting default values for all settings");
    
    // PnP/Grid Settings
    placeGridCols = 4;
    placeGridRows = 5;
    trayWidth_inch = 24.0f;
    trayHeight_inch = 18.0f;
    paintGunOffsetX_inch = 0.0f;
    paintGunOffsetY_inch = 1.5f;
    
    // PnP positions
    pnpPickLocationX_inch = 2.0f;
    pnpPickLocationY_inch = 2.0f;
    pnpPickLocationZ_inch = 2.0f;
    pnpPlaceHeight_inch = 0.5f;
    placeFirstXAbsolute_inch = 5.0f;
    placeFirstYAbsolute_inch = 5.0f;
    
    // Paint settings
    for (int i = 0; i < 4; i++) {
        paintZHeight_inch[i] = 1.0f;
        paintPitchAngle[i] = SERVO_INIT_POS_PITCH;
        paintPatternType[i] = (i % 2 == 0) ? 0 : 90; // Alternating patterns
        paintSpeed[i] = 10000.0f;
    }
    
    // Paint start positions
    paintStartX[0] = 11.5f;  // Back
    paintStartX[1] = 29.5f;  // Right
    paintStartX[2] = 11.5f;  // Front
    paintStartX[3] = 8.0f;   // Left
    
    paintStartY[0] = 20.5f;  // Back
    paintStartY[1] = 20.0f;  // Right
    paintStartY[2] = 0.5f;   // Front
    paintStartY[3] = 6.5f;   // Left
    
    // Recalculate grid spacing
    calculateAndSetGridSpacing(placeGridCols, placeGridRows);
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

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_TEXT: {
            // --- Log Raw Payload ---
            Serial.printf("[%u] WebSocket RAW Received (%d bytes): %.*s\n", num, length, length, payload); // Log raw payload

            // Create a mutable copy for strtok
            char payload_copy[length + 1];
            memcpy(payload_copy, payload, length);
            payload_copy[length] = '\0';

            // --- Parse Command (Only the first token) ---
            char* cmd = strtok(payload_copy, " "); // Get the first word/command
            if (cmd == NULL) {
                Serial.printf("[%u] WebSocket: Ignoring empty message.\n", num);
                return; 
            }
            // Store the command safely before strtok potentially modifies the buffer further if args are parsed
            char commandStr[32]; // Adjust size if longer commands are expected
            strncpy(commandStr, cmd, sizeof(commandStr) - 1);
            commandStr[sizeof(commandStr) - 1] = '\0'; // Ensure null termination
            
            Serial.printf("[%u] WebSocket Parsed Command: '%s'\n", num, commandStr);

            // --- Log Current State BEFORE Handling Command ---
            Serial.printf("    State Before Check: allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", 
                          allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);

            // --- Handle Command (Revised Structure) ---
            bool commandHandled = false; 

            // --- Simple Commands (No Args or State Checks needed beyond basic parse) ---
            if (strcmp(commandStr, "GET_STATUS") == 0) {
                commandHandled = true;
                Serial.printf("[%u] Handling GET_STATUS\n", num);
                sendAllSettingsUpdate(num, "Current status requested");
                if (allHomed) { sendCurrentPositionUpdate(); }
            } 
            else if (strcmp(commandStr, "EXIT_PICKPLACE") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling EXIT_PICKPLACE\n", num);
                 Serial.println("    EXIT_PICKPLACE Accepted: Exiting PnP mode.");
                 exitPickPlaceMode(true); 
             }
             else if (strcmp(commandStr, "EXIT_CALIBRATION") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling EXIT_CALIBRATION\n", num);
                 Serial.println("    EXIT_CALIBRATION Accepted: Exiting calibration mode.");
                 inCalibrationMode = false;
                 webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Exited calibration mode.\"}");
             }
             else if (strcmp(commandStr, "STOP") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling STOP\n", num);
                 Serial.println("    STOP Accepted: Initiating stop sequence.");
                 stopRequested = true; 
                 if(stepper_x) stepper_x->forceStop();
                 if(stepper_y_left) stepper_y_left->forceStop();
                 if(stepper_y_right) stepper_y_right->forceStop();
                 if(stepper_z) stepper_z->forceStop();
                 if(stepper_rot) stepper_rot->forceStop(); 
                 Serial.println("    STOP: Motors force stopped.");
                 isMoving = false; isHoming = false; inPickPlaceMode = false; inCalibrationMode = false; 
                 webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"STOP initiated. Homing axes...\"}"); 
                 homeAllAxes(); 
             }
             // --- PnP Step Commands (Require PnP Mode) ---
             else if (strcmp(commandStr, "PNP_NEXT_STEP") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling PNP_NEXT_STEP\n", num);
                 Serial.printf("    State Check (PNP_NEXT_STEP): inPnP=%d, isMoving=%d, isHoming=%d\n", inPickPlaceMode, isMoving, isHoming);
                 if (!inPickPlaceMode) { Serial.println("    PNP_NEXT_STEP Denied: Not in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    PNP_NEXT_STEP Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Machine is busy, cannot perform next step.\"}"); } 
                 else { Serial.println("    PNP_NEXT_STEP Accepted: Executing next step."); executeNextPickPlaceStep(); }
             } 
             else if (strcmp(commandStr, "PNP_SKIP_LOCATION") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling PNP_SKIP_LOCATION\n", num);
                 Serial.printf("    State Check (PNP_SKIP_LOCATION): inPnP=%d, isMoving=%d, isHoming=%d\n", inPickPlaceMode, isMoving, isHoming);
                 if (!inPickPlaceMode) { Serial.println("    PNP_SKIP_LOCATION Denied: Not in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    PNP_SKIP_LOCATION Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Machine is busy, cannot skip location.\"}"); } 
                 else { Serial.println("    PNP_SKIP_LOCATION Accepted: Skipping location."); skipPickPlaceLocation(); }
             } 
             else if (strcmp(commandStr, "PNP_BACK_LOCATION") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling PNP_BACK_LOCATION\n", num);
                 Serial.printf("    State Check (PNP_BACK_LOCATION): inPnP=%d, isMoving=%d, isHoming=%d\n", inPickPlaceMode, isMoving, isHoming);
                 if (!inPickPlaceMode) { Serial.println("    PNP_BACK_LOCATION Denied: Not in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    PNP_BACK_LOCATION Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Machine is busy, cannot go back.\"}"); } 
                 else { Serial.println("    PNP_BACK_LOCATION Accepted: Going back one location."); goBackPickPlaceLocation(); }
             }
             // --- Calibration Mode Commands (Require Calibration Mode) ---
              else if (strcmp(commandStr, "JOG") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling JOG\n", num);
                 Serial.printf("    State Check (JOG): inCalib=%d, isMoving=%d, isHoming=%d\n", inCalibrationMode, isMoving, isHoming);
                 if (!inCalibrationMode) { Serial.println("    JOG Denied: Not in calibration mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    JOG Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}"); } 
                 else {
                     char* axis_str = strtok(NULL, " "); char* dist_str = strtok(NULL, " ");
                     if (axis_str && dist_str && strlen(axis_str) == 1) {
                         char axis = axis_str[0]; float distance_inch = atof(dist_str);
                         Serial.printf("    JOG Accepted: Axis %c, Dist %.3f\n", axis, distance_inch);
                         // Find stepper based on axis...
                         FastAccelStepper* stepper_to_move = NULL; long current_steps = 0; long jog_steps = 0; float speed = 0, accel = 0;
                         if (axis == 'X' && stepper_x) { stepper_to_move = stepper_x; current_steps = stepper_x->getCurrentPosition(); jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY); speed = patternXSpeed; accel = patternXAccel; } 
                         else if (axis == 'Y' && stepper_y_left && stepper_y_right) { stepper_to_move = stepper_y_left; current_steps = stepper_y_left->getCurrentPosition(); jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY); speed = patternYSpeed; accel = patternYAccel; } 
                         else if (axis == 'Z' && stepper_z) { stepper_to_move = stepper_z; current_steps = stepper_z->getCurrentPosition(); jog_steps = (long)(distance_inch * STEPS_PER_INCH_Z); speed = patternZSpeed; accel = patternZAccel; } 
                         else { Serial.printf("    JOG Denied: Invalid axis '%c' or stepper not available.\n", axis); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}"); stepper_to_move = NULL; }

                         if (stepper_to_move) {
                             isMoving = true; webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                             long target_steps = current_steps + jog_steps;
                             stepper_to_move->setSpeedInHz(speed); stepper_to_move->setAcceleration(accel);
                             if (axis == 'Z') { float target_pos_inch = constrain((float)target_steps / STEPS_PER_INCH_Z, Z_MAX_TRAVEL_NEG_INCH, Z_MAX_TRAVEL_POS_INCH); target_steps = (long)(target_pos_inch * STEPS_PER_INCH_Z); Serial.printf("    Jogging Z (constrained) to %.3f inches (%ld steps)\n", target_pos_inch, target_steps); } 
                             else { Serial.printf("    Jogging %c to %ld steps\n", axis, target_steps); }
                             stepper_to_move->moveTo(target_steps);
                             if (axis == 'Y' && stepper_y_right) { stepper_y_right->setSpeedInHz(speed); stepper_y_right->setAcceleration(accel); stepper_y_right->moveTo(target_steps); }
                         }
                     } else { Serial.println("    JOG Denied: Invalid format."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y/Z distance\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "MOVE_TO_COORDS") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling MOVE_TO_COORDS\n", num);
                 Serial.printf("    State Check (MOVE_TO_COORDS): inCalib=%d, isMoving=%d, isHoming=%d\n", inCalibrationMode, isMoving, isHoming);
                 if (!inCalibrationMode) { Serial.println("    MOVE_TO_COORDS Denied: Not in calibration mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to move to coords.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    MOVE_TO_COORDS Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot move while machine is moving.\"}"); } 
                 else {
                     char* x_str = strtok(NULL, " "); char* y_str = strtok(NULL, " ");
                     if (x_str && y_str) {
                         float xVal = atof(x_str); float yVal = atof(y_str);
                         if (xVal >= 0 && yVal >= 0) { Serial.printf("    MOVE_TO_COORDS Accepted: X=%.2f, Y=%.2f\n", xVal, yVal); isMoving = true; webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Moving to position...\"}"); moveToXYPositionInches(xVal, yVal); } 
                         else { Serial.println("    MOVE_TO_COORDS Denied: Coordinates must be non-negative."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Coordinates must be non-negative.\"}"); }
                     } else { Serial.println("    MOVE_TO_COORDS Denied: Invalid format."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid MOVE_TO_COORDS format. Use: MOVE_TO_COORDS X Y\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_OFFSET_FROM_CURRENT") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_OFFSET_FROM_CURRENT\n", num);
                 Serial.printf("    State Check (SET_OFFSET_FROM_CURRENT): inCalib=%d, isMoving=%d, isHoming=%d\n", inCalibrationMode, isMoving, isHoming);
                 if (!inCalibrationMode) { Serial.println("    SET_OFFSET_FROM_CURRENT Denied: Not in calibration mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    SET_OFFSET_FROM_CURRENT Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot set while moving.\"}"); } 
                 else {
                     if (stepper_x && stepper_y_left) { pnpOffsetX_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY; pnpOffsetY_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; saveSettings(); Serial.printf("    SET_OFFSET_FROM_CURRENT Accepted: Set to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch); sendAllSettingsUpdate(num, "PnP Offset set from current position."); } 
                     else { Serial.println("    SET_OFFSET_FROM_CURRENT Denied: Steppers not available."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Steppers not available.\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_FIRST_PLACE_ABS_FROM_CURRENT") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_FIRST_PLACE_ABS_FROM_CURRENT\n", num);
                 Serial.printf("    State Check (SET_FIRST_PLACE_ABS_FROM_CURRENT): inCalib=%d, isMoving=%d, isHoming=%d\n", inCalibrationMode, isMoving, isHoming);
                 if (!inCalibrationMode) { Serial.println("    SET_FIRST_PLACE_ABS_FROM_CURRENT Denied: Not in calibration mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in Calibration Mode to set First Place position from current.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    SET_FIRST_PLACE_ABS_FROM_CURRENT Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot set while moving.\"}"); } 
                 else {
                     if (stepper_x && stepper_y_left) { placeFirstXAbsolute_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY; placeFirstYAbsolute_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; saveSettings(); Serial.printf("    SET_FIRST_PLACE_ABS_FROM_CURRENT Accepted: Set to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch); sendAllSettingsUpdate(num, "First Place Absolute Pos set from current position."); } 
                     else { Serial.println("    SET_FIRST_PLACE_ABS_FROM_CURRENT Denied: Steppers not available."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Internal stepper error.\"}"); }
                 }
             }
             // --- General Idle Commands (Require Homing, but not specific mode) ---
             else if (strcmp(commandStr, "HOME") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling HOME\n", num);
                 Serial.printf("    State Check (HOME): isMoving=%d, isHoming=%d\n", isMoving, isHoming);
                 if (isMoving || isHoming) { Serial.println("    HOME Denied: Busy"); webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot home, machine is busy.\"}"); } 
                 else { Serial.println("    HOME Accepted: Starting homing sequence."); homeAllAxes(); }
             } 
             else if (strcmp(commandStr, "GOTO_5_5_0") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling GOTO_5_5_0\n", num);
                 Serial.printf("    State Check (GOTO): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.println("    GOTO_5_5_0 Denied: Invalid state."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot GOTO: Machine busy or not ready.\"}"); } 
                 else { Serial.println("    GOTO_5_5_0 Accepted: Moving."); moveToPositionInches(5.0, 5.0, 0.0); }
             } 
             else if (strcmp(commandStr, "GOTO_20_20_0") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling GOTO_20_20_0\n", num);
                 Serial.printf("    State Check (GOTO): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.println("    GOTO_20_20_0 Denied: Invalid state."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot GOTO: Machine busy or not ready.\"}"); } 
                 else { Serial.println("    GOTO_20_20_0 Accepted: Moving."); moveToPositionInches(20.0, 20.0, 0.0); }
             } 
             else if (strcmp(commandStr, "ENTER_PICKPLACE") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling ENTER_PICKPLACE\n", num);
                 Serial.printf("    State Check (ENTER_PICKPLACE): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed) { Serial.println("    ENTER_PICKPLACE Denied: Not homed."); webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Machine not homed.\"}"); } 
                 else if (isMoving || isHoming) { Serial.println("    ENTER_PICKPLACE Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Machine is busy.\"}"); } 
                 else if (inPickPlaceMode) { Serial.println("    ENTER_PICKPLACE Denied: Already in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"PickPlaceReady\", \"message\":\"Already in Pick/Place mode. Use Exit button.\"}"); } 
                 else if (inCalibrationMode) { Serial.println("    ENTER_PICKPLACE Denied: In Calibration mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Exit calibration before entering PnP mode.\"}"); } 
                 else { Serial.println("    ENTER_PICKPLACE Accepted: Entering PnP mode."); enterPickPlaceMode(); }
             } 
              else if (strcmp(commandStr, "ENTER_CALIBRATION") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling ENTER_CALIBRATION\n", num);
                 Serial.printf("    State Check (ENTER_CALIBRATION): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed) { Serial.println("    ENTER_CALIBRATION Denied: Not homed."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine must be homed before entering calibration.\"}"); } 
                 else if (isMoving || isHoming || inPickPlaceMode) { Serial.println("    ENTER_CALIBRATION Denied: Machine busy or in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine busy or in PnP mode. Cannot enter calibration.\"}"); } 
                 else { Serial.println("    ENTER_CALIBRATION Accepted: Entering calibration mode."); inCalibrationMode = true; webSocket.broadcastTXT("{\"status\":\"CalibrationActive\", \"message\":\"Calibration mode entered.\"}"); sendCurrentPositionUpdate(); }
             }
             else if (strcmp(commandStr, "ROTATE") == 0) { // Also allow general rotation when idle
                 commandHandled = true;
                 Serial.printf("[%u] Handling ROTATE (General Idle)\n", num);
                 Serial.printf("    State Check (ROTATE): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.println("    ROTATE Denied: Invalid state."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot rotate: Machine busy or not ready.\"}"); }
                 else if (!stepper_rot) { Serial.println("    ROTATE Denied: Rotation Stepper Not Available"); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation control unavailable (pin conflict?)\"}"); } 
                 else {
                     char* degrees_str = strtok(NULL, " "); 
                     if (degrees_str) { float degrees = atof(degrees_str); Serial.printf("    ROTATE Accepted: Rotating by %.2f degrees\n", degrees); float currentAngle = (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE; int targetAngle = (int)round(currentAngle + degrees); rotateToAbsoluteDegree(targetAngle); } 
                     else { Serial.println("    ROTATE Denied: Missing degrees value"); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Missing degrees for ROTATE\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_ROT_ZERO") == 0) { // Allow general set zero when idle
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_ROT_ZERO (General Idle)\n", num);
                 Serial.printf("    State Check (SET_ROT_ZERO): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.println("    SET_ROT_ZERO Denied: Invalid state."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set zero while machine is busy or in special mode.\"}"); } 
                 else if (!stepper_rot) { Serial.println("    SET_ROT_ZERO Denied: Rotation stepper not enabled."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation stepper not enabled.\"}"); } 
                 else { Serial.println("    SET_ROT_ZERO Accepted: Setting current position to zero."); stepper_rot->setCurrentPosition(0); webSocket.sendTXT(num, "{\"status\":\"Ready\", \"message\":\"Current rotation set to zero.\"}"); sendCurrentPositionUpdate(); }
             }
             else if (strcmp(commandStr, "PAINT_SIDE_0") == 0 || strcmp(commandStr, "PAINT_SIDE_1") == 0 || strcmp(commandStr, "PAINT_SIDE_2") == 0 || strcmp(commandStr, "PAINT_SIDE_3") == 0) {
                 commandHandled = true;
                 int sideIndex = commandStr[strlen(commandStr)-1] - '0'; // Extract side index from command name
                 Serial.printf("[%u] Handling PAINT_SIDE_%d\n", num, sideIndex);
                 Serial.printf("    State Check (PAINT_SIDE): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.printf("    PAINT_SIDE_%d Denied: Invalid state.\n", sideIndex); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot start painting, invalid machine state.\"}"); } 
                 else { Serial.printf("    PAINT_SIDE_%d Accepted: Starting paint sequence.\n", sideIndex); paintSide(sideIndex); }
             } 
             else if (strcmp(commandStr, "PAINT_ALL") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling PAINT_ALL\n", num);
                 Serial.printf("    State Check (PAINT_ALL): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { 
                     Serial.println("    PAINT_ALL Denied: Invalid state."); 
                     webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot start Paint All, machine is busy or not ready.\"}"); 
                 } else {
                     Serial.println("    PAINT_ALL Accepted: Starting sequence."); 
                     webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Starting Paint All sequence...\"}");
                     
                     // Start the painting sequence using the non-blocking approach
                     // The sequence will paint sides in this order: 0 (Back), 2 (Front), 3 (Left), 1 (Right)
                     startPaintingSide(0, true);
                 }
             } 
             else if (strcmp(commandStr, "CLEAN_GUN") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling CLEAN_GUN\n", num);
                 Serial.printf("    State Check (CLEAN_GUN): allHomed=%d, isMoving=%d, isHoming=%d, inPnP=%d, inCalib=%d\n", allHomed, isMoving, isHoming, inPickPlaceMode, inCalibrationMode);
                 if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) { Serial.println("    CLEAN_GUN Denied: Invalid state."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot start Clean Gun, machine is busy or not ready.\"}"); } 
                 else {
                     Serial.println("    CLEAN_GUN Accepted: Starting sequence."); webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Starting Clean Gun sequence...\"}");
                     isMoving = true; stopRequested = false; 
                     
                     // Set servo to initial position (controls paint gun direction)
                     Serial.println("Setting pitch servo to initial position for cleaning");
                     servo_pitch.write(SERVO_INIT_POS_PITCH);
                     delay(300); // Allow servo to settle
                     
                     // Sequence...
                     rotateToAbsoluteDegree(0); if (stopRequested) { Serial.println("Clean Gun stopped during Rotation"); deactivatePaintGun(true); isMoving = false; return; } 
                     moveToXYPositionInches_Paint(3.0, 10.0, patternXSpeed / 2, patternXAccel / 2); if (stopRequested) { Serial.println("Clean Gun stopped during XY Move"); deactivatePaintGun(true); isMoving = false; return; } 
                     activatePaintGun(); unsigned long sprayStartTime = millis();
                     while (millis() - sprayStartTime < 3000) { webSocket.loop(); if (stopRequested) { Serial.println("Clean Gun stopped during Spray"); break; } yield(); }
                     // Always deactivate the paint gun, regardless of stop status
                     deactivatePaintGun(true); 
                     if (stopRequested) { isMoving = false; return; } 
                     if (!stopRequested) { moveToXYPositionInches(0.0, 0.0); while ((stepper_x && stepper_x->isRunning()) || (stepper_y_left && stepper_y_left->isRunning()) || (stepper_y_right && stepper_y_right->isRunning())) { webSocket.loop(); if (stopRequested) { Serial.println("Clean Gun stopped during Return Home"); break; } yield(); } }
                     
                     // Ensure servo is back to initial position after cleaning
                     Serial.println("Returning pitch servo to initial position after cleaning");
                     servo_pitch.write(SERVO_INIT_POS_PITCH);
                     delay(300); // Allow servo to settle
                     
                     isMoving = false; 
                     if (stopRequested) { Serial.println("Clean Gun stopped by user."); } 
                     else { Serial.println("Clean Gun sequence completed."); webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Clean Gun sequence completed.\"}"); }
                     sendCurrentPositionUpdate();
                 }
             }
             // --- Settings Commands (Can usually run when idle, some need args) ---
             else if (strcmp(commandStr, "SET_SERVO_PITCH") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_SERVO_PITCH\n", num);
                 char* angle_str = strtok(NULL, " "); 
                 if (angle_str) {
                     int angle = atoi(angle_str);
                     if (angle >= 0 && angle <= 180) { setPitchServoAngle(angle); char msgBuffer[100]; sprintf(msgBuffer, "{\"status\":\"Info\", \"message\":\"Pitch servo angle set to %d\"}", angle); webSocket.sendTXT(num, msgBuffer); } 
                     else { webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid angle (0-180)\"}"); }
                 } else { webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Missing angle for SET_SERVO_PITCH\"}"); }
             } 
             else if (strcmp(commandStr, "SET_PNP_OFFSET") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_PNP_OFFSET\n", num);
                 Serial.printf("    State Check (SET_PNP_OFFSET): isMoving=%d, isHoming=%d, inPnP=%d\n", isMoving, isHoming, inPickPlaceMode);
                 if (isMoving || isHoming || inPickPlaceMode) { Serial.println("    SET_PNP_OFFSET Denied: Machine busy or in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set offset while machine is busy or in PnP mode.\"}"); } 
                 else {
                     char* x_str = strtok(NULL, " "); char* y_str = strtok(NULL, " ");
                     if (x_str && y_str) { pnpOffsetX_inch = atof(x_str); pnpOffsetY_inch = atof(y_str); saveSettings(); Serial.printf("    SET_PNP_OFFSET Accepted: Set to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch); sendAllSettingsUpdate(num, "PnP offset updated."); } 
                     else { Serial.println("    SET_PNP_OFFSET Denied: Missing values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_PNP_OFFSET. Use: SET_PNP_OFFSET X Y\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_FIRST_PLACE_ABS") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_FIRST_PLACE_ABS\n", num);
                 Serial.printf("    State Check (SET_FIRST_PLACE_ABS): isMoving=%d, isHoming=%d, inPnP=%d\n", isMoving, isHoming, inPickPlaceMode);
                 if (isMoving || isHoming || inPickPlaceMode) { Serial.println("    SET_FIRST_PLACE_ABS Denied: Machine busy or in PnP mode."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set first place while machine is busy or in PnP mode.\"}"); } 
                 else {
                     char* x_str = strtok(NULL, " "); char* y_str = strtok(NULL, " ");
                     if (x_str && y_str) { placeFirstXAbsolute_inch = atof(x_str); placeFirstYAbsolute_inch = atof(y_str); saveSettings(); Serial.printf("    SET_FIRST_PLACE_ABS Accepted: Set to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch); sendAllSettingsUpdate(num, "First Place Absolute Pos updated."); } 
                     else { Serial.println("    SET_FIRST_PLACE_ABS Denied: Missing values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_FIRST_PLACE_ABS format.\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_GRID_SPACING") == 0) { 
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_GRID_SPACING\n", num);
                 Serial.printf("    State Check (SET_GRID_SPACING): isMoving=%d, isHoming=%d\n", isMoving, isHoming);
                 if (isMoving || isHoming) { Serial.println("    SET_GRID_SPACING Denied: Machine busy."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set grid while machine is busy.\"}"); } 
                 else {
                     char* cols_str = strtok(NULL, " "); char* rows_str = strtok(NULL, " ");
                     if (cols_str && rows_str) {
                         int cols = atoi(cols_str); int rows = atoi(rows_str);
                         if (cols > 0 && rows > 0) { Serial.printf("    SET_GRID_SPACING Accepted: %d x %d\n", cols, rows); calculateAndSetGridSpacing(cols, rows); } 
                         else { Serial.println("    SET_GRID_SPACING Denied: Invalid cols/rows value."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid grid columns/rows. Must be positive integers.\"}"); }
                     } else { Serial.println("    SET_GRID_SPACING Denied: Missing values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_GRID_SPACING. Use: SET_GRID_SPACING cols rows\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_TRAY_SIZE") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_TRAY_SIZE\n", num);
                 Serial.printf("    State Check (SET_TRAY_SIZE): isMoving=%d, isHoming=%d\n", isMoving, isHoming);
                 if (isMoving || isHoming) { Serial.println("    SET_TRAY_SIZE Denied: Machine busy."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set tray size while machine is busy.\"}"); } 
                 else {
                     char* width_str = strtok(NULL, " "); char* height_str = strtok(NULL, " ");
                     if (width_str && height_str) {
                         float width = atof(width_str); float height = atof(height_str);
                         if (width > 0 && height > 0) { Serial.printf("    SET_TRAY_SIZE Accepted: W=%.2f, H=%.2f\n", width, height); trayWidth_inch = width; trayHeight_inch = height; saveSettings(); calculateAndSetGridSpacing(placeGridCols, placeGridRows); } 
                         else { Serial.println("    SET_TRAY_SIZE Denied: Invalid width/height value."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid tray dimensions. Width and Height must be positive numbers.\"}"); }
                     } else { Serial.println("    SET_TRAY_SIZE Denied: Missing values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_TRAY_SIZE. Use: SET_TRAY_SIZE width height\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_PNP_SPEEDS") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_PNP_SPEEDS\n", num);
                 Serial.printf("    State Check (SET_PNP_SPEEDS): isMoving=%d, isHoming=%d\n", isMoving, isHoming);
                 if (isMoving || isHoming) { Serial.println("    SET_PNP_SPEEDS Denied: Machine busy."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set speeds while machine is busy.\"}"); } 
                 else {
                     char* xs_str = strtok(NULL, " "); char* ys_str = strtok(NULL, " ");
                     if (xs_str && ys_str) {
                         float receivedXS = atof(xs_str); float receivedYS = atof(ys_str);
                         if (receivedXS > 0 && receivedYS > 0) { Serial.printf("    SET_PNP_SPEEDS Accepted: XS=%.0f, YS=%.0f\n", receivedXS, receivedYS); patternXSpeed = receivedXS; patternYSpeed = receivedYS; saveSettings(); sendAllSettingsUpdate(num, "Speeds updated."); } 
                         else { Serial.println("    SET_PNP_SPEEDS Denied: Invalid speed values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid speed values. Must be positive numbers.\"}"); }
                     } else { Serial.println("    SET_PNP_SPEEDS Denied: Missing values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_PNP_SPEEDS. Use: SET_PNP_SPEEDS XS YS\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_PAINT_GUN_OFFSET") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_PAINT_GUN_OFFSET\n", num);
                 Serial.printf("    State Check (SET_PAINT_GUN_OFFSET): isMoving=%d, isHoming=%d\n", isMoving, isHoming); 
                 if (isMoving || isHoming) { Serial.println("    SET_PAINT_GUN_OFFSET Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set paint offset while busy.\"}"); } 
                 else {
                     char* gunX_str = strtok(NULL, " "); char* gunY_str = strtok(NULL, " ");
                     if (gunX_str && gunY_str) { paintGunOffsetX_inch = atof(gunX_str); paintGunOffsetY_inch = atof(gunY_str); saveSettings(); Serial.printf("    SET_PAINT_GUN_OFFSET Accepted: Set to X:%.2f, Y:%.2f\n", paintGunOffsetX_inch, paintGunOffsetY_inch); sendAllSettingsUpdate(num, "Paint gun offset updated."); } 
                     else { Serial.println("    SET_PAINT_GUN_OFFSET Denied: Invalid format."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid paint gun offset format.\"}"); }
                 }
             } 
             else if (strcmp(commandStr, "SET_PAINT_SIDE_SETTINGS") == 0) {
                 commandHandled = true;
                 Serial.printf("[%u] Handling SET_PAINT_SIDE_SETTINGS\n", num);
                 Serial.printf("    State Check (SET_PAINT_SIDE_SETTINGS): isMoving=%d, isHoming=%d\n", isMoving, isHoming); 
                 if (isMoving || isHoming) { Serial.println("    SET_PAINT_SIDE_SETTINGS Denied: Busy."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set paint side settings while busy.\"}"); } 
                 else {
                     char* sideIdx_str = strtok(NULL, " "); char* zVal_str = strtok(NULL, " "); char* pitchVal_str = strtok(NULL, " "); char* patternVal_str = strtok(NULL, " "); char* speedVal_str = strtok(NULL, " ");
                     if (sideIdx_str && zVal_str && pitchVal_str && patternVal_str && speedVal_str) {
                         int sideIdx = atoi(sideIdx_str); float zVal = atof(zVal_str); int pitchVal = atoi(pitchVal_str); int patternVal = atoi(patternVal_str); float speedVal = atof(speedVal_str);
                         if (sideIdx >= 0 && sideIdx < 4 && pitchVal >= 0 && pitchVal <= 180 && (patternVal == 0 || patternVal == 90)) { Serial.printf("    SET_PAINT_SIDE_SETTINGS Accepted: Side %d, Z=%.2f, P=%d, Pat=%d, S=%.0f\n", sideIdx, zVal, pitchVal, patternVal, speedVal); paintZHeight_inch[sideIdx] = zVal; paintPitchAngle[sideIdx] = pitchVal; paintPatternType[sideIdx] = patternVal; paintSpeed[sideIdx] = speedVal; saveSettings(); sendAllSettingsUpdate(num, "Paint side settings updated."); } 
                         else { Serial.println("    SET_PAINT_SIDE_SETTINGS Denied: Invalid parameter values."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid parameter values (Side 0-3, Pitch 0-180, Pattern 0/90).\"}"); }
                     } else { Serial.println("    SET_PAINT_SIDE_SETTINGS Denied: Invalid format."); webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid paint side settings format.\"}"); }
                 }
             } 
             // --- JSON Command Handling ---
             else if (payload[0] == '{') {
                 // This is a JSON-formatted command
                 JsonDocument doc;
                 DeserializationError error = deserializeJson(doc, payload, length);
                 
                 if (error) {
                     Serial.printf("[%u] Failed to parse JSON command: %s\n", num, error.c_str());
                     webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JSON format\"}");
                 } else {
                     // Successfully parsed JSON - handle commands
                     const char* cmd = doc["command"];
                     if (cmd && strcmp(cmd, "SET_PAINT_STARTS") == 0) {
                         commandHandled = true;
                         Serial.printf("[%u] Handling SET_PAINT_STARTS (JSON)\n", num);
                         Serial.printf("    State Check (SET_PAINT_STARTS): isMoving=%d, isHoming=%d\n", isMoving, isHoming);
                         
                         if (isMoving || isHoming) {
                             Serial.println("    SET_PAINT_STARTS Denied: Busy.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set paint start positions while busy.\"}");
                         } else {
                             // Extract values from the JSON data
                             JsonObject data = doc["data"];
                             if (data) {
                                 bool allValid = true;
                                 
                                 // Process each side's start position
                                 for (int i = 0; i < 4; i++) {
                                     char xKey[3], yKey[3];
                                     sprintf(xKey, "X%d", i);
                                     sprintf(yKey, "Y%d", i);
                                     
                                     if (data.containsKey(xKey) && data.containsKey(yKey)) {
                                         paintStartX[i] = data[xKey];
                                         paintStartY[i] = data[yKey];
                                         Serial.printf("    SET_PAINT_STARTS: Side %d set to X=%.2f, Y=%.2f\n", 
                                                    i, paintStartX[i], paintStartY[i]);
                                     } else {
                                         allValid = false;
                                         Serial.printf("    SET_PAINT_STARTS: Missing data for side %d\n", i);
                                     }
                                 }
                                 
                                 if (allValid) {
                                     saveSettings();
                                     Serial.println("    SET_PAINT_STARTS Accepted: All paint start positions updated.");
                                     sendCurrentSettings(num);
                                 } else {
                                     Serial.println("    SET_PAINT_STARTS Partially Processed: Some positions may be missing.");
                                     webSocket.sendTXT(num, "{\"status\":\"Warning\", \"message\":\"Some paint start positions were missing in the data\"}");
                                     saveSettings(); // Save what we could process
                                     sendCurrentSettings(num);
                                 }
                             } else {
                                 Serial.println("    SET_PAINT_STARTS Denied: Missing data object.");
                                 webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_PAINT_STARTS format: missing data\"}");
                             }
                         }
                     } else {
                         Serial.printf("[%u] Unknown JSON command: %s\n", num, cmd ? cmd : "null");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Unknown JSON command\"}");
                     }
                 }
             }

            // --- Final Check for Unhandled Commands ---
            if (!commandHandled) {
                // Use commandStr here as cmd might be NULL or pointing elsewhere if args were parsed
                Serial.printf("[%u] Unknown command received (Fell through all checks): '%s'\n", num, commandStr); 
                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Unknown command\"}");
            }
        }
        break;
        // ... other WStype cases ...
        case WStype_BIN:
             Serial.printf("[%u] WebSocket Received Binary Data (%d bytes)\n", num, length);
            break;
        case WStype_DISCONNECTED:
             Serial.printf("[%u] WebSocket Client Disconnected!\n", num);
             break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] WebSocket Client Connected from %s url: %s\n", num, ip.toString().c_str(), payload);
             String welcomeMsg = "Welcome! Connected to Paint + PnP Machine.";
             sendAllSettingsUpdate(num, welcomeMsg);
             if (allHomed) {
                  sendCurrentPositionUpdate();
             }
            }
             break;
        case WStype_ERROR:
             Serial.printf("[%u] WebSocket Error!\n", num);
             break;
        default:
             Serial.printf("[%u] WebSocket Unhandled Event Type: %d\n", num, type);
             break;

    }
}

// Function to stop all movement
void stopAllMovement() {
    isMoving = false;
    isHoming = false;
    
    // Ensure paint gun is deactivated when stopping
    deactivatePaintGun(true);
    
    // Double-check directly with pins for safety
    digitalWrite(PAINT_GUN_PIN, LOW);
    digitalWrite(PRESSURE_POT_PIN, LOW);
    
    // Stop all motors
    stepper_x->forceStop();
    stepper_y_left->forceStop();
    stepper_y_right->forceStop();
    stepper_z->forceStop();
    if (stepper_rot) {
        stepper_rot->forceStop();
    }
    
    // Send status message
    webSocket.broadcastTXT("{\"status\":\"Stopped\", \"message\":\"All movement stopped. Paint gun deactivated.\"}");
    // Serial.println("All movement stopped.");
}

// Helper function to send ALL current settings
// Includes machine state, PnP/Grid settings, and all Painting settings.
void sendCurrentSettings(uint8_t specificClientNum) {
    JsonDocument doc; // Use ArduinoJson library V7+ syntax

    // Machine State
    JsonObject statusObj = doc.createNestedObject("status");
    statusObj["isMoving"] = isMoving;
    statusObj["isHoming"] = isHoming;
    statusObj["allHomed"] = allHomed;
    statusObj["inCalibrationMode"] = inCalibrationMode;
    statusObj["inPickPlaceMode"] = inPickPlaceMode;
    // Add other states as needed, e.g., isPainting

    // Current Position (Tool Center Point - TCP)
    JsonObject positionObj = doc.createNestedObject("position");
    if (allHomed) {
        positionObj["x"] = stepper_x ? (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY : 0.0f;
        positionObj["y"] = stepper_y_left ? (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY : 0.0f; // Use left Y
        positionObj["z"] = stepper_z ? (float)stepper_z->getCurrentPosition() / STEPS_PER_INCH_Z : 0.0f;
        positionObj["rot"] = stepper_rot ? (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE : 0.0f;
    } else {
        positionObj["x"] = 0.0f;
        positionObj["y"] = 0.0f;
        positionObj["z"] = 0.0f;
        positionObj["rot"] = 0.0f;
    }


    // All Configurable Settings
    JsonObject settingsObj = doc.createNestedObject("settings");

    // Grid/Tray Settings
    settingsObj["gridCols"] = placeGridCols;
    settingsObj["gridRows"] = placeGridRows;
    settingsObj["gapX"] = placeGapX_inch;
    settingsObj["gapY"] = placeGapY_inch;
    settingsObj["trayWidth"] = trayWidth_inch;
    settingsObj["trayHeight"] = trayHeight_inch;

    // Painting General Settings
    settingsObj["paintGunOffsetX"] = paintGunOffsetX_inch;
    settingsObj["paintGunOffsetY"] = paintGunOffsetY_inch;
    // Add general paint speed/accel if they become configurable via UI

    // Painting Side-Specific Settings
    for (int i = 0; i < 4; ++i) {
        settingsObj["paintZ_" + String(i)] = paintZHeight_inch[i];
        settingsObj["paintP_" + String(i)] = paintPitchAngle[i]; // Send raw angle (0-180)
        settingsObj["paintR_" + String(i)] = paintPatternType[i]; // Use 'R' key for pattern to match HTML select ID
        settingsObj["paintS_" + String(i)] = paintSpeed[i];
        // --- ADDED: Paint Start Positions ---
        settingsObj["paintStartX_" + String(i)] = paintStartX[i];
        settingsObj["paintStartY_" + String(i)] = paintStartY[i];
        // ------------------------------------
    }

    // PnP Specific Settings
    settingsObj["pnpPickX"] = pnpPickLocationX_inch;
    settingsObj["pnpPickY"] = pnpPickLocationY_inch;
    settingsObj["pnpPickZ"] = pnpPickLocationZ_inch;
    settingsObj["pnpPlaceZ"] = pnpPlaceHeight_inch;
    settingsObj["firstPlaceX"] = placeFirstXAbsolute_inch;
    settingsObj["firstPlaceY"] = placeFirstYAbsolute_inch;
    // Add PnP speeds/accels if they become configurable

    // Serialize JSON to string
    String output;
    serializeJson(doc, output);

    // Send to specific client or broadcast
    if (specificClientNum < 255) { // 255 used as indicator to broadcast
        // Serial.printf("[DEBUG] Sending settings update to client %d: %s\\n", specificClientNum, output.c_str());
        webSocket.sendTXT(specificClientNum, output);
    } else {
        // Serial.printf("[DEBUG] Broadcasting settings update: %s\\n", output.c_str());
        webSocket.broadcastTXT(output);
    }
}

