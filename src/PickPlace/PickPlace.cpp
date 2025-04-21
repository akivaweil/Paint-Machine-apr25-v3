#include "PickPlace.h"
#include <WiFi.h> // Needed for WiFi.status() check
#include "../Main/GeneralSettings_PinDef.h" // Include pin definitions
#include <Arduino.h> // Include Arduino core

// === PnP Variable Definitions ===
// Define the variables declared extern in PickPlace.h
float pnpOffsetX_inch = 15.0; // Default X offset
float pnpOffsetY_inch = 0.0;  // Default Y offset
float placeFirstXAbsolute_inch = 20.0; // Default ABSOLUTE X
float placeFirstYAbsolute_inch = 20.0; // Default ABSOLUTE Y
int placeGridCols = 4;        // Default Columns
int placeGridRows = 5;        // Default Rows
float placeSpacingX_inch = 4.0f; // Default X Spacing
float placeSpacingY_inch = 4.0f; // Default Y Spacing

// Pattern Speed/Accel Variables (Defined in main.cpp, used by PnP moves)
// Declared extern in GeneralSettings_PinDef.h
extern float patternXSpeed;
extern float patternXAccel;
extern float patternYSpeed;
extern float patternYAccel;
extern float patternZSpeed;
extern float patternZAccel;

// === Internal PnP State Variables ===
// These are only used within the PnP logic.
int currentPlaceCol = 0; // 0-based index
int currentPlaceRow = 0; // 0-based index
bool pnpSequenceComplete = false;

// --- PnP Helper Functions ---

// PnP specific Z move
void moveToZ_PnP(float targetZ_inch, bool wait_for_completion /*= true*/) {
    if (!stepper_z) {
        Serial.println("ERROR: Cannot move Z (PnP) - Stepper not initialized.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Z Stepper not initialized.\"}");
        return;
    }

    // Convert inches to steps
    long targetZ_steps = (long)(targetZ_inch * STEPS_PER_INCH_Z);

    // === Z Limit Check ===
    long min_steps = (long)(Z_MAX_TRAVEL_NEG_INCH * STEPS_PER_INCH_Z);
    long max_steps = (long)(Z_HOME_POS_INCH * STEPS_PER_INCH_Z); // Should be 0
    targetZ_steps = max(min_steps, targetZ_steps); // Ensure not below min travel
    targetZ_steps = min(max_steps, targetZ_steps); // Ensure not above home position (0)
    // === End Z Limit Check ===

    if (stepper_z->getCurrentPosition() == targetZ_steps) {
        Serial.println("PnP: Already at target Z position.");
        return; // No move needed
    }

    // Set speed and acceleration
    stepper_z->setSpeedInHz(patternZSpeed);
    stepper_z->setAcceleration(patternZAccel);
    stepper_z->moveTo(targetZ_steps);

    // Optional wait
    if (wait_for_completion) {
        unsigned long zMoveStartTime = millis();
        while (stepper_z->isRunning()) {
            if (millis() - zMoveStartTime > 10000) { // Timeout (adjust as needed)
                 Serial.println("[ERROR] Timeout moving Z (PnP)!");
                 if (stepper_z) stepper_z->forceStop();
                 isMoving = false; // May need adjustment depending on where this is called
                 webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout moving Z (PnP)!\"}");
                 return; // Exit waiting
            }
            webSocket.loop(); yield(); // Keep responsive
        }
        // Serial.println("PnP: Z move complete.");
    }
}

// Function to move only X and Y axes to a target position in inches
// Assumes Z is already at the desired height. This is specific for PnP moves.
void moveToXYPositionInches_PnP(float targetX_inch, float targetY_inch) {
    // Check if steppers exist
    if (!stepper_x || !stepper_y_left || !stepper_y_right) {
        Serial.println("ERROR: Cannot move XY (PnP) - Steppers not initialized.");
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
        Serial.println("PnP: Already at target XY position.");
        return; // No move needed
    }

    // Set speeds and accelerations (Using PATTERN speeds for PnP moves)
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
    // Note: Completion is waited for within the calling PnP functions (enter/execute)
}

// --- Main PnP Functions ---

void enterPickPlaceMode() {
    if (!allHomed) {
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Machine not homed.\"}");
        return;
    }
     if (isMoving || isHoming || inPickPlaceMode) {
         webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Cannot enter PnP mode now.\"}");
         return;
    }

    Serial.println("[DEBUG] Entering Pick and Place Mode...");
    inPickPlaceMode = true; // Set flag BEFORE starting move to prevent loop() interference
    isMoving = true; // Block other actions during the initial move
    webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Entering PnP Mode - Moving to Pick position...\"}");

    // Reset PnP state (already in PnP mode, just resetting sequence)
    currentPlaceCol = 0;
    currentPlaceRow = 0;
    pnpSequenceComplete = false;

    // Move to the defined pick position (Using PnP specific move)
    // Move Z up to travel height first
    // moveToZ_PnP(PNP_Z_TRAVEL_INCH, true); // Wait for Z move

    // Move XY to Pick offset
    moveToXYPositionInches_PnP(pnpOffsetX_inch, pnpOffsetY_inch);

    // --- Wait for move completion --- (Blocking for simplicity here)
    unsigned long entryMoveStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        // Basic timeout check
        if (millis() - entryMoveStartTime > 15000) {
            Serial.println("[ERROR] Timeout waiting for move to Pick position!");
            if (stepper_x) stepper_x->forceStop();
            if (stepper_y_left) stepper_y_left->forceStop();
            if (stepper_y_right) stepper_y_right->forceStop();
            // if (stepper_z) stepper_z->forceStop(); // Stop Z if involved
            isMoving = false; // Force clear flag
            inPickPlaceMode = false; // Failed to enter mode, clear flag
            webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout moving to Pick position!\"}");
            return; // Failed to enter mode
        }
         // Keep server/OTA responsive
         if (WiFi.status() == WL_CONNECTED) { // Check WiFi status
             // ArduinoOTA.handle(); // OTA handled in main loop()
             webSocket.loop();
             // webServer.handleClient(); // HTTP handled in main loop()
         }
         yield();
    }
    // --- Move Completion ---

    Serial.printf("[DEBUG] enterPickPlaceMode: Move complete. isRunning X:%d YL:%d YR:%d\n", stepper_x->isRunning(), stepper_y_left->isRunning(), stepper_y_right->isRunning()); // DEBUG

    delay(50); // Add small delay to allow stepper state to settle

    // Double check motors stopped
    if (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) { // Check only XY
         Serial.println("[ERROR] Motors still running after move to Pick pos wait loop!");
         isMoving = false;
         inPickPlaceMode = false;
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Failed to reach Pick position reliably!\"}");
         return; // Failed to enter mode
    }

    Serial.println("[DEBUG] Reached Pick position. Clearing isMoving flag."); // DEBUG
    isMoving = false; // Clear busy flag AFTER move completes and state is confirmed
    webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"Pick/Place mode entered. Ready for step.\"}");
}

void exitPickPlaceMode(bool shouldHomeAfterExit /*= false*/) {
    if (!inPickPlaceMode) return; // Already out

    Serial.println("[DEBUG] Exiting Pick and Place Mode.");
    inPickPlaceMode = false;
    pnpSequenceComplete = false;
    if (shouldHomeAfterExit) {
        Serial.println("[DEBUG] PnP sequence complete. Requesting post-PnP homing.");
        pendingHomingAfterPnP = true;
        // Don't broadcast Ready yet, main loop will handle homing and status update
    } else {
        // Only broadcast Ready if not auto-homing
        webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Exited Pick/Place mode.\"}");
    }
}

void executeNextPickPlaceStep() {
     Serial.println("[DEBUG] executeNextPickPlaceStep: Entered function."); // DEBUG
     if (!inPickPlaceMode) {
        Serial.println("[DEBUG] executeNextPickPlaceStep: Failed check !inPickPlaceMode"); // DEBUG
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}");
        return;
    }
    if (isMoving || isHoming) {
         Serial.printf("[DEBUG] executeNextPickPlaceStep: Failed check isMoving=%d || isHoming=%d\n", isMoving, isHoming); // DEBUG
         webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Machine is busy.\"}");
         return;
    }
    if (pnpSequenceComplete) {
        Serial.println("[DEBUG] executeNextPickPlaceStep: Failed check pnpSequenceComplete"); // DEBUG
        webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"PnP sequence already completed.\"}");
        return;
    }
    Serial.println("[DEBUG] executeNextPickPlaceStep: Checks passed. Setting isMoving = true."); // DEBUG
    isMoving = true; // Set busy flag for the entire step
    webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Executing PnP Step...\"}");
    Serial.println("[DEBUG] --- Starting PnP Step --- ");

    // == Pick Action (User Steps 1-5) ==
    Serial.println("[DEBUG] 1. Performing Pick Action...");
    // Move Z down to Pick height

    digitalWrite(SUCTION_PIN, HIGH);       // 1. Suction ON
    digitalWrite(PICK_CYLINDER_PIN, HIGH); // 2. Extend Cylinder
    delay(500);                            // 3. Wait
    digitalWrite(PICK_CYLINDER_PIN, LOW);  // 4. Retract Cylinder
    delay(50);                             // 5. Wait (Reduced from 200ms)
    // Suction stays ON
    Serial.println("[DEBUG] Pick Action Complete (Suction ON).");

    // Move Z up to Travel height before XY move

    // == Move to Place Location (User Step 6) ==
    // Calculate target place coordinates for this step
    // Use absolute first place position and subtract grid offsets
    float absoluteTargetX = placeFirstXAbsolute_inch - (currentPlaceCol * placeSpacingX_inch);
    float absoluteTargetY = placeFirstYAbsolute_inch - (currentPlaceRow * placeSpacingY_inch);

    char msgBuffer[150];
    Serial.printf("[DEBUG] PnP Step Target Calc: Col=%d, Row=%d, FirstAbsX=%.2f, FirstAbsY=%.2f, SpacingX=%.2f, SpacingY=%.2f -> TargetX=%.2f, TargetY=%.2f\n",
                   currentPlaceCol, currentPlaceRow,
                   placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                   placeSpacingX_inch, placeSpacingY_inch,
                   absoluteTargetX, absoluteTargetY); // DEBUG
    sprintf(msgBuffer, "{\"status\":\"Moving\", \"message\":\"PnP Step %d,%d: Moving to Place (Abs: %.2f, %.2f)\"}",
            currentPlaceRow + 1, currentPlaceCol + 1, absoluteTargetX, absoluteTargetY);
    Serial.println("[DEBUG] 6. Moving to Place location...");
    Serial.println(msgBuffer); // Debug
    webSocket.broadcastTXT(msgBuffer);
    
    // Move XY to Place Location (Z is already at travel height)
    moveToXYPositionInches_PnP(absoluteTargetX, absoluteTargetY);
    // Wait for XY move to complete (blocking)
    unsigned long placeMoveStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        if (millis() - placeMoveStartTime > 15000) { /* Timeout check */
             Serial.println("[ERROR] Timeout moving to Place!");
             if (stepper_x) stepper_x->forceStop();
             if (stepper_y_left) stepper_y_left->forceStop();
             if (stepper_y_right) stepper_y_right->forceStop();
             isMoving = false;
             webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout moving to Place!\"}");
             return;
        }
        webSocket.loop(); yield(); // Keep responsive
    }
    Serial.printf("[DEBUG] Arrived at Place (Abs: %.2f, %.2f).\n", absoluteTargetX, absoluteTargetY);

    // == Place Action (User Steps 7-12) ==
    Serial.println("[DEBUG] 7. Performing Place Action...");
    // Move Z down to Place height

    digitalWrite(PICK_CYLINDER_PIN, HIGH); // 7. Extend Cylinder
    delay(500);                            // 8. Wait
    digitalWrite(SUCTION_PIN, LOW);        // 9. Turn Suction OFF
    delay(100);                            // 10. Wait
    digitalWrite(PICK_CYLINDER_PIN, LOW);  // 11. Retract Cylinder
    delay(150);                            // 12. Wait (Changed from 500ms)
    Serial.println("[DEBUG] Place Action Complete.");

    // Move Z up to Travel height

    // == Return to Pick Location (User Step 13) ==
    sprintf(msgBuffer, "{\"status\":\"Moving\", \"message\":\"PnP Step %d,%d: Returning to Pick (Abs: %.2f, %.2f)\"}",
            currentPlaceRow + 1, currentPlaceCol + 1, pnpOffsetX_inch, pnpOffsetY_inch);
    Serial.println("[DEBUG] 13. Returning to Pick location...");
    Serial.println(msgBuffer); // Debug
    webSocket.broadcastTXT(msgBuffer);
    // Move XY to Pick Location (Z is already at travel height)
    moveToXYPositionInches_PnP(pnpOffsetX_inch, pnpOffsetY_inch);
    // Wait for XY move to complete (blocking)
    unsigned long pickMoveStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
         if (millis() - pickMoveStartTime > 15000) { /* Timeout check */
             Serial.println("[ERROR] Timeout returning to Pick!");
             if (stepper_x) stepper_x->forceStop();
             if (stepper_y_left) stepper_y_left->forceStop();
             if (stepper_y_right) stepper_y_right->forceStop();
             isMoving = false;
             webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout returning to Pick!\"}");
             return;
         }
        webSocket.loop(); yield(); // Keep responsive
    }
     Serial.printf("[DEBUG] Arrived back at Pick (Abs: %.2f, %.2f).\n", pnpOffsetX_inch, pnpOffsetY_inch);
     Serial.println("[DEBUG] --- Completed PnP Step --- ");
     Serial.printf("[DEBUG] Current Grid Pos Before Increment: Col=%d, Row=%d\n", currentPlaceCol, currentPlaceRow); // DEBUG

    // == Update Grid Position for next step ==
    currentPlaceCol++;
    if (currentPlaceCol >= placeGridCols) {
        currentPlaceCol = 0;
        currentPlaceRow++;
        if (currentPlaceRow >= placeGridRows) {
            pnpSequenceComplete = true;
            Serial.println("[DEBUG] PnP Sequence Complete (All grid positions finished).");
        }
    }

    Serial.printf("[DEBUG] PnP Step End: Clearing isMoving flag. New Grid Pos: Col=%d, Row=%d, SequenceComplete=%d\n", currentPlaceCol, currentPlaceRow, pnpSequenceComplete); // DEBUG
    isMoving = false; // Clear busy flag for the whole step

    // == Send Status Update ==
    if (pnpSequenceComplete) {
        // Sequence complete, stay in PnP mode until user Homes.
        // exitPickPlaceMode(true); // Request homing after exit <-- REMOVED
        webSocket.broadcastTXT("{\"status\":\"PickPlaceComplete\",\"message\":\"PnP sequence complete. Press Home All Axis to exit.\"}");
    } else {
        // Ready for the next step
        // Use current indices as they point to the NEXT step to be executed
        sprintf(msgBuffer, "{\"status\":\"PickPlaceReady\",\"message\":\"PnP step %d,%d complete. Ready for next step (%d,%d).\"}",
                currentPlaceRow + 1, currentPlaceCol + 1, // Show the index of the step that *will* run next
                currentPlaceRow + 1, currentPlaceCol + 1);
        webSocket.broadcastTXT(msgBuffer);
    }
}

// Function to skip the current target location and move to the next one
void skipPickPlaceLocation() {
    Serial.println("[DEBUG] skipPickPlaceLocation: Entered function.");
    if (!inPickPlaceMode) {
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}");
        return;
    }
    if (isMoving || isHoming) {
        webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Machine is busy.\"}");
        return;
    }
    if (pnpSequenceComplete) {
        webSocket.broadcastTXT("{\"status\":\"PickPlaceComplete\", \"message\":\"Sequence already completed.\"}");
        return;
    }

    // Calculate the index of the *next* theoretical step
    int currentIndex = currentPlaceRow * placeGridCols + currentPlaceCol;
    int nextIndex = currentIndex + 1;
    int totalLocations = placeGridRows * placeGridCols;

    char msgBuffer[200];
    sprintf(msgBuffer, "{\"status\":\"Busy\", \"message\":\"Skipping location [%d,%d]...\"}", currentPlaceCol, currentPlaceRow);
    webSocket.broadcastTXT(msgBuffer);
    Serial.println(msgBuffer);

    if (nextIndex >= totalLocations) {
        // Skipped the last location, sequence is now complete
        Serial.println("[DEBUG] Skipped last location. Sequence complete.");
        pnpSequenceComplete = true;
        webSocket.broadcastTXT("{\"status\":\"PickPlaceComplete\", \"message\":\"Sequence completed by skipping last location.\"}");
    } else {
        // Update row and column to the next location (without moving)
        currentPlaceCol = nextIndex % placeGridCols;
        currentPlaceRow = nextIndex / placeGridCols;
        Serial.printf("[DEBUG] Skipped to next location: Col=%d, Row=%d\n", currentPlaceCol, currentPlaceRow);

        // Calculate new target coordinates (for reference only, no movement)
        float absoluteTargetX = placeFirstXAbsolute_inch - (currentPlaceCol * placeSpacingX_inch);
        float absoluteTargetY = placeFirstYAbsolute_inch - (currentPlaceRow * placeSpacingY_inch);
        Serial.printf("[DEBUG] Next location coordinates (not moving): X=%.2f, Y=%.2f\n", absoluteTargetX, absoluteTargetY);

        sprintf(msgBuffer, "{\"status\":\"PickPlaceReady\", \"message\":\"Skipped to location %d,%d. Ready for next step.\"}",
                currentPlaceRow + 1, currentPlaceCol + 1);
        webSocket.broadcastTXT(msgBuffer);
    }
}

// Function to move back to the previous target location
void goBackPickPlaceLocation() {
    Serial.println("[DEBUG] goBackPickPlaceLocation: Entered function.");
    if (!inPickPlaceMode) {
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Not in Pick/Place mode.\"}");
        return;
    }
    if (isMoving || isHoming) {
        webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Machine is busy.\"}");
        return;
    }

    // Calculate the index of the *previous* theoretical step
    int currentIndex = currentPlaceRow * placeGridCols + currentPlaceCol;
    int prevIndex = currentIndex - 1;

    char msgBuffer[200];

    if (prevIndex < 0) {
        // Already at the first location (or before it)
        Serial.println("[DEBUG] Cannot go back further.");
        webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"Already at first location.\"}");
        return;
    }

    sprintf(msgBuffer, "{\"status\":\"Busy\", \"message\":\"Going back from [%d,%d]...\"}", currentPlaceCol, currentPlaceRow);
    webSocket.broadcastTXT(msgBuffer);
    Serial.println(msgBuffer);

    // If the sequence was marked complete, going back means it's no longer complete
    if (pnpSequenceComplete) {
        Serial.println("[DEBUG] Sequence was complete, marking as incomplete due to move back.");
        pnpSequenceComplete = false;
    }

    // Update row and column to the previous location (without moving)
    currentPlaceCol = prevIndex % placeGridCols;
    currentPlaceRow = prevIndex / placeGridCols;
    Serial.printf("[DEBUG] Went back to location: Col=%d, Row=%d\n", currentPlaceCol, currentPlaceRow);

    // Calculate coordinates (for reference only, no movement)
    float absoluteTargetX = placeFirstXAbsolute_inch - (currentPlaceCol * placeSpacingX_inch);
    float absoluteTargetY = placeFirstYAbsolute_inch - (currentPlaceRow * placeSpacingY_inch);
    Serial.printf("[DEBUG] Previous location coordinates (not moving): X=%.2f, Y=%.2f\n", absoluteTargetX, absoluteTargetY);

    sprintf(msgBuffer, "{\"status\":\"PickPlaceReady\", \"message\":\"Moved back to location %d,%d. Ready for next step.\"}",
            currentPlaceRow + 1, currentPlaceCol + 1);
    webSocket.broadcastTXT(msgBuffer);
}