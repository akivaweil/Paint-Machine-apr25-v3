#include "PaintPatterns_SideSpecific.h"
#include "PatternActions.h"
#include "PredefinedPatterns.h" // For calculatePaintingSpacing helper
#include "../../Main/SharedGlobals.h"
#include "../Painting.h" // For ROT_POS_... constants
#include <Arduino.h>
#include "../../Main/GeneralSettings_PinDef.h" // For STEPS_PER_INCH_XY

// === Back Side Pattern (Side 0) ===
bool executePaintPatternBack(float speed, float accel) {
    const int sideIndex = 0; // Hard-coded for this file
    int patternType = paintPatternType[sideIndex]; // Read pattern type for this side

    Serial.printf("[Pattern Sequence] Executing BACK Side Pattern (Side %d) - Type: %s\n", 
                  sideIndex, (patternType == 0) ? "Up/Down" : (patternType == 90 ? "Sideways" : "Unknown"));

    float currentX = 0.0, currentY = 0.0; // Track current TCP position
    bool stopped = false;

    // --- Shared Pattern Setup --- 
    Serial.println("  Setting up BACK pattern...");
    
    // 1. Rotation Angle (Always Back for this file)
    int targetAngle = ROT_POS_BACK_DEG;
    Serial.printf("    Target Rotation: %d degrees\n", targetAngle);

    // 2. Calculate Spacing (Same for both patterns)
    float colSpacing = 0.0, rowSpacing = 0.0;
    calculatePaintingSpacing(colSpacing, rowSpacing); // Uses helper from PredefinedPatterns
    float verticalSweepDistance = (placeGridRows > 1) ? ((placeGridRows - 1) * rowSpacing) : 0.0f;
    float horizontalSweepDistance = (placeGridCols > 1) ? ((placeGridCols - 1) * colSpacing) : 0.0f;
    Serial.printf("    Spacing: Col=%.3f, Row=%.3f, VertSweep=%.3f, HorizSweep=%.3f\n", 
                  colSpacing, rowSpacing, verticalSweepDistance, horizontalSweepDistance);

    // --- Pattern Specific Setup & Execution --- 

    if (patternType == 0) { // --- Up/Down Pattern --- 
        Serial.println("  Executing Up/Down Pattern Logic for Back...");

        // 3. Determine Start XY & Sweep Direction (Hard-coded for Back Up/Down)
        float startX = placeFirstXAbsolute_inch + paintGunOffsetX_inch; // Top-Right X
        float startY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // Top-Right Y
        bool sweepDownFirst = true;
        Serial.printf("    Start Corner: Top-Right (%.3f, %.3f), First Sweep: DOWN vvv\n", startX, startY);

        // 4. Execute Action Sequence 
        stopped = actionRotateTo(targetAngle);
        if (stopped) { Serial.println("Pattern stopped during Rotation."); return true; }
        float startZ = paintZHeight_inch[sideIndex];
        Serial.printf("    Moving Z to paint height: %.3f\n", startZ);
        stopped = actionMoveToZ(startZ, patternZSpeed, patternZAccel);
        if (stopped) { Serial.println("Pattern stopped during Z Move."); return true; }
        currentX = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
        currentY = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; // Assume synced
        Serial.printf("    Moving to Start XY: (%.3f, %.3f)\n", startX, startY);
        stopped = actionMoveToXY(startX, startY, speed, accel, currentX, currentY);
        if (stopped) { Serial.println("Pattern stopped during initial XY Move."); return true; }

        // 5. Column Loop (Up-Down Pattern) 
        Serial.println("  Starting column sweeps...");
        bool currentSweepDown = sweepDownFirst;
        for (int c = 0; c < placeGridCols; ++c) {
            Serial.printf("\n  -- Column %d --\n", c);
            float shiftX = (c > 0) ? -colSpacing : 0.0f;
            if (c > 0) {
                Serial.printf("    Shifting horizontally by %.3f\n", shiftX);
                stopped = actionShiftXY(shiftX, 0.0f, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Shift to Column %d.\n", c); return true; }
            }
            if (verticalSweepDistance > 0.001) { 
                Serial.printf("    Sweeping vertically (Down=%s) by %.3f\n", currentSweepDown ? "true" : "false", verticalSweepDistance);
                stopped = actionSweepVertical(currentSweepDown, verticalSweepDistance, currentX, currentY, speed, accel, sideIndex);
                if (stopped) { Serial.printf("Pattern stopped during Vertical Sweep in Column %d.\n", c); return true; }
            } else {
                Serial.println("    Skipping vertical sweep (distance is zero).");
            }
            currentSweepDown = !currentSweepDown;
        } // End column loop

    } else if (patternType == 90) { // --- Sideways Pattern --- 
        Serial.println("  Executing Sideways Pattern Logic for Back...");
        
        // 3. Determine Start XY & Sweep Direction (Adapted for Back Sideways)
        // Similar to Right side pattern logic (Side 1), but with Back rotation angle.
        float startX = placeFirstXAbsolute_inch + paintGunOffsetX_inch; // Top-Right X (Same as UpDown for Back)
        float startY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // Top-Right Y (Same as UpDown for Back)
        bool sweepRightFirst = false; // Sweep LEFT first for Back side Sideways pattern
        float startRefY = startY; // Use the top Y as the reference for vertical shifts
        Serial.printf("    Start Corner: Top-Right (%.3f, %.3f), First Sweep: LEFT <<<\n", startX, startY);

        // 4. Execute Action Sequence
        stopped = actionRotateTo(targetAngle);
        if (stopped) { Serial.println("Pattern stopped during Rotation."); return true; }
        float startZ = paintZHeight_inch[sideIndex];
        Serial.printf("    Moving Z to paint height: %.3f\n", startZ);
        stopped = actionMoveToZ(startZ, patternZSpeed, patternZAccel);
        if (stopped) { Serial.println("Pattern stopped during Z Move."); return true; }
        currentX = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
        currentY = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; // Assume synced
        Serial.printf("    Moving to Start XY: (%.3f, %.3f)\n", startX, startY);
        stopped = actionMoveToXY(startX, startY, speed, accel, currentX, currentY);
        if (stopped) { Serial.println("Pattern stopped during initial XY Move."); return true; }

        // 5. Row Loop (Sideways Pattern)
        Serial.println("  Starting row sweeps...");
        bool currentSweepRight = sweepRightFirst;
        for (int r = 0; r < placeGridRows; ++r) {
            Serial.printf("\n  -- Row %d --\n", r);
            float shiftY = (r > 0) ? -rowSpacing : 0.0f; // Negative shift to move down
            if (r > 0) {
                Serial.printf("    Shifting vertically by %.3f\n", shiftY);
                stopped = actionShiftXY(0.0f, shiftY, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Shift to Row %d.\n", r); return true; }
            }
            if (horizontalSweepDistance > 0.001) { 
                Serial.printf("    Sweeping horizontally (Right=%s) by %.3f\n", currentSweepRight ? "true" : "false", horizontalSweepDistance);
                stopped = actionSweepHorizontal(currentSweepRight, horizontalSweepDistance, currentY, currentX, speed, accel, sideIndex);
                if (stopped) { Serial.printf("Pattern stopped during Horizontal Sweep in Row %d.\n", r); return true; }
            } else {
                Serial.println("    Skipping horizontal sweep (distance is zero).");
            }
            currentSweepRight = !currentSweepRight;
        } // End row loop

    } else { // --- Unknown Pattern Type --- 
        Serial.printf("[ERROR] Unknown paintPatternType: %d for Side %d\n", patternType, sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Unknown pattern type selected for Back side.\"}");
        return true; // Indicate an error/stop condition
    }

    // --- Pattern Completion --- 
    Serial.printf("[Pattern Sequence] BACK Side Pattern (Type %d) COMPLETED.\n", patternType);
    return false; // Completed successfully
} 