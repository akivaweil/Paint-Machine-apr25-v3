#ifndef PREDEFINED_PATTERNS_H
#define PREDEFINED_PATTERNS_H

#include <Arduino.h>

// NOTE: Extern declarations for global vars (grid settings, offsets, speeds, etc.) 
// are now expected to be included via "../../Main/SharedGlobals.h" BEFORE this 
// header is included in a .cpp file.

// === Helper Function Declarations ===

/**
 * @brief Calculates column and row spacing based on global PnP settings.
 * Needed by side-specific pattern files.
 *
 * @param colSpacing Reference to store calculated column spacing (inches).
 * @param rowSpacing Reference to store calculated row spacing (inches).
 */
void calculatePaintingSpacing(float &colSpacing, float &rowSpacing);

#endif // PREDEFINED_PATTERNS_H 