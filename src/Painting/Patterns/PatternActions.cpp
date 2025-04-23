#include "PatternActions.h"
#include "../../Main/SharedGlobals.h" // <<< ADDED INCLUDE for externs and prototypes
#include "../../Main/GeneralSettings_PinDef.h" // For steps/inch if needed, though might not be needed if currentX/Y updated correctly
#include "../PaintGunControl.h" // Include paint gun control

// Helper function: Prints a message to Serial and WebSocket
// Duplicated here for simplicity, could be moved to a shared utility
void printAndBroadcastAction(const char* actionName, const char* details) {
    Serial.printf("[Action: %s] %s\n", actionName, details);
    char jsonMsg[250]; // Adjust size as needed
    // Using Busy status for actions
    sprintf(jsonMsg, "{\"status\":\"Busy\", \"message\":\"Action [%s]: %s\"}", actionName, details);
    if (webSocket.connectedClients() > 0) { // Check if any clients are connected
         webSocket.broadcastTXT(jsonMsg);
    }
}

// --- Action Implementations --- 

// ACTION: Rotate Tray
bool actionRotateTo(int targetAngle) {
    char details[50];
    sprintf(details, "Rotating to %d degrees", targetAngle);
    printAndBroadcastAction("Rotate", details);

    if (!stepper_rot) {
        Serial.println("ERROR: Rotation stepper not available!");
        // Log more detailed error information
        Serial.println("  Check ROTATION_STEP_PIN and ROTATION_DIR_PIN in GeneralSettings_PinDef.h");
        Serial.println("  Ensure there are no pin conflicts with other steppers");
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Rotation stepper not available. Check configuration.\"}");
        return true; // Signal error to caller
    }

    // Get current position before rotation for logging
    long currentSteps = stepper_rot->getCurrentPosition();
    float currentAngle = (float)currentSteps / STEPS_PER_DEGREE;
    Serial.printf("  Current rotation before move: %.2f degrees (Steps: %ld)\n", currentAngle, currentSteps);
    Serial.printf("  Target rotation: %d degrees\n", targetAngle);

    // Call the existing global function (declared extern in .h)
    rotateToAbsoluteDegree(targetAngle); 

    // Log final position after rotation
    currentSteps = stepper_rot->getCurrentPosition();
    currentAngle = (float)currentSteps / STEPS_PER_DEGREE;
    Serial.printf("  Final rotation after move: %.2f degrees (Steps: %ld)\n", currentAngle, currentSteps);

    // rotateToAbsoluteDegree has its own check for stopRequested and waits for completion.
    // We just need to check the flag *after* it returns in case it was stopped.
    return stopRequested; // Return true if stopped, false if completed normally
}

// ACTION: Move To Absolute XY
bool actionMoveToXY(float targetX, float targetY, float speed, float accel, float &currentX, float &currentY) {
    char details[80];
    sprintf(details, "Moving to XY: (%.3f, %.3f)", targetX, targetY);
    printAndBroadcastAction("MoveToXY", details);

    moveToXYPositionInches_Paint(targetX, targetY, speed, accel);
    // moveToXYPositionInches_Paint waits for completion internally.
    
    // Update position tracking variables passed by reference
    currentX = targetX;
    currentY = targetY;
    return false; // Completed normally
}

// ACTION: Move To Absolute Z
bool actionMoveToZ(float targetZ, float speed, float accel) {
    char details[50];
    sprintf(details, "Moving to Z: %.3f inches", targetZ);
    printAndBroadcastAction("MoveToZ", details);

    moveZToPositionInches(targetZ, speed, accel);
    // moveZToPositionInches waits for completion internally.
    return false; // Assume completion
}

/**
 * @brief ACTION: Perform a vertical sweep (move along Y axis).
 * Sends status updates via WebSocket.
 * @param sweepDown True to sweep down (negative Y direction), false to sweep up (positive Y direction).
 * @param distance The distance to sweep in inches.
 * @param currentX The current X position (tool stays at this X).
 * @param currentY Reference to the current Y position (updated by this function).
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @param sideIndex The current painting side index (0-3)
 * @return true if stopped, false otherwise.
 */
bool actionSweepVertical(bool sweepDown, float distance, float currentX, float &currentY, float speed, float accel, int sideIndex) {
    float targetY = currentY + (sweepDown ? -distance : distance);
    char details[80];
    sprintf(details, "Sweeping %s by %.3f inches to Y=%.3f (at X=%.3f)", 
            sweepDown ? "Down" : "Up", distance, targetY, currentX);
    printAndBroadcastAction("SweepVertical", details);

    // Update paint gun for Y-axis movement (vertical sweep)
    updatePaintGunForMovement(false, paintPatternType[sideIndex]); // Use the specified side's pattern

    moveToXYPositionInches_Paint(currentX, targetY, speed, accel);
    // moveToXYPositionInches_Paint waits for completion.

    currentY = targetY; // Update position
    return false; // Completed normally
}

// ACTION: Horizontal Sweep
bool actionSweepHorizontal(bool sweepRight, float distance, float currentY, float &currentX, float speed, float accel, int sideIndex) {
    float targetX = currentX + (sweepRight ? distance : -distance);
    char details[80];
    sprintf(details, "Sweeping %s by %.3f inches to X=%.3f (at Y=%.3f)",
            sweepRight ? "Right" : "Left", distance, targetX, currentY);
    printAndBroadcastAction("SweepHorizontal", details);

    // Update paint gun for X-axis movement (horizontal sweep)
    updatePaintGunForMovement(true, paintPatternType[sideIndex]); // Use the specified side's pattern

    moveToXYPositionInches_Paint(targetX, currentY, speed, accel);
    // moveToXYPositionInches_Paint waits for completion.

    currentX = targetX; // Update position
    return false; // Completed normally
}

// ACTION: Relative Shift XY
bool actionShiftXY(float deltaX, float deltaY, float &currentX, float &currentY, float speed, float accel) {
    float targetX = currentX + deltaX;
    float targetY = currentY + deltaY;
    char details[100];
    sprintf(details, "Shifting by (dX:%.3f, dY:%.3f) to (X:%.3f, Y:%.3f)",
            deltaX, deltaY, targetX, targetY);
    printAndBroadcastAction("ShiftXY", details);

    moveToXYPositionInches_Paint(targetX, targetY, speed, accel);
    // moveToXYPositionInches_Paint waits for completion.

    currentX = targetX; // Update position
    currentY = targetY;
    return false; // Completed normally
} 