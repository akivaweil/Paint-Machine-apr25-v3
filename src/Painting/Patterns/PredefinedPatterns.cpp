#include "PredefinedPatterns.h"
// #include "PatternActions.h" // No longer needed here
#include "../../Main/SharedGlobals.h" // Needed for PnP variables used in helper
// #include "../Painting.h" // No longer needed here
#include <Arduino.h>
// #include "../../Main/GeneralSettings_PinDef.h" // No longer needed here

// === Helper: Calculate Spacing ===
// Calculates column and row spacing based on global settings.
// This is kept here as it's used by all side-specific pattern files.
void calculatePaintingSpacing(float &colSpacing, float &rowSpacing) {
    // Using PnP item dimensions and gaps for painting spacing for now
    // Adjust these formulas if painting requires different spacing
    colSpacing = pnpItemWidth_inch + placeGapX_inch; // Horizontal shift distance
    rowSpacing = pnpItemHeight_inch + placeGapY_inch; // Vertical sweep distance
}

// <<< REMOVED executeStandardSerpentineUpDown definition >>>


// <<< REMOVED executeStandardSerpentineSideways definition >>> 