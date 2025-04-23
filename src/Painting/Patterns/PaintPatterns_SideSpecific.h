#ifndef PAINT_PATTERNS_SIDE_SPECIFIC_H
#define PAINT_PATTERNS_SIDE_SPECIFIC_H

#include <Arduino.h>

// NOTE: Requires inclusion of SharedGlobals.h and PatternActions.h before use in .cpp

// === Side-Specific Pattern Function Declarations ===
// Each function executes the complete painting sequence for one side.
// They return 'true' if stopRequested becomes true during execution, 'false' otherwise.

/**
 * @brief Executes the painting sequence for the Back side (Side 0).
 * Uses the Up-Down serpentine pattern.
 * @param speed The painting speed for XY movements (Hz).
 * @param accel The painting acceleration for XY movements.
 * @return true if stopped by user or error, false otherwise.
 */
bool executePaintPatternBack(float speed, float accel);

/**
 * @brief Executes the painting sequence for the Front side (Side 2).
 * Uses the Up-Down serpentine pattern.
 * @param speed The painting speed for XY movements (Hz).
 * @param accel The painting acceleration for XY movements.
 * @return true if stopped by user or error, false otherwise.
 */
bool executePaintPatternFront(float speed, float accel);

/**
 * @brief Executes the painting sequence for the Right side (Side 1).
 * Uses the Sideways serpentine pattern.
 * @param speed The painting speed for XY movements (Hz).
 * @param accel The painting acceleration for XY movements.
 * @return true if stopped by user or error, false otherwise.
 */
bool executePaintPatternRight(float speed, float accel);

/**
 * @brief Executes the painting sequence for the Left side (Side 3).
 * Uses the Sideways serpentine pattern.
 * @param speed The painting speed for XY movements (Hz).
 * @param accel The painting acceleration for XY movements.
 * @return true if stopped by user or error, false otherwise.
 */
bool executePaintPatternLeft(float speed, float accel);


#endif // PAINT_PATTERNS_SIDE_SPECIFIC_H 