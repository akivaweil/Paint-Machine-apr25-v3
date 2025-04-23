#include "PaintingPatterns.h"
#include "../Main/GeneralSettings_PinDef.h"
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <WebSocketsServer.h>

// Note: Most extern variables are declared in PaintingPatterns.h

// Helper function: Prints a message to Serial and WebSocket
void printAndBroadcast(const char* message) {
    Serial.println(message);
    // Create a simple JSON status message
    char jsonMsg[200]; // Adjust size as needed
    sprintf(jsonMsg, "{\"status\":\"Busy\", \"message\":\"%s\"}", message); // Using Busy status during pattern execution
    webSocket.broadcastTXT(jsonMsg);
}

// --- Up-Down Pattern Implementation ---
bool executeUpDownPattern(int sideIndex, float speed, float accel) {
    Serial.printf("[Pattern] Executing Up-Down Pattern for Side %d\n", sideIndex);

    // Calculate spacing based on PnP settings
    float colSpacing = pnpItemWidth_inch + placeGapX_inch; // Horizontal shift distance
    float rowSpacing = pnpItemHeight_inch + placeGapY_inch; // Vertical sweep distance

    // --- Determine Start Point and Direction based on Side ---
    bool movingDownFirst; // Vertical direction for the FIRST column
    float startRefX;      // Horizontal reference point for calculations
    float initialTcpX;    // Initial X position to move to before starting columns
    float initialTcpY;    // Initial Y position (always top for this pattern)

    initialTcpY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // Start at the top Y reference

    if (sideIndex == 2) { // FRONT SIDE (Starts Top-Left, moves Right)
        printAndBroadcast("Pattern: Front (Side 2) - Starting Top-Left, Moving Right ->");
        // Start at the calculated *end* X of the back pattern (i.e., left-most X)
        startRefX = placeFirstXAbsolute_inch + paintGunOffsetX_inch - ((placeGridCols - 1) * colSpacing);
        initialTcpX = startRefX; 
        movingDownFirst = false; // Start sweeping UP for the first column
        Serial.printf("  Calculated Start: RefX=%.3f, TCP=(%.3f, %.3f), First Sweep: UP\n", startRefX, initialTcpX, initialTcpY, colSpacing, rowSpacing);
    } else { // BACK SIDE (Side 0) or default (Starts Top-Right, moves Left)
        printAndBroadcast("Pattern: Back (Side 0) - Starting Top-Right, Moving Left <-");
        // Original start point (Top-right reference)
        startRefX = placeFirstXAbsolute_inch + paintGunOffsetX_inch;
        initialTcpX = startRefX;
        movingDownFirst = true; // Start sweeping DOWN for the first column
        Serial.printf("  Calculated Start: RefX=%.3f, TCP=(%.3f, %.3f), First Sweep: DOWN\n", startRefX, initialTcpX, initialTcpY, colSpacing, rowSpacing);
    }

    // Get current position before starting the pattern
    // We assume Z is already at the correct height and servos are set.
    // The main `paintSide` function handles moving to the initial XY before calling this.
    float currentTcpX = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
    float currentTcpY = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; // Assume left/right are synced

    // Move to the calculated starting XY for the *entire pattern*
    // This is the corner before starting the first column traversal.
    Serial.printf("Moving to pattern start XY: (%.3f, %.3f)\n", initialTcpX, initialTcpY);
    moveToXYPositionInches_Paint(initialTcpX, initialTcpY, speed, accel);
    if (stopRequested) { return true; } // Check for stop
    currentTcpX = initialTcpX;
    currentTcpY = initialTcpY;


    // --- Execute Column Sweeps --- 
    Serial.printf("Starting column sweeps (Cols: %d, Rows: %d)\n", placeGridCols, placeGridRows);
    bool currentSweepDown = movingDownFirst;

    for (int c = 0; c < placeGridCols; ++c) {
        Serial.printf("\n-- Column %d --\n", c);
        
        // 1. Calculate target X for this column
        float targetColX;
        if (sideIndex == 2) { // FRONT - Move Right
            targetColX = startRefX + (c * colSpacing);
            // Serial.printf("  Target X (Front): %.3f = %.3f + (%d * %.3f)\n", targetColX, startRefX, c, colSpacing);
        } else { // BACK - Move Left
            targetColX = startRefX - (c * colSpacing);
            // Serial.printf("  Target X (Back): %.3f = %.3f - (%d * %.3f)\n", targetColX, startRefX, c, colSpacing);
        }

        // 2. Move Horizontally to the Column X position
        //    (Skip if it's the first column AND we are already at the correct X)
        if (c > 0 || abs(currentTcpX - targetColX) > 0.001) {
             Serial.printf("  Moving Horizontally -> X: %.3f (Current Y: %.3f)\n", targetColX, currentTcpY);
             moveToXYPositionInches_Paint(targetColX, currentTcpY, speed, accel);
             if (stopRequested) { return true; } // Check for stop
             currentTcpX = targetColX; // Update current X
        } else {
            // Serial.printf("  Skipping horizontal move for first column (already at X=%.3f)\n", currentTcpX);
        }
        
        // 3. Perform Vertical Sweep for this column
        float topY = initialTcpY; // Top reference Y (constant for all columns)
        float bottomY = initialTcpY - ((placeGridRows - 1) * rowSpacing); // Calculate bottom Y
        float targetSweepY; // The Y position to move to for this sweep

        if (currentSweepDown) {
            // Moving DOWN VVV
            targetSweepY = bottomY;
            Serial.printf("  Sweeping Vertically VVV to Y: %.3f (Current X: %.3f)\n", targetSweepY, currentTcpX);
        } else {
            // Moving UP ^^^ 
            targetSweepY = topY;
            Serial.printf("  Sweeping Vertically ^^^ to Y: %.3f (Current X: %.3f)\n", targetSweepY, currentTcpX);
        }

        moveToXYPositionInches_Paint(currentTcpX, targetSweepY, speed, accel);
        if (stopRequested) { return true; } // Check for stop
        currentTcpY = targetSweepY; // Update current Y

        // 4. Flip sweep direction for the NEXT column
        currentSweepDown = !currentSweepDown; 
        // Serial.printf("  Next column sweep direction will be: %s\n", currentSweepDown ? "Down" : "Up");
    } // End of column loop

    Serial.println("[Pattern] Up-Down Pattern Finished Successfully.");
    return false; // Pattern completed without being stopped
}


// --- Left-Right Pattern Implementation ---
bool executeLeftRightPattern(int sideIndex, float speed, float accel) {
    Serial.printf("[Pattern] Executing Left-Right Pattern for Side %d\n", sideIndex);
    webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Executing Left-Right pattern (placeholder)...\"}");
    
    // ************************************************************
    // ********* PLACEHOLDER - Needs Implementation ***************
    // ************************************************************
    // The logic for left-right serpentine (similar to up-down but
    // swapping X and Y roles) needs to be added here.
    // It will involve calculating rowSpacing, colSpacing differently
    // and iterating through rows, sweeping horizontally.
    
    Serial.println("WARNING: Left-Right pattern is not yet implemented!");
    delay(1000); // Simulate some work

    // For now, just return false (not stopped)
    return false; 
} 