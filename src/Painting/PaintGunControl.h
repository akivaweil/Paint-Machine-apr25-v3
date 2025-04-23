#ifndef PAINT_GUN_CONTROL_H
#define PAINT_GUN_CONTROL_H

#include <Arduino.h>
#include "../Main/GeneralSettings_PinDef.h" // For pin definitions

// Function declarations for paint gun control

/**
 * @brief Initialize the paint gun control pins
 * This function should be called in setup() to configure the paint control pins
 */
void initializePaintGunControl();

/**
 * @brief Activate the paint gun
 * @param activatePressurePot If true, also activate the pressure pot (default: true)
 */
void activatePaintGun(bool activatePressurePot = true);

/**
 * @brief Deactivate the paint gun
 * @param deactivatePressurePot If true, also deactivate the pressure pot (default: true)
 */
void deactivatePaintGun(bool deactivatePressurePot = true);

/**
 * @brief Activate or deactivate the paint gun based on current movement direction
 * Implements pattern-specific logic:
 * - For sideways pattern (90), activate during X movement, deactivate during Y movement
 * - For vertical pattern (0), activate during Y movement, deactivate during X movement
 * 
 * @param isXMovement True if moving in X direction, false if moving in Y direction
 * @param patternType The current pattern type (0 for vertical, 90 for sideways)
 */
void updatePaintGunForMovement(bool isXMovement, int patternType);

#endif // PAINT_GUN_CONTROL_H 