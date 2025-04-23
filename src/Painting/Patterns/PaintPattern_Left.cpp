#include "PaintPatterns_SideSpecific.h"
#include "PatternActions.h"
#include "PredefinedPatterns.h" // For calculatePaintingSpacing helper
#include "../../Main/SharedGlobals.h"
#include "../Painting.h" // For ROT_POS_... constants
#include <Arduino.h>
#include "../../Main/GeneralSettings_PinDef.h" // For STEPS_PER_INCH_XY

// === Left Side Pattern (Side 3) ===
bool executePaintPatternLeft(float speed, float accel) {
    const int sideIndex = 3; // Hard-coded for this file
    int patternType = paintPatternType[sideIndex]; // Read pattern type for this side

    Serial.printf("[Pattern Sequence] Executing LEFT Side Pattern (Side %d) - Type: %s\n", 
                  sideIndex, (patternType == 0) ? "Up/Down" : (patternType == 90 ? "Sideways" : "Unknown"));

    float currentX = 0.0, currentY = 0.0; // Track current TCP position
    bool stopped = false;

    // --- Shared Pattern Setup --- 
    Serial.println("  Setting up LEFT pattern...");
    
    // 1. Rotation Angle (Always Left for this file)
    int targetAngle = ROT_POS_LEFT_DEG;
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
        Serial.println("  Executing Up/Down Pattern Logic for Left (User Defined)...");

        float startX = 8.0f; // UPDATED User defined start X
        float startY = 6.5f;  // UPDATED User defined start Y
        bool sweepUpFirst = true; // User defined: First sweep is UP
        float verticalSweepDistUser = trayWidth_inch; // User defined: sweep vertically by TRAY WIDTH
        float horizontalShiftDistUser = 3.0f + placeGapY_inch; // User defined: 3 inches + placeGapY_inch (Y gap for X shift)
        Serial.printf("    Start Corner: User Defined (%.3f, %.3f), First Sweep: UP ^^\n", startX, startY);
        Serial.printf("    Sweep Vertical (TrayWidth): %.3f, Shift Horizontal (3 + GapY): %.3f (GapY=%.3f)\n", 
                      verticalSweepDistUser, horizontalShiftDistUser, placeGapY_inch);

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

        // 5. Column Loop (Up-Down Pattern - User Defined)
        Serial.println("  Starting column sweeps...");
        bool currentSweepUp = sweepUpFirst; // Start with UP sweep
        for (int c = 0; c < placeGridRows; ++c) { // FLIPPED: Loop ROWS times based on user feedback
            Serial.printf("\n  -- Column/Sweep %d --\n", c); // Renamed label for clarity
            // Horizontal shift happens *before* the sweep for columns c > 0
            if (c > 0) {
                Serial.printf("    Shifting horizontally by %.3f (3.0 + %.3f)\n", horizontalShiftDistUser, placeGapY_inch);
                stopped = actionShiftXY(horizontalShiftDistUser, 0.0f, currentX, currentY, speed, accel); // Use user-defined horizontal shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Column %d.\n", c); return true; }
            }
            
            // Vertical sweep using user-defined distance (trayWidth_inch)
            if (verticalSweepDistUser > 0.001) { 
                // Note: actionSweepVertical takes sweepDown flag. sweepUp=true means sweepDown=false
                bool sweepDown = !currentSweepUp;
                Serial.printf("    Sweeping vertically (Up=%s) by %.3f\n", currentSweepUp ? "true" : "false", verticalSweepDistUser);
                stopped = actionSweepVertical(sweepDown, verticalSweepDistUser, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Vertical Sweep in Column %d.\n", c); return true; }
            } else {
                Serial.println("    Skipping vertical sweep (distance is zero).");
            }
            currentSweepUp = !currentSweepUp; // Alternate direction for next sweep
        } // End column loop

    } else if (patternType == 90) { // --- Sideways Pattern --- 
        Serial.println("  Executing Sideways Pattern Logic for Left (User Defined)...");
        
        float startX = 8.0f; // UPDATED User defined start X
        float startY = 6.5f;  // UPDATED User defined start Y
        bool sweepRightFirstUser = true; // User defined: First sweep is RIGHT
        float horizontalSweepDistUser = trayHeight_inch; // User defined: sweep horizontally by TRAY HEIGHT
        float verticalShiftDistUser = 3.0f + placeGapX_inch; // User defined: 3 inches + placeGapX_inch (X gap for Y shift)
        Serial.printf("    Start Corner: User Defined (%.3f, %.3f), First Sweep: RIGHT >>>\n", startX, startY);
        Serial.printf("    Sweep Horizontal (TrayHeight): %.3f, Shift Vertical (3 + GapX): %.3f (GapX=%.3f)\n", 
                      horizontalSweepDistUser, verticalShiftDistUser, placeGapX_inch);

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

        // 5. Row Loop (Sideways Pattern - User Defined)
        Serial.println("  Starting row sweeps...");
        bool currentSweepRight = sweepRightFirstUser;
        for (int r = 0; r < placeGridCols; ++r) { // FLIPPED: Loop COLS times based on user feedback
            Serial.printf("\n  -- Row/Sweep %d --\n", r); // Renamed label for clarity
            // Vertical shift happens *before* the sweep for rows r > 0
            if (r > 0) {
                 Serial.printf("    Shifting vertically by %.3f (3.0 + %.3f)\n", verticalShiftDistUser, placeGapX_inch);
                stopped = actionShiftXY(0.0f, verticalShiftDistUser, currentX, currentY, speed, accel); // Use user-defined vertical shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Row %d.\n", r); return true; }
            }
            // Horizontal sweep using user-defined distance (trayHeight_inch)
            if (horizontalSweepDistUser > 0.001) { 
                Serial.printf("    Sweeping horizontally (Right=%s) by %.3f\n", currentSweepRight ? "true" : "false", horizontalSweepDistUser);
                stopped = actionSweepHorizontal(currentSweepRight, horizontalSweepDistUser, currentY, currentX, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Horizontal Sweep in Row %d.\n", r); return true; }
            } else {
                Serial.println("    Skipping horizontal sweep (distance is zero).");
            }
            currentSweepRight = !currentSweepRight;
        } // End row loop

    } else { // --- Unknown Pattern Type --- 
        Serial.printf("[ERROR] Unknown paintPatternType: %d for Side %d\n", patternType, sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Unknown pattern type selected for Left side.\"}");
        return true; // Indicate an error/stop condition
    }

    // --- Pattern Completion --- 
    Serial.printf("[Pattern Sequence] LEFT Side Pattern (Type %d) COMPLETED.\n", patternType);
    return false; // Completed successfully
} 