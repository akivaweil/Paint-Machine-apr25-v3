import matplotlib.pyplot as plt
import numpy as np

# --- Simulation Parameters (Based on defaults/loaded values) ---
placeFirstXAbsolute_inch = 20.0
placeFirstYAbsolute_inch = 20.0
placeGridCols = 4
placeGridRows = 5
trayWidth_inch = 18.0
trayHeight_inch = 26.0
paintGunOffsetX_inch = 0.0
paintGunOffsetY_inch = 1.5
padding = 0.5 # Assumed padding from JS calculation
itemSize = 3.0 # Assumed item size from JS calculation
sideIndex = 0 # Back Side

# --- Calculate Spacing ---
center_spacing_x_paint = 0.0
center_spacing_y_paint = 0.0
num_sweeps = 0
sweep_steps = 0

if sideIndex == 3: # Left Side (Not used here, but kept for completeness)
    center_spacing_x_paint = (placeGridRows > 1) * (trayWidth_inch - padding - itemSize) / (placeGridRows - 1) if (placeGridRows > 1) else 0.0
    center_spacing_y_paint = (placeGridCols > 1) * (trayHeight_inch - padding - itemSize) / (placeGridCols - 1) if (placeGridCols > 1) else 0.0
    num_sweeps = placeGridRows
    sweep_steps = placeGridCols
else: # Back (0) or Front (2)
    center_spacing_x_paint = (placeGridCols > 1) * (trayWidth_inch - padding - itemSize) / (placeGridCols - 1) if (placeGridCols > 1) else 0.0
    center_spacing_y_paint = (placeGridRows > 1) * (trayHeight_inch - padding - itemSize) / (placeGridRows - 1) if (placeGridRows > 1) else 0.0
    num_sweeps = placeGridRows
    sweep_steps = placeGridCols

# --- Calculate Start Point ---
pathStartX_itemGrid = placeFirstXAbsolute_inch
pathStartY_itemGrid = placeFirstYAbsolute_inch

startSweepTcpX = pathStartX_itemGrid + paintGunOffsetX_inch
startSweepTcpY = pathStartY_itemGrid + paintGunOffsetY_inch

# --- Generate Path Coordinates ---
x_coords = [startSweepTcpX]
y_coords = [startSweepTcpY]
currentTcpX = startSweepTcpX
currentTcpY = startSweepTcpY

print("Simulating Back Side (Side 0) Path...")
print(f"Start Point: ({currentTcpX:.2f}, {currentTcpY:.2f})")

for sweep in range(num_sweeps):
    targetTcpX = 0.0
    targetTcpY = 0.0

    # Calculate Target Y for this sweep (constant during horizontal sweep)
    # Back side progresses DOWN
    currentTcpY = startSweepTcpY - sweep * center_spacing_y_paint
    
    # Determine Horizontal Sweep Target X
    if sweep % 2 == 0: # Even sweep (0, 2, ...): Sweep LEFT (from right)
        targetTcpX = (pathStartX_itemGrid - (sweep_steps - 1) * center_spacing_x_paint) + paintGunOffsetX_inch
        print(f"Sweep {sweep} (Even): Sweeping Left to X={targetTcpX:.2f} (Y={currentTcpY:.2f})")
    else: # Odd sweep (1, 3, ...): Sweep RIGHT (from left)
        targetTcpX = pathStartX_itemGrid + paintGunOffsetX_inch
        print(f"Sweep {sweep} (Odd): Sweeping Right to X={targetTcpX:.2f} (Y={currentTcpY:.2f})")

    # Add sweep end point
    x_coords.append(targetTcpX)
    y_coords.append(currentTcpY)
    currentTcpX = targetTcpX # Update X

    # Move Vertically to Next Sweep's starting Y (if not the last sweep)
    if sweep < num_sweeps - 1:
        # Move DOWN
        nextSweepStartY = startSweepTcpY - (sweep + 1) * center_spacing_y_paint
        print(f"Sweep {sweep}: Moving Down to Y={nextSweepStartY:.2f} (X={currentTcpX:.2f})")
        
        # Add vertical move end point
        x_coords.append(currentTcpX)
        y_coords.append(nextSweepStartY)
        currentTcpY = nextSweepStartY # Update Y

# --- Plotting ---
plt.figure(figsize=(8, 10))
plt.plot(x_coords, y_coords, marker='o', linestyle='-', color='blue', label='TCP Path')

# Annotate points
for i, (x, y) in enumerate(zip(x_coords, y_coords)):
    plt.text(x + 0.1, y + 0.1, f'{i+1}', fontsize=9)

# Add Start and End markers
plt.plot(x_coords[0], y_coords[0], 'go', markersize=10, label='Start') # Green circle for start
plt.plot(x_coords[-1], y_coords[-1], 'rs', markersize=10, label='End') # Red square for end

# Calculate approximate item grid corners for reference
topLeftX = pathStartX_itemGrid - (sweep_steps - 1) * center_spacing_x_paint
topLeftY = pathStartY_itemGrid
bottomRightX = pathStartX_itemGrid
bottomRightY = pathStartY_itemGrid - (num_sweeps - 1) * center_spacing_y_paint

# Draw approximate item grid rectangle
rect = plt.Rectangle((topLeftX, bottomRightY), bottomRightX - topLeftX, topLeftY - bottomRightY, 
                     linewidth=1, edgecolor='gray', facecolor='none', linestyle='--', label='Approx. Item Grid')
plt.gca().add_patch(rect)

plt.title('Simulated TCP Path for Back Side Painting (Side 0)')
plt.xlabel('X Coordinate (inches)')
plt.ylabel('Y Coordinate (inches)')
plt.grid(True)
plt.axis('equal') # Ensure X and Y axes have the same scale
plt.gca().invert_yaxis() # Often makes sense for machine coordinates (0,0 top-left)
plt.legend()
plt.show() 