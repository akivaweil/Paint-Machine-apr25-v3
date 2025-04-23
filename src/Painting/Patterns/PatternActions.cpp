#include "PatternActions.h"
#include "../../Main/SharedGlobals.h" // <<< ADDED INCLUDE for externs and prototypes
#include "../../Main/GeneralSettings_PinDef.h" // For steps/inch if needed, though might not be needed if currentX/Y updated correctly

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
        // Consider how to signal error - maybe return true?
        return false; // Continue for now, but rotation failed
    }

    // Call the existing global function (declared extern in .h)
    rotateToAbsoluteDegree(targetAngle); 

    // rotateToAbsoluteDegree has its own check for stopRequested and waits for completion.
    // We just need to check the flag *after* it returns in case it was stopped.
    return false; // Assume completion if we get here (stop is checked by caller or main loop)
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

// ACTION: Vertical Sweep
bool actionSweepVertical(bool sweepDown, float distance, float currentX, float &currentY, float speed, float accel) {
    float targetY = currentY + (sweepDown ? -distance : distance);
    char details[80];
    sprintf(details, "Sweeping %s by %.3f inches to Y=%.3f (at X=%.3f)", 
            sweepDown ? "Down" : "Up", distance, targetY, currentX);
    printAndBroadcastAction("SweepVertical", details);

    moveToXYPositionInches_Paint(currentX, targetY, speed, accel);
    // moveToXYPositionInches_Paint waits for completion.

    currentY = targetY; // Update position
    return false; // Completed normally
}

// ACTION: Horizontal Sweep
bool actionSweepHorizontal(bool sweepRight, float distance, float currentY, float &currentX, float speed, float accel) {
    float targetX = currentX + (sweepRight ? distance : -distance);
    char details[80];
    sprintf(details, "Sweeping %s by %.3f inches to X=%.3f (at Y=%.3f)",
            sweepRight ? "Right" : "Left", distance, targetX, currentY);
    printAndBroadcastAction("SweepHorizontal", details);

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