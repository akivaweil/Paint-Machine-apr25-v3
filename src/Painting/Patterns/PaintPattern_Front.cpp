#include "PaintPatterns_SideSpecific.h"
#include "PatternActions.h"
#include "PredefinedPatterns.h" // For calculatePaintingSpacing helper
#include "../../Main/SharedGlobals.h"
#include "../Painting.h" // For ROT_POS_... constants
#include <Arduino.h>
#include "../../Main/GeneralSettings_PinDef.h" // For STEPS_PER_INCH_XY

// === Front Side Pattern (Side 2) ===
bool executePaintPatternFront(float speed, float accel) {
    const int sideIndex = 2; // Hard-coded for this file
    int patternType = paintPatternType[sideIndex]; // Read pattern type for this side

    Serial.printf("[Pattern Sequence] Executing FRONT Side Pattern (Side %d) - Type: %s\n", 
                  sideIndex, (patternType == 0) ? "Up/Down" : (patternType == 90 ? "Sideways" : "Unknown"));

    float currentX = 0.0, currentY = 0.0; // Track current TCP position
    bool stopped = false;

    // --- Shared Pattern Setup --- 
    Serial.println("  Setting up FRONT pattern...");
    
    // 1. Rotation Angle (Always Front for this file)
    int targetAngle = ROT_POS_FRONT_DEG;
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
        Serial.println("  Executing Up/Down Pattern Logic for Front (User Defined)...");

        // 3. Determine Start XY & Sweep Direction (USING GLOBAL SETTINGS)
        float startX = paintStartX[sideIndex]; // NEW: Use global setting
        float startY = paintStartY[sideIndex]; // NEW: Use global setting
        bool sweepUpFirst = true; // Front Up/Down sweeps UP first
        float verticalSweepDistUser = trayHeight_inch; // Sweep full tray height
        float horizontalShiftDistUser = 3.0f + placeGapX_inch; // Fixed shift + gap
        Serial.printf("    Start Corner: Global Setting (%.3f, %.3f), First Sweep: UP ^^^", startX, startY);
        Serial.printf("    Sweep Vertical: %.3f, Shift Horizontal (3 + GapX): %.3f (GapX=%.3f)\n", 
                      verticalSweepDistUser, horizontalShiftDistUser, placeGapX_inch);

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
        for (int c = 0; c < placeGridCols; ++c) {
            Serial.printf("\n  -- Column %d --\n", c);
            
            // Horizontal shift happens *before* the sweep for columns c > 0
            if (c > 0) {
                Serial.printf("    Shifting horizontally by %.3f (3.0 + %.3f)\n", horizontalShiftDistUser, placeGapX_inch);
                stopped = actionShiftXY(horizontalShiftDistUser, 0.0f, currentX, currentY, speed, accel); // Use user-defined horizontal shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Column %d.\n", c); return true; }
            }
            
            // Vertical sweep using user-defined distance
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
        Serial.println("  Executing Sideways Pattern Logic for Front (User Defined)...");
        
        // 3. Determine Start XY & Sweep Direction (USING GLOBAL SETTINGS)
        float startX = paintStartX[sideIndex]; // NEW: Use global setting
        float startY = paintStartY[sideIndex]; // NEW: Use global setting
        bool sweepRightFirst = true; // Front Sideways sweeps RIGHT first
        // Calculate spacing needed for vertical shifts
        float colSpacing = 0.0, rowSpacing = 0.0; // Need rowSpacing
        calculatePaintingSpacing(colSpacing, rowSpacing); // Get rowSpacing
        float verticalShiftDistance = 3.0f + placeGapY_inch; // NEW: User defined: 3 inches + placeGapY_inch (placeGapY_inch is calculated from UI)
        float horizontalSweepDist = trayWidth_inch; // Sweep full tray width
        Serial.printf("    Start Corner: Global Setting (%.3f, %.3f), First Sweep: RIGHT >>>\n", startX, startY);
        Serial.printf("    Sweep Horizontal: %.3f, Shift Vertical (3 + GapY): %.3f (GapY=%.3f)\n", horizontalSweepDist, verticalShiftDistance, placeGapY_inch); // NEW Print with GapY info


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

        // 5. Row Loop (Sideways Pattern for Front - Y increases)
        Serial.println("  Starting row sweeps...");
        bool currentSweepRight = sweepRightFirst;
        for (int r = 0; r < placeGridRows; ++r) {
            Serial.printf("\n  -- Row %d --\n", r);
            // Vertical shift happens *before* the sweep for rows r > 0
            if (r > 0) {
                Serial.printf("    Shifting vertically by %.3f (3.0 + %.3f)\n", verticalShiftDistance, placeGapY_inch); // NEW Print
                stopped = actionShiftXY(0.0f, verticalShiftDistance, currentX, currentY, speed, accel); // Use user-defined vertical shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Row %d.\n", r); return true; }
            }
            // Horizontal sweep across the tray width
            if (horizontalSweepDist > 0.001) { 
                Serial.printf("    Sweeping horizontally (Right=%s) by %.3f\n", currentSweepRight ? "true" : "false", horizontalSweepDist);
                // Note: actionSweepHorizontal updates currentX internally
                stopped = actionSweepHorizontal(currentSweepRight, horizontalSweepDist, currentY, currentX, speed, accel); 
                if (stopped) { Serial.printf("Pattern stopped during Horizontal Sweep in Row %d.\n", r); return true; }
            } else {
                Serial.println("    Skipping horizontal sweep (distance is zero).");
            }
            currentSweepRight = !currentSweepRight; // Alternate direction for next sweep
        } // End row loop

    } else { // --- Unknown Pattern Type --- 
        Serial.printf("[ERROR] Unknown paintPatternType: %d for Side %d\n", patternType, sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Unknown pattern type selected for Front side.\"}");
        return true; // Indicate an error/stop condition
    }

    // --- Pattern Completion --- 
    Serial.printf("[Pattern Sequence] FRONT Side Pattern (Type %d) COMPLETED.\n", patternType);
    return false; // Completed successfully
} 