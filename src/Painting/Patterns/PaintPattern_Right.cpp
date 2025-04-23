#include "PaintPatterns_SideSpecific.h"
#include "PatternActions.h"
#include "PredefinedPatterns.h" // For calculatePaintingSpacing helper
#include "../../Main/SharedGlobals.h"
#include "../Painting.h" // For ROT_POS_... constants
#include <Arduino.h>
#include "../../Main/GeneralSettings_PinDef.h" // For STEPS_PER_INCH_XY

// === Right Side Pattern (Side 1) ===
bool executePaintPatternRight(float speed, float accel) {
    const int sideIndex = 1; // Hard-coded for this file
    int patternType = paintPatternType[sideIndex]; // Read pattern type for this side

    Serial.printf("[Pattern Sequence] Executing RIGHT Side Pattern (Side %d) - Type: %s\n", 
                  sideIndex, (patternType == 0) ? "Up/Down" : (patternType == 90 ? "Sideways" : "Unknown"));

    float currentX = 0.0, currentY = 0.0; // Track current TCP position
    bool stopped = false;

    // --- Shared Pattern Setup --- 
    Serial.println("  Setting up RIGHT pattern...");
    
    // 1. Rotation Angle (Always Right for this file)
    int targetAngle = ROT_POS_RIGHT_DEG;
    Serial.printf("    Target Rotation: %d degrees\n", targetAngle);

    // 2. Calculate Spacing (Same for both patterns)
    float colSpacing = 0.0, rowSpacing = 0.0;
    calculatePaintingSpacing(colSpacing, rowSpacing); // Uses helper from PredefinedPatterns
    float verticalSweepDistance = (placeGridRows > 1) ? ((placeGridRows - 1) * rowSpacing) : 0.0f;
    float horizontalSweepDistance = (placeGridCols > 1) ? ((placeGridCols - 1) * colSpacing) : 0.0f;
    Serial.printf("    Spacing: Col=%.3f, Row=%.3f, VertSweep=%.3f, HorizSweep=%.3f\n", 
                  colSpacing, rowSpacing, verticalSweepDistance, horizontalSweepDistance);

    // --- Pattern Specific Setup & Execution --- 

    // Calculate starting coordinates relative to Left side's start (8.0, 6.5)
    float startX_Right = 8.0f + 25.5f; // 33.5f
    float startY_Right = 6.5f + 17.5f; // 24.0f

    if (patternType == 0) { // --- Up/Down Pattern (User Defined) --- 
        // Serial.println("  Executing Up/Down Pattern Logic for Right..."); // OLD
        Serial.println("  Executing Up/Down Pattern Logic for Right (User Defined)...");

        // 3. Determine Start XY & Sweep Direction (User Defined)
        // float startX = placeFirstXAbsolute_inch + paintGunOffsetX_inch; // OLD 
        // float startY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // OLD
        // bool sweepDownFirst = true; // OLD
        // float startRefX = startX; // OLD
        // Serial.printf("    Start Corner: Top-Right (%.3f, %.3f), First Sweep: DOWN vvv\n", startX, startY); // OLD

        float startX = startX_Right; // Use calculated start X
        float startY = startY_Right; // Use calculated start Y
        bool sweepDownFirstUser = true; // User defined: First sweep is DOWN
        float verticalSweepDistUser = trayWidth_inch; // User defined: sweep vertically by TRAY WIDTH
        float horizontalShiftDistUser = 3.0f + placeGapY_inch; // User defined: 3 inches + placeGapY_inch (Y gap for X shift)
        Serial.printf("    Start Corner: User Defined (%.3f, %.3f), First Sweep: DOWN vvv\n", startX, startY);
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

        // 5. Column Loop (Up-Down Pattern for Right - User Defined)
        Serial.println("  Starting column sweeps...");
        bool currentSweepDown = sweepDownFirstUser; // Start with DOWN sweep
        for (int c = 0; c < placeGridCols; ++c) { // Loop COLUMNS times
            Serial.printf("\n  -- Column %d --\n", c);
            // Horizontal shift happens *before* the sweep for columns c > 0
            // float shiftX = (c > 0) ? -colSpacing : 0.0f; // OLD
            if (c > 0) {
                // Serial.printf("    Shifting horizontally by %.3f\n", shiftX); // OLD
                Serial.printf("    Shifting horizontally LEFT by %.3f (3.0 + %.3f)\n", horizontalShiftDistUser, placeGapY_inch);
                stopped = actionShiftXY(-horizontalShiftDistUser, 0.0f, currentX, currentY, speed, accel); // Use NEGATIVE user-defined horizontal shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Column %d.\n", c); return true; }
            }
            // Vertical sweep using user-defined distance (trayWidth_inch)
            // if (verticalSweepDistance > 0.001) { // OLD check
            if (verticalSweepDistUser > 0.001) { 
                Serial.printf("    Sweeping vertically (Down=%s) by %.3f\n", currentSweepDown ? "true" : "false", verticalSweepDistUser);
                stopped = actionSweepVertical(currentSweepDown, verticalSweepDistUser, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Vertical Sweep in Column %d.\n", c); return true; }
            } else {
                Serial.println("    Skipping vertical sweep (distance is zero).");
            }
            currentSweepDown = !currentSweepDown; // Alternate direction
        } // End column loop

    } else if (patternType == 90) { // --- Sideways Pattern (User Defined) --- 
        // Serial.println("  Executing Sideways Pattern Logic for Right..."); // OLD
        Serial.println("  Executing Sideways Pattern Logic for Right (User Defined)...");
        
        // 3. Determine Start XY & Sweep Direction (User Defined)
        // float startX = placeFirstXAbsolute_inch + paintGunOffsetX_inch; // OLD
        // float startY = placeFirstYAbsolute_inch + paintGunOffsetY_inch; // OLD
        // bool sweepRightFirst = false; // OLD 
        // float startRefY = startY; // OLD
        // Serial.printf("    Start Corner: Top-Right (%.3f, %.3f), First Sweep: LEFT <<<\n", startX, startY); // OLD

        float startX = startX_Right; // Use calculated start X
        float startY = startY_Right; // Use calculated start Y
        bool sweepLeftFirstUser = true; // User defined: First sweep is LEFT
        float horizontalSweepDistUser = trayHeight_inch; // User defined: sweep horizontally by TRAY HEIGHT
        float verticalShiftDistUser = 3.0f + placeGapX_inch; // User defined: 3 inches + placeGapX_inch (X gap for Y shift)
        Serial.printf("    Start Corner: User Defined (%.3f, %.3f), First Sweep: LEFT <<<\n", startX, startY);
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
        bool currentSweepLeft = sweepLeftFirstUser; // Start with LEFT sweep
        for (int r = 0; r < placeGridRows; ++r) { // Loop ROWS times
            Serial.printf("\n  -- Row %d --\n", r);
            // Vertical shift happens *before* the sweep for rows r > 0
            // float shiftY = (r > 0) ? -rowSpacing : 0.0f; // OLD
            if (r > 0) {
                 // Serial.printf("    Shifting vertically by %.3f\n", shiftY); // OLD
                 Serial.printf("    Shifting vertically DOWN by %.3f (3.0 + %.3f)\n", verticalShiftDistUser, placeGapX_inch);
                stopped = actionShiftXY(0.0f, -verticalShiftDistUser, currentX, currentY, speed, accel); // Use NEGATIVE user-defined vertical shift
                if (stopped) { Serial.printf("Pattern stopped during Shift to Row %d.\n", r); return true; }
            }
            // Horizontal sweep using user-defined distance (trayHeight_inch)
            // if (horizontalSweepDistance > 0.001) { // OLD check
             if (horizontalSweepDistUser > 0.001) { 
                // Note: actionSweepHorizontal takes sweepRight flag. sweepLeft=true means sweepRight=false
                bool sweepRight = !currentSweepLeft; 
                Serial.printf("    Sweeping horizontally (Left=%s) by %.3f\n", currentSweepLeft ? "true" : "false", horizontalSweepDistUser);
                stopped = actionSweepHorizontal(sweepRight, horizontalSweepDistUser, currentY, currentX, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Horizontal Sweep in Row %d.\n", r); return true; }
            } else {
                Serial.println("    Skipping horizontal sweep (distance is zero).");
            }
            currentSweepLeft = !currentSweepLeft; // Alternate direction
        } // End row loop

    } else { // --- Unknown Pattern Type --- 
        Serial.printf("[ERROR] Unknown paintPatternType: %d for Side %d\n", patternType, sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Unknown pattern type selected for Right side.\"}");
        return true; // Indicate an error/stop condition
    }

    // --- Pattern Completion --- 
    Serial.printf("[Pattern Sequence] RIGHT Side Pattern (Type %d) COMPLETED.\n", patternType);
    return false; // Completed successfully
} 