# Paint Application System Implementation

## Overview
This implementation adds automated paint application control to the existing machine, with precise control over paint dispensing patterns based on movement direction.

## Hardware Configuration
- **Paint Gun Control**: Pin 12 (active HIGH)
- **Pressure Pot Control**: Pin 13 (active HIGH)

## Operational Parameters

### Pattern Control Logic
The system implements two distinct painting patterns:

#### Sideways Pattern (90°)
- Paint gun activates during X-axis movements
- Paint gun deactivates during Y-axis movements

#### Vertical Pattern (0°)
- Paint gun activates during Y-axis movements
- Paint gun deactivates during X-axis movements

### Position-Specific Operations

#### Clean Gun Position
- When executing the "Clean Gun" command, the system:
  1. Rotates to 0 degrees
  2. Moves to position (3.0, 10.0)
  3. Activates both paint gun and pressure pot for 3 seconds
  4. Deactivates both systems before returning home

#### Home Position
- Paint gun and pressure pot automatically deactivate before returning to home position
- Ensures system safety and prevents inadvertent paint dispensing

## Implementation Details

### New Files
- `src/Painting/PaintGunControl.h`: Interface for paint gun control
- `src/Painting/PaintGunControl.cpp`: Implementation of paint gun control functions

### Modified Files
- `src/Main/GeneralSettings_PinDef.h`: Added pin definitions
- `src/Main/main.cpp`: Added initialization and updated CLEAN_GUN command
- `src/Painting/Patterns/PatternActions.cpp`: Added paint gun control to movement actions
- `src/Painting/Patterns/PatternActions.h`: Updated function signatures
- `src/Painting/Patterns/PaintPattern_Back.cpp`: Updated to pass side index to action functions

### Key Functions
- `initializePaintGunControl()`: Sets up pins for paint gun control
- `activatePaintGun(bool activatePressurePot)`: Activates paint gun and optionally the pressure pot
- `deactivatePaintGun(bool deactivatePressurePot)`: Deactivates paint gun and optionally the pressure pot
- `updatePaintGunForMovement(bool isXMovement, int patternType)`: Controls paint gun based on movement direction and pattern type

## Using the System
No changes to the user interface were required. The existing "Paint Side X" and "Paint All Sides" buttons will now operate with the added paint gun control logic, automatically handling paint activation/deactivation based on the configured pattern for each side.

The "Clean Gun" button will now activate the actual paint gun (instead of the pick cylinder) when running its sequence. 