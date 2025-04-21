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
    webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Entering PnP Mode - Moving to Idle position...\"}");

    // Reset PnP state (already in PnP mode, just resetting sequence)
    currentPlaceCol = 0;
    currentPlaceRow = 0;
    pnpSequenceComplete = false;

    // Move to the defined IDLE position (2 inches above pick offset)
    float idleY_inch = pnpOffsetY_inch + 2.0f;

    // Move to the IDLE Position (XY)
    moveToXYPositionInches_PnP(pnpOffsetX_inch, idleY_inch); // Use idleY_inch

    // --- Wait for move completion --- (Blocking for simplicity here)
    unsigned long entryMoveStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        // Basic timeout check
        if (millis() - entryMoveStartTime > 15000) {
            Serial.println("[ERROR] Timeout waiting for move to Idle position!");
            if (stepper_x) stepper_x->forceStop();
            if (stepper_y_left) stepper_y_left->forceStop();
            if (stepper_y_right) stepper_y_right->forceStop();
            isMoving = false; // Force clear flag
            inPickPlaceMode = false; // Failed to enter mode, clear flag
            webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout moving to Idle position!\"}");
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
         Serial.println("[ERROR] Motors still running after move to Idle pos wait loop!");
         isMoving = false;
         inPickPlaceMode = false;
         webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Failed to reach Idle position reliably!\"}");
         return; // Failed to enter mode
    }

    Serial.println("[DEBUG] Reached Idle position. Clearing isMoving flag."); // DEBUG
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
    // Display current step before checks
    Serial.printf("[DEBUG-PnP] Step request - isMoving:%d, isHoming:%d, inPickPlaceMode:%d, pnpSequenceComplete:%d\n", 
                  isMoving, isHoming, inPickPlaceMode, pnpSequenceComplete);
                  
    if (!inPickPlaceMode) {
        Serial.println("[ERROR-PnP] Not in Pick and Place mode.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Not in Pick and Place mode.\"}");
        return;
    }

    if (isMoving || isHoming) {
        Serial.println("[ERROR-PnP] Machine is busy. Cannot execute next step.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Machine is busy. Wait for current operation to complete.\"}");
        return;
    }

    if (pnpSequenceComplete) {
        Serial.println("[DEBUG-PnP] PnP sequence already completed.");
        webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"PnP sequence already completed.\"}");
        return;
    }

    // Set busy state immediately
    isMoving = true;
    Serial.println("[DEBUG-PnP] Starting PnP step, set isMoving=true");
    webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Executing Pick and Place step...\"}");

    // == Move from Idle to Pick Location ==
    Serial.println("[DEBUG] Moving from Idle to Pick location...");
    webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Moving to Pick location...\"}");
    moveToXYPositionInches_PnP(pnpOffsetX_inch, pnpOffsetY_inch); // Move to actual pick Y
    // Wait for move completion
    unsigned long moveToPickStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        if (millis() - moveToPickStartTime > 15000) { /* Timeout check */
             Serial.println("[ERROR] Timeout moving to Pick location!");
             if (stepper_x) stepper_x->forceStop();
             if (stepper_y_left) stepper_y_left->forceStop();
             if (stepper_y_right) stepper_y_right->forceStop();
             isMoving = false; // Ensure flag is cleared on error
             webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout moving to Pick!\"}");
             return;
        }
        webSocket.loop(); yield(); // Keep responsive
    }
    Serial.println("[DEBUG] Arrived at Pick location.");

    // == Pick Action (User Steps 1-5) ==
    Serial.println("[DEBUG] 1. Performing Pick Action...");
    delay(100); // ADDED 100ms delay before starting pick action
    digitalWrite(SUCTION_PIN, HIGH);       // 1. Suction ON
    digitalWrite(PICK_CYLINDER_PIN, HIGH); // 2. Extend Cylinder
    delay(500);                            // 3. Wait
    digitalWrite(PICK_CYLINDER_PIN, LOW);  // 4. Retract Cylinder
    delay(200);                            // 5. Wait (CHANGED from 50ms to 200ms)
    // Suction stays ON
    Serial.println("[DEBUG] Pick Action Complete (Suction ON).");

    // == Move to Place Location (User Step 6) ==
    // Calculate target place coordinates for this step
    // Use absolute first place position and subtract grid offsets

    // Calculate Center-to-Center spacing dynamically
    // Formula: (tray_dimension - total_padding - first_item_size) / (number_of_gaps)
    float center_spacing_x = (placeGridCols > 1) ? (trayWidth_inch - 0.5f - 3.0f) / (placeGridCols - 1) : 0.0f;
    float center_spacing_y = (placeGridRows > 1) ? (trayHeight_inch - 0.5f - 3.0f) / (placeGridRows - 1) : 0.0f;

    // Use calculated center-to-center spacing
    float absoluteTargetX = placeFirstXAbsolute_inch - (currentPlaceCol * center_spacing_x); // Use center_spacing_x
    float absoluteTargetY = placeFirstYAbsolute_inch - (currentPlaceRow * center_spacing_y); // Use center_spacing_y

    char msgBuffer[150];
    // Update Debug Log to show calculated center spacing
    Serial.printf("[DEBUG] PnP Step Target Calc: Col=%d, Row=%d, FirstAbsX=%.2f, FirstAbsY=%.2f, CenterSpacingX=%.2f, CenterSpacingY=%.2f -> TargetX=%.2f, TargetY=%.2f\n",
                   currentPlaceCol, currentPlaceRow,
                   placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                   center_spacing_x, center_spacing_y, // Use calculated spacing in log
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
    digitalWrite(PICK_CYLINDER_PIN, HIGH); // 7. Extend Cylinder
    delay(500);                            // 8. Wait
    digitalWrite(SUCTION_PIN, LOW);        // 9. Turn Suction OFF
    delay(100);                            // 10. Wait
    digitalWrite(PICK_CYLINDER_PIN, LOW);  // 11. Retract Cylinder
    delay(150);                            // 12. Wait (Changed from 500ms)
    Serial.println("[DEBUG] Place Action Complete.");

    // == Return to IDLE Location ==
    float returnIdleY_inch = pnpOffsetY_inch + 2.0f; // Calculate idle Y again for clarity
    sprintf(msgBuffer, "{\"status\":\"Moving\", \"message\":\"PnP Step %d,%d: Returning to Idle (X: %.2f, Y: %.2f)\"}",
            currentPlaceRow + 1, currentPlaceCol + 1, pnpOffsetX_inch, returnIdleY_inch);
    Serial.println("[DEBUG] 13. Returning to Idle location...");
    Serial.println(msgBuffer); // Debug
    webSocket.broadcastTXT(msgBuffer);
    // Move XY to IDLE Location
    moveToXYPositionInches_PnP(pnpOffsetX_inch, returnIdleY_inch); // Use returnIdleY_inch
    // Wait for XY move to complete (blocking)
    unsigned long returnMoveStartTime = millis();
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
         if (millis() - returnMoveStartTime > 15000) { /* Timeout check */
             Serial.println("[ERROR] Timeout returning to Idle!");
             if (stepper_x) stepper_x->forceStop();
             if (stepper_y_left) stepper_y_left->forceStop();
             if (stepper_y_right) stepper_y_right->forceStop();
             isMoving = false;
             webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Timeout returning to Idle!\"}");
             return;
         }
        webSocket.loop(); yield(); // Keep responsive
    }
     Serial.printf("[DEBUG] Arrived back at Idle (X: %.2f, Y: %.2f).\n", pnpOffsetX_inch, returnIdleY_inch);
     Serial.println("[DEBUG] --- Completed PnP Step --- ");
     Serial.printf("[DEBUG] Current Grid Pos Before Increment: Col=%d, Row=%d\n", currentPlaceCol, currentPlaceRow); // DEBUG

    // == Wait for Movement Completion ==
    Serial.println("[DEBUG] PnP: Waiting for steppers to finish movement...");
    unsigned long movementStartTime = millis(); // For timeout tracking
    while (stepper_x->isRunning() || stepper_y_left->isRunning() || stepper_y_right->isRunning()) {
        yield(); // Allow background tasks
        webSocket.loop(); // Keep WebSocket responsive
        
        // Check for timeout (15 seconds max)
        if (millis() - movementStartTime > 15000) {
            Serial.println("[ERROR] PnP step move timed out after 15 seconds!");
            break; // Exit even if still moving - let the rest of the sequence continue
        }
    }
    Serial.println("[DEBUG] PnP: Stepper movement complete!");
    
    // Add a small delay to ensure serial messages are sent and motors have completely stopped
    delay(100);

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

    Serial.printf("[DEBUG] PnP Step End: New Grid Pos: Col=%d, Row=%d, SequenceComplete=%d\n", currentPlaceCol, currentPlaceRow, pnpSequenceComplete);
    Serial.println("[DEBUG-PnP] Completed PnP step, about to set isMoving=false");
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
    
    // Extra double-check safety delay at end of function to ensure motors have completely stopped 
    // and isMoving is properly cleared before returning
    delay(200);
    
    // Final safety check to ensure isMoving is definitely false by the time we exit
    if (isMoving) {
        Serial.println("[DEBUG-PnP] WARNING: isMoving was still true at end of function, forcing to false!");
        isMoving = false;
    } else {
        Serial.println("[DEBUG-PnP] Confirmed isMoving is false at end of function.");
    }
}

void moveToNextPickPlaceSquare()
{
    // Display current position before checks
    Serial.printf("[DEBUG-PnP] Next Square request - isMoving:%d, isHoming:%d, inPickPlaceMode:%d, pnpSequenceComplete:%d\n", 
                  isMoving, isHoming, inPickPlaceMode, pnpSequenceComplete);
                  
    if (!inPickPlaceMode) {
        Serial.println("[ERROR-PnP] Not in Pick and Place mode.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Not in Pick and Place mode.\"}");
        return;
    }

    if (isMoving || isHoming) {
        Serial.println("[ERROR-PnP] Machine is busy. Cannot move to next square.");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Machine is busy. Wait for current operation to complete.\"}");
        return;
    }

    if (pnpSequenceComplete) {
        Serial.println("[DEBUG-PnP] PnP sequence already completed.");
        webSocket.broadcastTXT("{\"status\":\"PickPlaceReady\", \"message\":\"PnP sequence already completed.\"}");
        return;
    }

    // Update the grid position without doing any physical actions
    currentPlaceCol++;
    if (currentPlaceCol >= placeGridCols) {
        currentPlaceCol = 0;
        currentPlaceRow++;
        if (currentPlaceRow >= placeGridRows) {
            pnpSequenceComplete = true;
            Serial.println("[DEBUG] PnP Sequence Complete (All grid positions finished).");
        }
    }

    Serial.printf("[DEBUG] PnP Next Square: New Grid Pos: Col=%d, Row=%d, SequenceComplete=%d\n", 
                  currentPlaceCol, currentPlaceRow, pnpSequenceComplete);

    char msgBuffer[150];
    // Update status message
    if (pnpSequenceComplete) {
        webSocket.broadcastTXT("{\"status\":\"PickPlaceComplete\",\"message\":\"PnP sequence complete. Press Home All Axis to exit.\"}");
    } else {
        // Ready for the next step
        sprintf(msgBuffer, "{\"status\":\"PickPlaceReady\",\"message\":\"Moved to square %d,%d. Ready for next step.\"}",
                currentPlaceRow + 1, currentPlaceCol + 1);
        webSocket.broadcastTXT(msgBuffer);
    }
}