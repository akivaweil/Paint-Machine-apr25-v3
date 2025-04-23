#include "PaintPatterns_SideSpecific.h"
#include "PatternActions.h"
#include "PredefinedPatterns.h" // For calculatePaintingSpacing helper
#include "../../Main/SharedGlobals.h"
#include "../Painting.h" // For ROT_POS_... constants
#include <Arduino.h>
#include "../../Main/GeneralSettings_PinDef.h" // For STEPS_PER_INCH_XY

// === Right Side Pattern (Side 2) ===
bool executePaintPatternRight(float speed, float accel) {
    const int sideIndex = 2; // Hard-coded for this file
    int patternType = paintPatternType[sideIndex]; // Read pattern type for this side

    Serial.printf("[Pattern Sequence] Executing RIGHT Side Pattern (Side %d) - Type: %s\n", 
                  sideIndex, (patternType == PATTERN_UP_DOWN) ? "Up_Down" : (patternType == PATTERN_SIDEWAYS ? "Sideways" : "Unknown"));

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
    Serial.printf("    Spacing: Col=%.3f, Row=%.3f\n", colSpacing, rowSpacing);

    // --- Common setup for both pattern types ---
    // UPDATED: Rotation must complete first, then Z and XY positioning
    float startX = 25.0f;  // Hard-coded start X per requirements
    float startY = 30.0f;  // Hard-coded start Y per requirements
    float startZ = paintZHeight_inch[sideIndex];
    
    // Position the machine - ROTARY MOVEMENT FIRST
    Serial.println("  Positioning machine before sweeping...");
    
    // 1. First rotate to the target angle and wait for completion
    Serial.printf("    Rotating to %d degrees and waiting for completion\n", targetAngle);
    stopped = actionRotateTo(targetAngle);
    if (stopped) { Serial.println("Pattern stopped during Rotation."); return true; }
    
    Serial.println("    Rotation complete!");
    
    // 2. Move Z to the painting height
    Serial.printf("    Moving Z to paint height: %.3f\n", startZ);
    stopped = actionMoveToZ(startZ, patternZSpeed, patternZAccel);
    if (stopped) { Serial.println("Pattern stopped during Z Move."); return true; }
    
    // 3. Move XY to the starting position - only after rotation is complete
    currentX = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
    currentY = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY; // Assume synced
    Serial.printf("    Moving to Start XY: (%.3f, %.3f)\n", startX, startY);
    stopped = actionMoveToXY(startX, startY, speed, accel, currentX, currentY);
    if (stopped) { Serial.println("Pattern stopped during initial XY Move."); return true; }
    
    Serial.println("  All positioning complete. Beginning painting pattern...");

    // --- Pattern Specific Execution --- 
    if (patternType == PATTERN_UP_DOWN) { // --- Up/Down Pattern --- 
        Serial.println("  Executing Up_Down Pattern Logic for Right...");

        bool sweepDownFirst = true; // Right side sweeps DOWN first
        
        // Calculate sweep distance from tray height
        float verticalSweepDistance = trayHeight_inch;
        // Calculate shift distance for the pattern
        float horizontalShiftDistance = 3.0f + placeGapX_inch;
        
        Serial.printf("    Vertical Sweep: %.3f (trayHeight), Horizontal Shift: %.3f (3 + %.3f)\n", 
                      verticalSweepDistance, horizontalShiftDistance, placeGapX_inch);

        // 5. Column Loop (Up-Down Pattern) 
        Serial.println("  Starting column sweeps...");
        bool currentSweepDown = sweepDownFirst;
        
        // UPDATED: Execute placeGridCols number of vertical sweeps
        for (int c = 0; c < placeGridCols; ++c) {
            Serial.printf("\n  -- Column %d --\n", c);
            
            // Horizontal shift happens *before* the sweep for columns c > 0
            if (c > 0) {
                Serial.printf("    Shifting horizontally by %.3f (positive direction)\n", horizontalShiftDistance);
                stopped = actionShiftXY(horizontalShiftDistance, 0.0f, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Shift to Column %d.\n", c); return true; }
            }
            
            // Vertical sweep
            if (verticalSweepDistance > 0.001) { 
                Serial.printf("    Sweeping vertically (Down=%s) by %.3f\n", currentSweepDown ? "true" : "false", verticalSweepDistance);
                stopped = actionSweepVertical(currentSweepDown, verticalSweepDistance, currentX, currentY, speed, accel, sideIndex);
                if (stopped) { Serial.printf("Pattern stopped during Vertical Sweep in Column %d.\n", c); return true; }
            } else {
                Serial.println("    Skipping vertical sweep (distance is zero).");
            }
            
            // Alternate sweep direction for next column
            currentSweepDown = !currentSweepDown;
        } // End column loop

    } else if (patternType == PATTERN_SIDEWAYS) { // --- Sideways Pattern --- 
        Serial.println("  Executing Sideways Pattern Logic for Right...");
        
        bool sweepRightFirst = true; // Sweep RIGHT first for Right side Sideways pattern
        
        // Calculate sweep distance from tray width
        float horizontalSweepDistance = trayWidth_inch;
        // Calculate shift distance for the pattern
        float verticalShiftDistance = 3.0f + placeGapY_inch;
        
        Serial.printf("    First Sweep: RIGHT >>>\n");
        Serial.printf("    Horizontal Sweep: %.3f (trayWidth), Vertical Shift: %.3f (3 + %.3f)\n", 
                      horizontalSweepDistance, verticalShiftDistance, placeGapY_inch);

        // 5. Row Loop (Sideways Pattern)
        Serial.println("  Starting row sweeps...");
        bool currentSweepRight = sweepRightFirst; // Start with RIGHT sweep (sweepRight = true)
        
        // UPDATED: Execute placeGridRows number of horizontal sweeps
        for (int r = 0; r < placeGridRows; ++r) {
            Serial.printf("\n  -- Row %d --\n", r);
            
            // Vertical shift happens *before* the sweep for rows r > 0
            if (r > 0) {
                Serial.printf("    Shifting vertically by %.3f (positive direction)\n", verticalShiftDistance);
                stopped = actionShiftXY(0.0f, verticalShiftDistance, currentX, currentY, speed, accel);
                if (stopped) { Serial.printf("Pattern stopped during Shift to Row %d.\n", r); return true; }
            }
            
            // Horizontal sweep
            if (horizontalSweepDistance > 0.001) { 
                Serial.printf("    Sweeping horizontally (Right=%s) by %.3f\n", currentSweepRight ? "true" : "false", horizontalSweepDistance);
                stopped = actionSweepHorizontal(currentSweepRight, horizontalSweepDistance, currentY, currentX, speed, accel, sideIndex);
                if (stopped) { Serial.printf("Pattern stopped during Horizontal Sweep in Row %d.\n", r); return true; }
            } else {
                Serial.println("    Skipping horizontal sweep (distance is zero).");
            }
            
            // Alternate sweep direction for next row
            currentSweepRight = !currentSweepRight;
        } // End row loop

    } else { // --- Unknown Pattern Type --- 
        Serial.printf("[ERROR] Unknown paintPatternType: %d for Side %d\n", patternType, sideIndex);
        webSocket.broadcastTXT("{\"status\":\"Error\", \"message\":\"Unknown pattern type selected for Right side.\"}");
        return true; // Indicate an error/stop condition
    }

    // --- Pattern Completion --- 
    Serial.printf("[Pattern Sequence] RIGHT Side Pattern (Type %s) COMPLETED.\n", 
                  (patternType == PATTERN_UP_DOWN) ? "Up_Down" : (patternType == PATTERN_SIDEWAYS ? "Sideways" : "Unknown"));
    return false; // Completed successfully
} 