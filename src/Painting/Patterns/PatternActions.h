#ifndef PATTERN_ACTIONS_H
#define PATTERN_ACTIONS_H

#include <Arduino.h>
// Includes for types used in declarations, if not already brought in by Arduino.h
#include <FastAccelStepper.h>
#include <WebSocketsServer.h>

// NOTE: Extern declarations for global vars (steppers, webSocket, stopRequested) 
// and core functions (moveTo..., rotateTo...) are now expected to be included 
// via "../../Main/SharedGlobals.h" BEFORE this header is included in a .cpp file.


// === Action Function Declarations ===
// These functions represent the basic "blocks" for building patterns.
// They return 'true' if stopRequested becomes true during execution, 'false' otherwise.

/**
 * @brief ACTION: Rotate the tray to a specific angle.
 * Sends status updates via WebSocket.
 * @param targetAngle The target angle in degrees (0-360).
 * @return true if stopped, false otherwise.
 */
bool actionRotateTo(int targetAngle);

/**
 * @brief ACTION: Move the TCP to a specific absolute XY coordinate.
 * Used for moving to the start of a pattern or section.
 * Sends status updates via WebSocket.
 * @param targetX Target X in inches.
 * @param targetY Target Y in inches.
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @param currentX Reference to the current X position (updated by this function).
 * @param currentY Reference to the current Y position (updated by this function).
 * @return true if stopped, false otherwise.
 */
bool actionMoveToXY(float targetX, float targetY, float speed, float accel, float &currentX, float &currentY);

/**
 * @brief ACTION: Move the Z axis to a specific absolute height.
 * @param targetZ Target Z height in inches.
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @return true if stopped, false otherwise.
 */
bool actionMoveToZ(float targetZ, float speed, float accel);

/**
 * @brief ACTION: Perform a vertical sweep (move along Y axis).
 * Sends status updates via WebSocket.
 * @param sweepDown True to sweep down (negative Y direction), false to sweep up (positive Y direction).
 * @param distance The distance to sweep in inches.
 * @param currentX The current X position (tool stays at this X).
 * @param currentY Reference to the current Y position (updated by this function).
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @return true if stopped, false otherwise.
 */
bool actionSweepVertical(bool sweepDown, float distance, float currentX, float &currentY, float speed, float accel);

/**
 * @brief ACTION: Perform a horizontal sweep (move along X axis).
 * Sends status updates via WebSocket.
 * @param sweepRight True to sweep right (positive X direction), false to sweep left (negative X direction).
 * @param distance The distance to sweep in inches.
 * @param currentY The current Y position (tool stays at this Y).
 * @param currentX Reference to the current X position (updated by this function).
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @return true if stopped, false otherwise.
 */
bool actionSweepHorizontal(bool sweepRight, float distance, float currentY, float &currentX, float speed, float accel);

/**
 * @brief ACTION: Shift the XY position by a relative amount.
 * Useful for moving between columns or rows.
 * Sends status updates via WebSocket.
 * @param deltaX Change in X inches.
 * @param deltaY Change in Y inches.
 * @param currentX Reference to the current X position (updated by this function).
 * @param currentY Reference to the current Y position (updated by this function).
 * @param speed Movement speed (Hz).
 * @param accel Movement acceleration.
 * @return true if stopped, false otherwise.
 */
bool actionShiftXY(float deltaX, float deltaY, float &currentX, float &currentY, float speed, float accel);


#endif // PATTERN_ACTIONS_H 