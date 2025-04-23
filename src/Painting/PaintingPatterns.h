#ifndef PAINTING_PATTERNS_H
#define PAINTING_PATTERNS_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include <WebSocketsServer.h> // Include for status updates

// Forward declarations for external variables/objects needed by the patterns
// These are defined in main.cpp
extern FastAccelStepper *stepper_x;
extern FastAccelStepper *stepper_y_left;
extern FastAccelStepper *stepper_y_right;
extern WebSocketsServer webSocket;
extern volatile bool stopRequested;
extern volatile bool isMoving; // Pattern functions might need to check/clear this

// PnP dimensions needed for pattern calculations (declared extern in PickPlace.h)
extern const float pnpItemWidth_inch;
extern const float pnpItemHeight_inch;
extern float placeGapX_inch;
extern float placeGapY_inch;
extern float placeFirstXAbsolute_inch;
extern float placeFirstYAbsolute_inch;
extern int placeGridCols;
extern int placeGridRows;

// Painting offsets (declared extern in Painting.h)
extern float paintGunOffsetX_inch;
extern float paintGunOffsetY_inch;

// Helper function from main.cpp for painting moves
extern void moveToXYPositionInches_Paint(float targetX_inch, float targetY_inch, float speedHz, float accel);

/**
 * @brief Executes the Up-Down (Serpentine) painting pattern for Back (0) or Front (2) sides.
 *
 * @param sideIndex The side being painted (0 for Back, 2 for Front).
 * @param speed The desired painting speed (steps/sec).
 * @param accel The desired painting acceleration (steps/sec^2).
 * @return true if the pattern was stopped by the user, false otherwise.
 *
 * Pattern Logic (Side 0 - Back):
 * 1. Start at Top-Right reference corner.
 * 2. Move Left to the first column's X position.
 * 3. Sweep Down vertically along the first column.
 *       |
 *       V
 *       |
 * 4. Move Left to the second column's X position. <--
 * 5. Sweep Up vertically along the second column.   ^
 *       |                                         |
 * 6. Move Left to the third column's X position.  <--
 * 7. Sweep Down vertically along the third column.
 *       |
 *       V
 *       |
 * ... and so on.
 *
 * Pattern Logic (Side 2 - Front):
 * 1. Start at Top-Left reference corner.
 * 2. Move Right to the first column's X position.
 * 3. Sweep Up vertically along the first column.
 *       ^
 *       |
 * 4. Move Right to the second column's X position. -->
 * 5. Sweep Down vertically along the second column. V
 *       |                                         |
 * 6. Move Right to the third column's X position.   -->
 * 7. Sweep Up vertically along the third column.
 *       ^
 *       |
 * ... and so on.
 */
bool executeUpDownPattern(int sideIndex, float speed, float accel);


/**
 * @brief Executes the Left-Right (Serpentine) painting pattern for Left (3) or Right (1) sides.
 *        (Currently a placeholder - needs implementation)
 *
 * @param sideIndex The side being painted (1 for Right, 3 for Left).
 * @param speed The desired painting speed (steps/sec).
 * @param accel The desired painting acceleration (steps/sec^2).
 * @return true if the pattern was stopped by the user, false otherwise.
 */
bool executeLeftRightPattern(int sideIndex, float speed, float accel);


#endif // PAINTING_PATTERNS_H 