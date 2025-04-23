#include "PaintGunControl.h"
#include "../Main/SharedGlobals.h"
#include "../Main/GeneralSettings_PinDef.h"

void initializePaintGunControl() {
    // Configure the paint gun and pressure pot pins as outputs
    pinMode(PAINT_GUN_PIN, OUTPUT);
    pinMode(PRESSURE_POT_PIN, OUTPUT);
    
    // Ensure both are initially off
    digitalWrite(PAINT_GUN_PIN, LOW);
    digitalWrite(PRESSURE_POT_PIN, LOW);
    
    // Serial.println("Paint Gun Control initialized");
}

void activatePaintGun(bool activatePressurePot) {
    // Activate paint gun
    digitalWrite(PAINT_GUN_PIN, HIGH);
    
    // Activate pressure pot if requested
    if (activatePressurePot) {
        digitalWrite(PRESSURE_POT_PIN, HIGH);
    }
    
    // Serial.println("Paint Gun activated");
}

void deactivatePaintGun(bool deactivatePressurePot) {
    // Deactivate paint gun
    digitalWrite(PAINT_GUN_PIN, LOW);
    
    // Deactivate pressure pot if requested
    if (deactivatePressurePot) {
        digitalWrite(PRESSURE_POT_PIN, LOW);
        Serial.println("Paint Gun and Pressure Pot deactivated");
    } else {
        Serial.println("Paint Gun deactivated (Pressure Pot remains active)");
    }
}

void updatePaintGunForMovement(bool isXMovement, int patternType) {
    // Implement pattern-specific control logic:
    // - For sideways pattern (90), activate during X movement, deactivate during Y movement
    // - For vertical pattern (0), activate during Y movement, deactivate during X movement
    
    if (patternType == 90) { // Sideways pattern
        if (isXMovement) {
            activatePaintGun();
        } else {
            deactivatePaintGun(false); // Deactivate gun but keep pressure pot on
        }
    } else if (patternType == 0) { // Vertical pattern
        if (!isXMovement) {
            activatePaintGun();
        } else {
            deactivatePaintGun(false); // Deactivate gun but keep pressure pot on
        }
    }
} 