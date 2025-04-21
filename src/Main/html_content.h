#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <Arduino.h>

const char HTML_PROGMEM[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Stepper Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #222; color: #eee; } /* Darker gray background */
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px;
              text-align: center; text-decoration: none; display: inline-block;
              font-size: 16px; margin: 10px; cursor: pointer; border-radius: 8px; 
              transition: transform 0.1s ease-out, background-color 0.1s ease-out; /* Added transition */ 
            }
    .button:active { /* Added active state for click animation */
        transform: scale(0.95);
        background-color: #367c39; /* Slightly darker green on click */
    }
    .button:disabled { background-color: #cccccc; cursor: not-allowed; transform: none; } /* Ensure disabled buttons don't transform */
    .input-group { margin: 15px; display: inline-block; vertical-align: top; border: 2px solid #ccc; padding: 10px; border-radius: 5px; text-align: left; background-color: #444; } /* Slightly lighter background for groups, thicker border */
    .input-group h3 { margin-top: 0; text-align: center; color: #eee; } /* Explicit color for headings */
    .input-group label { margin-right: 5px; display: inline-block; width: 60px; color: #ddd;} /* Lighter label text */
    .input-group input { padding: 8px; width: 90px; text-align: right; margin-bottom: 5px; background-color: #555; color: #eee; border: 1px solid #777; border-radius: 5px; } /* Darker input fields, slightly lighter border, rounded corners */
    .input-group span { display: block; font-size: 0.8em; color: #bbb; margin-top: 0px; margin-bottom: 10px; } /* Lighter span text */
    .setting-button { padding: 10px 15px !important; margin-left: 10px !important; display: block; margin: 5px auto !important;}
    #status { margin-top: 20px; font-weight: bold; clear: both; }
    /* Custom styles */
    #homeButton { background-color: #17a2b8; } /* Blue for Home button */
    #homeButton:active { background-color: #117a8b; } /* Darker blue on click */
    #setRotationZeroButton { background-color: #ff9800; } /* Orange for Set Zero */
    #setRotationZeroButton:active { background-color: #cc7a00; } /* Darker orange on click */
    #downloadSettingsButton { background-color: #007bff; } /* Blue for Download */
    #downloadSettingsButton:active { background-color: #0056b3; } /* Darker blue on click */
    #uploadSettingsButton { background-color: #28a745; } /* Green for Upload */
    #uploadSettingsButton:active { background-color: #1e7e34; } /* Darker green on click */
    #exitCalibrationButton { background-color: #f44336; } /* Red for Exit Cal */
    #exitCalibrationButton:active { background-color: #d32f2f; } /* Darker red on click */
    /* Section Colors */
    .section-grid .input-group { border-color: #4285F4; } /* Blue */
    .section-pnp .input-group { border-color: #34A853; } /* Green */
    .section-manual .input-group { border-color: #FBBC05; } /* Orange */
    .section-paint .input-group { border-color: #A142F4; } /* Purple */
    .section-rotation .input-group { border-color: #E91E63; } /* Pink */

    /* Section Header Colors */
    .section-grid h2 { color: #4285F4; } /* Blue */
    .section-pnp h2 { color: #34A853; } /* Green */
    .section-rotation h2 { color: #E91E63; } /* Pink */
    .section-manual h2 { color: #FBBC05; } /* Orange */
    .section-paint h2 { color: #A142F4; } /* Purple */

    /* Hide number input spinners */
    input[type=number]::-webkit-outer-spin-button,
    input[type=number]::-webkit-inner-spin-button {
      -webkit-appearance: none;
      margin: 0;
    }
    input[type=number] {
      -moz-appearance: textfield; /* Firefox */
    }

    .exit-button { background-color: #f44336; } /* Red for Exit button */
    .exit-button:active { background-color: #d32f2f; } /* Darker red on click */

  </style>
</head>
<body>
  <h1>Paint + Pick and Place Machine</h1> <!-- Changed header -->
  <button id="homeButton" class="button" onclick="sendCommand('HOME')">Home All Axes</button>
  <button id="gotoButton" class="button" onclick="sendCommand('GOTO_5_5_0')">Go to (5, 5, 0)</button>
  <button id="gotoButton2020" class="button" onclick="sendCommand('GOTO_20_20_0')">Go to (20, 20, 0)</button>
  <hr>
  <div class="section-grid"> <!-- ADDED -->
    <h2>Master Grid Settings</h2>
    <div class="input-group">
        <h3>Grid & Spacing</h3>
        <label for="gridCols">Columns:</label>
        <input type="number" id="gridCols" step="1" value="4">
        <label for="gridRows">Rows:</label>
        <input type="number" id="gridRows" step="1" value="5">
        <span id="gridDisplay">Current: 4 x 5</span>
        <br>
        <span id="spacingDisplay">Spacing: X=Auto, Y=Auto</span> <!-- Updated display text -->
        <button id="setGridSpacingButton" class="button setting-button" onclick="setGridSpacing()">Set Grid Columns/Rows</button> <!-- Updated button text -->
    </div>
    <div class="input-group"> <!-- Added Tray Dimensions Group -->
        <h3>Tray Dimensions</h3>
        <label for="trayWidth">Width:</label>
        <input type="number" id="trayWidth" step="0.1" value="24.0"> <!-- Default Value -->
        <label for="trayHeight">Height:</label>
        <input type="number" id="trayHeight" step="0.1" value="18.0"> <!-- Default Value -->
        <span id="traySizeDisplay">Current: W=24.00, H=18.00</span> <!-- Default Display -->
        <button id="setTraySizeButton" class="button setting-button" onclick="setTraySize()">Set Tray Size</button>
    </div>
  </div> <!-- ADDED -->
  <hr>
  <div class="section-pnp"> <!-- ADDED -->
    <h2>Pick & Place Control</h2>
    <div class="input-group">
        <h3>Pickup Location</h3> <!-- Changed header -->
        <label for="offsetX">X:</label>
        <input type="number" id="offsetX" step="0.1" value="15.0">
        <label for="offsetY">Y:</label>
        <input type="number" id="offsetY" step="0.1" value="0.0">
        <span id="offsetDisplay">Current: X=15.00, Y=0.00</span>
        <button id="setOffsetButton" class="button setting-button" onclick="setOffset()">Set Offset</button>
    </div>

    <div class="input-group">
        <h3>First Drop Off Location</h3> <!-- Changed header -->
        <label for="firstPlaceXAbs">Abs X:</label>
        <input type="number" id="firstPlaceXAbs" step="0.1" value="20.0"> <!-- Default updated -->
        <label for="firstPlaceYAbs">Abs Y:</label>
        <input type="number" id="firstPlaceYAbs" step="0.1" value="20.0"> <!-- Default updated -->
        <span id="firstPlaceAbsDisplay">Current: X=20.00, Y=20.00</span> <!-- ID and Default updated -->
        <button id="setFirstPlaceAbsButton" class="button setting-button" onclick="setFirstPlaceAbs()">Set Drop Off</button> <!-- Changed text -->
    </div>

    <div class="input-group">
        <h3>Speed/Accel (k steps/s)</h3>
        <label for="xSpeed">X Spd:</label>
        <input type="number" id="xSpeed" step="1" value="20"> <!-- Represented as 20 -->
        <span id="xSpeedDisplay">Current: 20</span> <!-- Represented as 20 -->
        <label for="xAccel">X Acc:</label>
        <input type="number" id="xAccel" step="1" value="20"> <!-- Represented as 20 -->
        <span id="xAccelDisplay">Current: 20</span> <!-- Represented as 20 -->
        <br>
        <label for="ySpeed">Y Spd:</label>
        <input type="number" id="ySpeed" step="1" value="20"> <!-- Represented as 20 -->
        <span id="ySpeedDisplay">Current: 20</span> <!-- Represented as 20 -->
        <label for="yAccel">Y Acc:</label>
        <input type="number" id="yAccel" step="1" value="20"> <!-- Represented as 20 -->
        <span id="yAccelDisplay">Current: 20</span> <!-- Represented as 20 -->
        <button id="setSpeedButton" class="button setting-button" onclick="setSpeedAccel()">Set Speed/Accel</button>
    </div>

    <button id="pnpButton" class="button" onclick="sendCommand('ENTER_PICKPLACE')">Pick and Place Mode</button> <!-- Changed text -->
    <button id="pnpStepButton" class="button" onclick="sendCommand('PNP_NEXT_STEP')" style="display: none;">Next PnP Step (N)</button>
    <button id="pnpSkipButton" class="button" onclick="sendCommand('PNP_SKIP_LOCATION')" style="display: none;">Skip Next</button>
    <button id="pnpBackButton" class="button" onclick="sendCommand('PNP_BACK_LOCATION')" style="display: none;">Back One</button>
  </div> <!-- ADDED -->
  <hr>
  <div class="section-rotation"> <!-- ADDED Rotation Section -->
    <h2>Tray Rotation Control</h2>
    <div class="input-group">
      <h3>Rotation</h3>
      <!-- <span id="rotationAngleDisplay" style="display: block; font-size: 1.2em; margin-bottom: 10px;">Current: 0.0&deg;</span> --> <!-- REMOVED Angle Display -->
      <button id="rotateMinus90Button" class="button" onclick="rotateTray(-90)" style="width: 80px;">-90&deg;</button>
      <button id="rotatePlus90Button" class="button" onclick="rotateTray(90)" style="width: 80px;">+90&deg;</button>

      <button id="rotateMinus5Button" class="button" onclick="rotateTray(-5)" style="width: 70px; margin-top: 10px;">-5&deg;</button>
    <button id="rotatePlus5Button" class="button" onclick="rotateTray(5)" style="width: 70px; margin-top: 10px;">+5&deg;</button>

      <!-- REMOVED Trim Buttons -->
      <!-- <button id="trimMinus1Button" class="button setting-button" onclick="trimRotation(-1)" style="padding: 5px 10px !important; font-size: 14px !important;">-1&deg; Trim</button> -->
      <!-- <button id="trimPlus1Button" class="button setting-button" onclick="trimRotation(1)" style="padding: 5px 10px !important; font-size: 14px !important;">+1&deg; Trim</button> -->
      <br>
      <button id="setRotationZeroButton" class="button setting-button" onclick="setRotationZero()" style="background-color: #ff9800;">Set Zero Angle</button>
    </div>
  </div> <!-- END Rotation Section -->
  <hr>
  <div class="section-manual"> <!-- ADDED -->
    <h2>Manual Mode</h2> <!-- Changed header -->
     <div id="calibrationControls" style="display: none;">
       <div class="input-group">
          <h3>Jog Controls</h3>
          <label>Jog Step (in):</label>
          <input type="number" id="jogStep" step="0.01" value="0.1">
          <br>
          <button class="button setting-button" onclick="jogAxis('X', -1)">X-</button>
          <button class="button setting-button" onclick="jogAxis('X', 1)">X+</button>
          <br>
          <button class="button setting-button" onclick="jogAxis('Y', -1)">Y-</button>
          <button class="button setting-button" onclick="jogAxis('Y', 1)">Y+</button>
          <p style="font-size: 0.8em; margin-top: 10px;">Use arrow keys for 0.2" moves:<br>
          ← X- | → X+ | ↑ Y+ | ↓ Y-</p>
       </div>
       <div class="input-group">
          <h3>Move to Position</h3>
          <label for="gotoX">X (in):</label>
          <input type="number" id="gotoX" step="0.1" min="0">
          <br>
          <label for="gotoY">Y (in):</label>
          <input type="number" id="gotoY" step="0.1" min="0">
          <br>
          <button class="button setting-button" onclick="moveToCoords()">Go to Position</button>
       </div>
       <div class="input-group">
          <h3>Set Positions</h3>
          <button id="setOffsetCurrentButton" class="button setting-button" onclick="sendCommand('SET_OFFSET_FROM_CURRENT')">Set PnP Offset<br>From Current</button>
          <button id="setFirstPlaceAbsCurrentButton" class="button setting-button" onclick="sendCommand('SET_FIRST_PLACE_ABS_FROM_CURRENT')">Set First Place Abs<br>From Current</button> <!-- ID, text, and command updated -->
       </div>
       <div class="input-group">
          <h3>Current Position</h3>
          <span id="currentPosDisplay">X: 0.000, Y: 0.000, Z: 0.000</span>
       </div>
       <br style="clear:both;">
       <button id="exitCalibrationButton" class="button" onclick="sendCommand('EXIT_CALIBRATION')" style="background-color: #f44336;">Exit Manual Mode</button> <!-- Changed text -->
     </div>
     <button id="enterCalibrationButton" class="button" onclick="sendCommand('ENTER_CALIBRATION')">Manual Mode</button> <!-- Changed text -->
  </div> <!-- ADDED -->
   <hr style="clear:both;">

  <div class="section-paint"> <!-- ADDED -->
    <h2>Paint Control</h2>
    <div id="paintControls">
      <!-- Settings for Global Paint Offsets -->
      <div class="input-group">
          <h3>Global Paint Offsets</h3>
          <label for="paintPatternOffsetX">Pattern X:</label>
          <input type="number" id="paintPatternOffsetX" step="0.1" value="0.0">
          <label for="paintPatternOffsetY">Pattern Y:</label>
          <input type="number" id="paintPatternOffsetY" step="0.1" value="0.0">
          <span id="paintPatternOffsetDisplay">Current: X=0.00, Y=0.00</span>
          <br>
          <label for="paintGunOffsetX">Gun X:</label>
          <input type="number" id="paintGunOffsetX" step="0.1" value="0.0">
          <label for="paintGunOffsetY">Gun Y:</label>
          <input type="number" id="paintGunOffsetY" step="0.1" value="0.0">
          <span id="paintGunOffsetDisplay">Current: X=0.00, Y=0.00</span>
          <button id="setPaintOffsetsButton" class="button setting-button" onclick="setPaintOffsets()">Set Paint Offsets</button>
      </div>

      <!-- Placeholder for Side Settings Panels -->
      <div style="clear:both;"></div>
      <p>(Side settings panels will go here)</p>

      <!-- Placeholder for Start Button -->
       <button id="startPaintingButton" class="button setting-button" onclick="sendCommand('START_PAINTING')" style="background-color: #dc3545;">Start Painting</button>
       <button id="paintBackButton" class="button setting-button" onclick="sendCommand('PAINT_SIDE_0')" style="background-color: #A142F4;">Paint Back Side</button>
       <!-- Placeholder for additional paint side buttons if needed -->
       <!-- Placeholder for Stop Button -->
       <button id="stopButton" class="button setting-button" onclick="sendCommand('STOP')" style="background-color: #ffc107;">STOP</button>

              </div>
    </div> <!-- ADDED -->
    <hr>

    <button id="downloadSettingsButton" class="button setting-button" onclick="downloadSettings()" style="background-color: #007bff; float: right; margin: 10px;">Download Settings</button>
    <button id="uploadSettingsButton" class="button setting-button" onclick="document.getElementById('settingsFileInput').click()" style="background-color: #28a745; float: right; margin: 10px;">Upload Settings</button>
    <input type="file" id="settingsFileInput" style="display: none;" accept=".json" onchange="uploadSettings(this.files)">
    <div style="clear:both;"></div> <!-- Clear float -->
    <div id="status">Connecting to ESP32...</div> <span id="connectionIndicator" style="color:red; font-weight:bold;"></span>
    <p style="font-size: 0.8em; color: #aaa;">Keyboard Shortcuts: [H] Home All, [N] Next PnP Step (when active)</p> /* Lighter shortcut text */

    <script>
      var gateway = `ws://${window.location.hostname}:81/`; // Use port 81 for WebSocket
      var websocket;
      var statusDiv = document.getElementById('status');
      var connectionIndicatorSpan = document.getElementById('connectionIndicator');
      var allHomed = false; // Global state variable
      var isMoving = false; // Global state variable for general movement
      var isHoming = false; // Global state variable for homing process
      var homeButton = document.getElementById('homeButton');
      var gotoButton = document.getElementById('gotoButton');
      var gotoButton2020 = document.getElementById('gotoButton2020');
      var pnpButton = document.getElementById('pnpButton');
      var pnpStepButton = document.getElementById('pnpStepButton');
      var offsetXInput = document.getElementById('offsetX');
      var offsetYInput = document.getElementById('offsetY');
      var setOffsetButton = document.getElementById('setOffsetButton');
      var offsetDisplaySpan = document.getElementById('offsetDisplay');

      // Track machine state
      var isMoving = false;
      var isHoming = false;

      var firstPlaceXAbsInput = document.getElementById('firstPlaceXAbs'); // Updated ID
      var firstPlaceYAbsInput = document.getElementById('firstPlaceYAbs'); // Updated ID
      var setFirstPlaceAbsButton = document.getElementById('setFirstPlaceAbsButton'); // Updated ID
      var firstPlaceAbsDisplaySpan = document.getElementById('firstPlaceAbsDisplay'); // Updated ID

      // Grid/Spacing Elements
      var gridColsInput = document.getElementById('gridCols');
      var gridRowsInput = document.getElementById('gridRows');
      var setGridSpacingButton = document.getElementById('setGridSpacingButton');
      var gridDisplaySpan = document.getElementById('gridDisplay');
      var spacingDisplaySpan = document.getElementById('spacingDisplay');

      // Tray Size Elements (NEW)
      var trayWidthInput = document.getElementById('trayWidth');
      var trayHeightInput = document.getElementById('trayHeight');
      var setTraySizeButton = document.getElementById('setTraySizeButton');
      var traySizeDisplaySpan = document.getElementById('traySizeDisplay');

      var xSpeedInput = document.getElementById('xSpeed');
      var xAccelInput = document.getElementById('xAccel');
      var ySpeedInput = document.getElementById('ySpeed');
      var yAccelInput = document.getElementById('yAccel');
      var setSpeedButton = document.getElementById('setSpeedButton');
      var xSpeedDisplaySpan = document.getElementById('xSpeedDisplay');
      var xAccelDisplaySpan = document.getElementById('xAccelDisplay');
      var ySpeedDisplaySpan = document.getElementById('ySpeedDisplay');
      var yAccelDisplaySpan = document.getElementById('yAccelDisplay');

      // Calibration Elements
      var enterCalibrationButton = document.getElementById('enterCalibrationButton');
      var exitCalibrationButton = document.getElementById('exitCalibrationButton');
      var calibrationControlsDiv = document.getElementById('calibrationControls');
      var jogStepInput = document.getElementById('jogStep');
      var currentPosDisplaySpan = document.getElementById('currentPosDisplay');
      var setOffsetCurrentButton = document.getElementById('setOffsetCurrentButton');
      var setFirstPlaceAbsCurrentButton = document.getElementById('setFirstPlaceAbsCurrentButton'); // Updated ID

      // Paint Offset Elements
      var paintPatternOffsetXInput = document.getElementById('paintPatternOffsetX');
      var paintPatternOffsetYInput = document.getElementById('paintPatternOffsetY');
      var paintGunOffsetXInput = document.getElementById('paintGunOffsetX');
      var paintGunOffsetYInput = document.getElementById('paintGunOffsetY');
      var setPaintOffsetsButton = document.getElementById('setPaintOffsetsButton');
      var paintPatternOffsetDisplaySpan = document.getElementById('paintPatternOffsetDisplay');
      var paintGunOffsetDisplaySpan = document.getElementById('paintGunOffsetDisplay');

      // Paint Side Settings Elements (Arrays)
      var paintZInputs = [];
      var paintPInputs = [];
      var paintRInputs = [];
      var paintSInputs = [];
      var paintZDisplays = [];
      var paintPDisplays = [];
      var paintRDisplays = [];
      var paintSDisplays = [];
      var setPaintSideButtons = [];
      for (let i = 0; i < 4; i++) {
          paintZInputs[i] = document.getElementById(`paintZ_${i}`);
          paintPInputs[i] = document.getElementById(`paintP_${i}`);
          paintRInputs[i] = document.getElementById(`paintR_${i}`);
          paintSInputs[i] = document.getElementById(`paintS_${i}`);
          paintZDisplays[i] = document.getElementById(`paintZDisplay_${i}`);
          paintPDisplays[i] = document.getElementById(`paintPDisplay_${i}`);
          paintRDisplays[i] = document.getElementById(`paintRDisplay_${i}`);
          paintSDisplays[i] = document.getElementById(`paintSDisplay_${i}`);
          // Assuming button IDs follow a pattern like setPaintSideButton_0 (adjust if needed)
          // setPaintSideButtons[i] = document.getElementById(`setPaintSideButton_${i}`);
          // Simpler: get button references from within the setPaintSideSettings function based on index
      }

      // Paint Control Elements
      var startPaintingButton = document.getElementById('startPaintingButton');
      var paintBackButton = document.getElementById('paintBackButton');
      var stopButton = document.getElementById('stopButton');
      var calibrateButton = document.getElementById('calibrateButton');

      // Rotation Control Elements (NEW)
      var rotateMinus90Button = document.getElementById('rotateMinus90Button');
      var rotatePlus90Button = document.getElementById('rotatePlus90Button');
      var rotateMinus5Button = document.getElementById('rotateMinus5Button');
      var rotatePlus5Button = document.getElementById('rotatePlus5Button');
      var setRotationZeroButton = document.getElementById('setRotationZeroButton');

      // Pick and Place Control Elements
      var pnpSkipButton = document.getElementById('pnpSkipButton');
      var pnpBackButton = document.getElementById('pnpBackButton');

      function initWebSocket() {
        console.log('Trying to open a WebSocket connection...');
        statusDiv.innerHTML = 'Connecting...';
        statusDiv.style.color = 'orange';
        websocket = new WebSocket(gateway);
        websocket.onopen    = onOpen;
        websocket.onclose   = onClose;
        websocket.onmessage = onMessage;
        websocket.onerror   = onError;
      }

      function onOpen(event) {
        console.log('Connection opened');
        statusDiv.innerHTML = 'Connected';
        statusDiv.style.color = 'green';
        connectionIndicatorSpan.innerHTML = ''; // Clear indicator on connect
        enableButtons(true, false); // Enable buttons on connect, not in PnP mode
      }

      function onClose(event) {
        console.log('Connection closed');
        statusDiv.innerHTML = 'Disconnected - Retrying...';
        statusDiv.style.color = 'red';
        connectionIndicatorSpan.innerHTML = ' (Offline)'; // Show indicator on close
         enableButtons(false, false); // Disable buttons on disconnect, not in PnP mode
         pnpButton.innerHTML = "Enter Pick/Place"; // Updated text
         pnpStepButton.style.display = 'none'; // Hide PnP step button
         enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button
         calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
        setTimeout(initWebSocket, 2000); // Try to reconnect every 2 seconds
      }

      function onMessage(event) {
          console.log('Received: ', event.data);
          try {
              const data = JSON.parse(event.data);
              statusDiv.innerHTML = `Status: ${data.status} - ${data.message}`;

              // --- Add JS Debugging ---
              console.log(`JS Debug: Received status='${data.status}'`);
              console.log(`JS Debug: Before enableButtons: isMoving=${isMoving}, isHoming=${isHoming}`);
              // --- End JS Debugging ---

              // Update JavaScript state variables
              isMoving = (data.status === "Busy" || data.status === "Moving" || data.status === "Homing");
              isHoming = (data.status === "Homing");

              // Update offset display and inputs if offset info is present
              if (data.hasOwnProperty('pnpOffsetX') && data.hasOwnProperty('pnpOffsetY')) {
                  let currentX = parseFloat(data.pnpOffsetX).toFixed(1); // Changed to 1 decimal place
                  let currentY = parseFloat(data.pnpOffsetY).toFixed(1); // Changed to 1 decimal place
                  offsetDisplaySpan.innerHTML = `Current: X=${currentX}, Y=${currentY}`;
                  offsetXInput.value = currentX;
                  offsetYInput.value = currentY;
              }

              // Update first place display and inputs if info is present
              if (data.hasOwnProperty('placeFirstXAbs') && data.hasOwnProperty('placeFirstYAbs')) { // Updated keys
                  let currentX = parseFloat(data.placeFirstXAbs).toFixed(1); // Changed to 1 decimal place
                  let currentY = parseFloat(data.placeFirstYAbs).toFixed(1); // Changed to 1 decimal place
                  firstPlaceAbsDisplaySpan.innerHTML = `Current: X=${currentX}, Y=${currentY}`; // Updated span ID
                  firstPlaceXAbsInput.value = currentX; // Updated input ID
                  firstPlaceYAbsInput.value = currentY; // Updated input ID
              }

              // Update speed/accel display and inputs if info is present
              if (data.hasOwnProperty('patXSpeed') && data.hasOwnProperty('patXAccel') &&
                  data.hasOwnProperty('patYSpeed') && data.hasOwnProperty('patYAccel')) {
                  // Divide received actual values (e.g., 20000) by 1000 for display (e.g., 20)
                  let xS_display = parseInt(data.patXSpeed) / 1000;
                  let xA_display = parseInt(data.patXAccel) / 1000;
                  let yS_display = parseInt(data.patYSpeed) / 1000;
                  let yA_display = parseInt(data.patYAccel) / 1000;
                  xSpeedDisplaySpan.innerHTML = `Current: ${xS_display}`;
                  xAccelDisplaySpan.innerHTML = `Current: ${xA_display}`;
                  ySpeedDisplaySpan.innerHTML = `Current: ${yS_display}`;
                  yAccelDisplaySpan.innerHTML = `Current: ${yA_display}`;
                  xSpeedInput.value = xS_display;
                  xAccelInput.value = xA_display;
                  ySpeedInput.value = yS_display;
                  yAccelInput.value = yA_display;
              }

              // Update Grid/Spacing display and inputs if info is present
              if (data.hasOwnProperty('gridCols') && data.hasOwnProperty('gridRows') &&
                  data.hasOwnProperty('gapX') && data.hasOwnProperty('gapY')) {
                  let currentCols = parseInt(data.gridCols);
                  let currentRows = parseInt(data.gridRows);
                  let currentGapX = parseFloat(data.gapX).toFixed(3); // Use gapX, maybe more precision?
                  let currentGapY = parseFloat(data.gapY).toFixed(3); // Use gapY, maybe more precision?
                  gridDisplaySpan.innerHTML = `Current: ${currentCols} x ${currentRows}`;
                  // Update display text to show Gap
                  spacingDisplaySpan.innerHTML = `Gap: X=${currentGapX}, Y=${currentGapY}`;
                  gridColsInput.value = currentCols;
                  gridRowsInput.value = currentRows;
                  // Note: We don't update input fields for gapX/gapY as they are calculated, not set directly.
              }

              // Update Tray Size display and inputs if info is present (NEW)
              if (data.hasOwnProperty('trayWidth') && data.hasOwnProperty('trayHeight')) {
                  let currentW = parseFloat(data.trayWidth).toFixed(2);
                  let currentH = parseFloat(data.trayHeight).toFixed(2);
                  traySizeDisplaySpan.innerHTML = `Current: W=${currentW}, H=${currentH}`;
                  trayWidthInput.value = currentW;
                  trayHeightInput.value = currentH;
              }

              // Update Paint Offsets display and inputs if info is present
              if (data.hasOwnProperty('paintPatOffX') && data.hasOwnProperty('paintPatOffY')) {
                  let currentX = parseFloat(data.paintPatOffX).toFixed(2);
                  let currentY = parseFloat(data.paintPatOffY).toFixed(2);
                  paintPatternOffsetDisplaySpan.innerHTML = `Current: X=${currentX}, Y=${currentY}`;
                  paintPatternOffsetXInput.value = currentX;
                  paintPatternOffsetYInput.value = currentY;
              }
              if (data.hasOwnProperty('paintGunOffX') && data.hasOwnProperty('paintGunOffY')) {
                  let currentX = parseFloat(data.paintGunOffX).toFixed(2);
                  let currentY = parseFloat(data.paintGunOffY).toFixed(2);
                  paintGunOffsetDisplaySpan.innerHTML = `Current: X=${currentX}, Y=${currentY}`;
                  paintGunOffsetXInput.value = currentX;
                  paintGunOffsetYInput.value = currentY;
              }

              // Update Paint Side Settings display and inputs
              for (let i = 0; i < 4; i++) {
                  let keyZ = `paintZ_${i}`;
                  let keyP = `paintP_${i}`;
                  let keyR = `paintR_${i}`;
                  let keyS = `paintS_${i}`;
                  if (data.hasOwnProperty(keyZ) && paintZInputs[i] && paintZDisplays[i]) {
                      let currentZ = parseFloat(data[keyZ]).toFixed(2);
                      paintZInputs[i].value = currentZ;
                      paintZDisplays[i].innerHTML = `Current: ${currentZ}`;
                  }
                   if (data.hasOwnProperty(keyP) && paintPInputs[i] && paintPDisplays[i]) {
                      let currentP = parseInt(data[keyP]);
                      paintPInputs[i].value = currentP;
                      paintPDisplays[i].innerHTML = `${currentP}°`;
                  }
                   if (data.hasOwnProperty(keyR) && paintRInputs[i] && paintRDisplays[i]) {
                      let currentR = parseInt(data[keyR]);
                      paintRInputs[i].value = currentR;
                      paintRDisplays[i].innerHTML = `${currentR}°`;
                  }
                  if (data.hasOwnProperty(keyS) && paintSInputs[i] && paintSDisplays[i]) {
                      let currentS = parseInt(data[keyS]);
                      paintSInputs[i].value = currentS;
                      paintSDisplays[i].innerHTML = `${currentS}`;
                  }
              }

              // Update current position display if it's a position update message
              if (data.type === "positionUpdate") {
                  currentPosDisplaySpan.innerHTML = `X: ${parseFloat(data.posX).toFixed(3)}, Y: ${parseFloat(data.posY).toFixed(3)}, Z: ${parseFloat(data.posZ).toFixed(3)}`;
              }

              if (data.status === "Ready" || data.status === "Error") {
                  console.log("JS Debug: Executing 'Ready' or 'Error' block"); // JS Debug
                  statusDiv.style.color = (data.status === "Ready") ? 'green' : 'red';
                  allHomed = (data.status === "Ready"); // <<< SET allHomed based on Ready status
                  enableButtons(true, false);
                  pnpButton.innerHTML = "Enter Pick/Place"; // Reset button text
                  pnpButton.className = "button"; // Reset to default button style
                  pnpButton.onclick = function() { sendCommand('ENTER_PICKPLACE'); }; // Reset function
                  pnpStepButton.style.display = 'none'; // Hide PnP step button
                  enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
              } else if (data.status === "PickPlaceReady") {
                  console.log("JS Debug: Executing 'PickPlaceReady' block"); // JS Debug
                  statusDiv.style.color = 'blue'; // Indicate special mode
                  enableButtons(true, true); // PnP ready state (enable=true, enablePnP=true)
                  // Change PnP button to Exit button with red color
                  pnpButton.innerHTML = "Exit Pick and Place Mode";
                  pnpButton.className = "button exit-button"; // Add red styling
                  pnpButton.disabled = false;  
                  pnpButton.onclick = function() { sendCommand('EXIT_PICKPLACE'); }; // Change function to exit
                  pnpStepButton.style.display = 'inline-block'; // Show PnP step button
                  enterCalibrationButton.style.display = 'none'; // Hide Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
                  pnpSkipButton.style.display = 'inline-block'; // Show Skip button
                  pnpBackButton.style.display = 'inline-block'; // Show Back button
                  enableButtons(); // Ensure buttons are enabled correctly
              } else if (data.status === "Busy" || data.status === "Moving" || data.status === "Homing") {
                  console.log("JS Debug: Executing 'Busy/Moving/Homing' block"); // JS Debug
                  statusDiv.style.color = 'orange';
                  enableButtons(false, false); // Busy state (applies to normal, PnP, and Cal)
                  // Keep PnP step button visible if already in PnP mode, even if moving
                  pnpStepButton.style.display = (pnpButton.onclick.toString().includes('EXIT_PICKPLACE')) ? 'inline-block' : 'none';
                  // Hide Calibration controls if Homing starts (treat like Ready/Error)
                   calibrationControlsDiv.style.display = 'none'; // Hide Cal controls on Homing
                   enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button on Homing
              } else if (data.status === "CalibrationActive") {
                  console.log("JS Debug: Executing 'CalibrationActive' block"); // JS Debug
                  statusDiv.style.color = 'purple'; // Indicate calibration mode
                  allHomed = true; // Assume homed when entering calibration
                  isMoving = false; // Assume not moving initially
                  isHoming = false;
                  enableButtons(); // Update buttons for calibration mode
                  enterCalibrationButton.style.display = 'none'; // Hide Enter Cal button
                  calibrationControlsDiv.style.display = 'block'; // Show Cal controls
                  pnpButton.innerHTML = "Enter Pick/Place"; // Ensure PnP button is reset
                  pnpButton.className = "button"; // Reset to default button style
                  pnpButton.onclick = function() { sendCommand('ENTER_PICKPLACE'); };
                  pnpStepButton.style.display = 'none'; // Hide PnP step button
              } else if (data.status === "PickPlaceComplete") {
                  console.log("JS Debug: Executing 'PickPlaceComplete' block"); // JS Debug
                  statusDiv.style.color = 'green';
                  allHomed = true; // Still homed
                  isMoving = false;
                  isHoming = false;
                  enableButtons(); // Update buttons post-PnP
                  pnpButton.innerHTML = "Sequence Complete"; // Update PnP button text
                  pnpButton.disabled = true; // Disable PnP button as sequence is done
                  pnpStepButton.style.display = 'none'; // Ensure step button is hidden
                  enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
              } else if (command == "START_PAINTING") {
                      Serial.println("WebSocket: Received START_PAINTING command.");
                      // startPaintingSequence(); // Call the (placeholder) function // COMMENTED OUT FOR NOW
                   }
          } catch (e) {
              statusDiv.innerHTML = `Received raw: ${event.data}`; // Show raw data if JSON parse fails
              statusDiv.style.color = 'blue';
          }
      }

       function onError(event) {
          console.error("WebSocket Error: ", event);
          statusDiv.innerHTML = 'WebSocket Error';
          statusDiv.style.color = 'red';
          connectionIndicatorSpan.innerHTML = ' (Error)'; // Show indicator on error
          enableButtons(false, false);
          enterCalibrationButton.style.display = 'inline-block'; // Ensure Enter Cal button is visible on error/disconnect
          calibrationControlsDiv.style.display = 'none'; // Ensure Cal controls are hidden on error/disconnect
       }

      function sendCommand(command) {
        console.log('Sending: ' + command);
        websocket.send(command);
        // Optionally disable buttons immediately after sending
        enableButtons(false, false); // Disable buttons immediately on command send
        statusDiv.innerHTML = 'Command Sent: ' + command;
        statusDiv.style.color = 'blue';
      }

      function enableButtons() {
          console.log(`enableButtons called: connected=${websocket && websocket.readyState === WebSocket.OPEN}, allHomed=${allHomed}, isMoving=${isMoving}, isHoming=${isHoming}, inPnPMode=${pnpButton.innerHTML.includes('Mode') || pnpButton.innerHTML.includes('Complete')}, inCalibMode=${calibrationControlsDiv.style.display === 'block'}`);

          let connected = websocket && websocket.readyState === WebSocket.OPEN;
          let inPnPMode = pnpButton.innerHTML.includes('Mode') || pnpButton.innerHTML.includes('Complete');
          let inCalibMode = calibrationControlsDiv.style.display === 'block';
          let canStop = connected && (isMoving || isHoming || inPnPMode || inCalibMode); // Can stop if doing anything interruptible

          homeButton.disabled = !connected || isMoving || isHoming || inPnPMode || inCalibMode;
          stopButton.disabled = !canStop;

          // PnP Button Logic - UPDATED
          // Disable if not connected, not homed, moving, homing, in calibration, or sequence complete
          pnpButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode || pnpButton.innerHTML.includes('Complete');

          // PnP Step Button Logic (Only enabled in PnPReady state)
          pnpStepButton.disabled = !(connected && pnpButton.innerHTML.includes('Mode') && !isMoving && !isHoming);
          pnpSkipButton.disabled = pnpStepButton.disabled; // Skip/Back follow Step button logic
          pnpBackButton.disabled = pnpStepButton.disabled; // Skip/Back follow Step button logic

          // Calibration Button Logic
          enterCalibrationButton.disabled = !connected || !allHomed || isMoving || isHoming || inPnPMode || inCalibMode;

          // --- Painting Buttons ---
          // Disable painting if not connected, not homed, moving, homing, in PnP, or calibrating
          startPaintingButton.disabled = !connected || !allHomed || isMoving || isHoming || inPnPMode || inCalibMode; // Requires homing
          paintBackButton.disabled = startPaintingButton.disabled; // Same logic as start

          // Rotation Buttons
          rotateMinus90Button.disabled = !connected || isMoving || isHoming || inPnPMode || inCalibMode; // Allow rotation before homing
          rotatePlus90Button.disabled = rotateMinus90Button.disabled;
          rotateMinus5Button.disabled = rotateMinus90Button.disabled;
          rotatePlus5Button.disabled = rotateMinus90Button.disabled;
          setRotationZeroButton.disabled = rotateMinus90Button.disabled; // Allow setting zero before homing

          // --- Settings Buttons/Inputs ---
          // Generally disable settings inputs/buttons if disconnected, moving, homing, in PnP, or calibrating
          // Special exceptions for Grid/Tray buttons
          let generalDisableCondition = !connected || isMoving || isHoming || inPnPMode || inCalibMode;

          // *** Keep Grid and Tray Size buttons enabled if connected ***
          setGridSpacingButton.disabled = !connected;
          setTraySizeButton.disabled = !connected;
          // *** End Special Exception ***

          setOffsetButton.disabled = generalDisableCondition || !allHomed; // Requires homing
          setFirstPlaceAbsButton.disabled = generalDisableCondition || !allHomed; // Requires homing
          setSpeedButton.disabled = generalDisableCondition || !allHomed; // Requires homing
          setPaintOffsetsButton.disabled = generalDisableCondition || !allHomed; // Requires homing

          // Disable input fields based on the same general condition + homing requirement for most
          offsetXInput.disabled = setOffsetButton.disabled;
          offsetYInput.disabled = setOffsetButton.disabled;
          firstPlaceXAbsInput.disabled = setFirstPlaceAbsButton.disabled;
          firstPlaceYAbsInput.disabled = setFirstPlaceAbsButton.disabled;
          xSpeedInput.disabled = setSpeedButton.disabled;
          xAccelInput.disabled = setSpeedButton.disabled;
          ySpeedInput.disabled = setSpeedButton.disabled;
          yAccelInput.disabled = setSpeedButton.disabled;
          paintPatternOffsetXInput.disabled = setPaintOffsetsButton.disabled;
          paintPatternOffsetYInput.disabled = setPaintOffsetsButton.disabled;
          paintGunOffsetXInput.disabled = setPaintOffsetsButton.disabled;
          paintGunOffsetYInput.disabled = setPaintOffsetsButton.disabled;

          // Grid/Tray inputs are linked to their buttons (only disabled if disconnected)
          gridColsInput.disabled = setGridSpacingButton.disabled;
          gridRowsInput.disabled = setGridSpacingButton.disabled;
          trayWidthInput.disabled = setTraySizeButton.disabled;
          trayHeightInput.disabled = setTraySizeButton.disabled;

          // Calibration Controls (only enabled when in calibration mode and not moving/homing)
          let calibControlsDisable = !connected || !inCalibMode || isMoving || isHoming;
          let calibInputs = calibrationControlsDiv.querySelectorAll('input, button');
          calibInputs.forEach(el => {
              if (el.id !== 'exitCalibrationButton') { // Exit button handled separately
                el.disabled = calibControlsDisable;
              }
          });
          exitCalibrationButton.disabled = !connected || !inCalibMode; // Can exit calibration even if moving

          // Download/Upload Buttons (only need connection)
          document.getElementById('downloadSettingsButton').disabled = !connected;
          document.getElementById('uploadSettingsButton').disabled = !connected;

          // Specific overrides based on state machine logic elsewhere (e.g., PnPComplete disabling pnpButton)
          if (pnpButton.innerHTML.includes('Complete')) {
               pnpButton.disabled = true;
          }
          if (pnpButton.innerHTML.includes('Mode')) { // Ensure PnP Step button visibility is correct
               pnpStepButton.style.display = 'inline-block';
               pnpSkipButton.style.display = 'inline-block'; // Show Skip/Back in PnP Mode
               pnpBackButton.style.display = 'inline-block';
          } else {
               pnpStepButton.style.display = 'none';
               pnpSkipButton.style.display = 'none'; // Hide Skip/Back outside PnP Mode
               pnpBackButton.style.display = 'none';
          }
      }

      window.addEventListener('load', initWebSocket);
      // Add keyboard listener
      document.addEventListener('keydown', function(event) {
          // Ignore keyboard input if focus is on an input field
          if (document.activeElement.tagName === 'INPUT') return;

          if (event.key === 'n' || event.key === 'N') {
              // Check if the PnP Cycle button is visible and enabled before sending
              if (pnpStepButton.style.display !== 'none' && !pnpStepButton.disabled) {
                  sendCommand('PNP_NEXT_STEP');
              }
          } else if (event.key === 'h' || event.key === 'H') {
              // Check if the Home button is enabled before sending
              if (!homeButton.disabled) {
                  sendCommand('HOME');
              }
          } else if (event.key === '1') {
              // Check if the Go to (5,5,0) button is enabled
              if (!gotoButton.disabled) {
                  sendCommand('GOTO_5_5_0');
              }
          } else if (event.key === '2') {
              // Check if the Go to (20,20,0) button is enabled
              if (!gotoButton2020.disabled) {
                  sendCommand('GOTO_20_20_0');
              }
          } else if (calibrationControlsDiv.style.display === 'block') {
              // Arrow key controls for calibration mode - fixed 0.2" increments
              const fixedJogStep = 0.2; // Always move in 0.2" increments with arrow keys
              
              switch (event.key) {
                  case 'ArrowLeft':
                      // Jog X-
                      if (!isMoving && !isHoming) jogAxis('X', -1, fixedJogStep);
                      event.preventDefault(); // Prevent page scrolling
                      break;
                  case 'ArrowRight':
                      // Jog X+
                      if (!isMoving && !isHoming) jogAxis('X', 1, fixedJogStep);
                      event.preventDefault();
                      break;
                  case 'ArrowUp':
                      // Jog Y+
                      if (!isMoving && !isHoming) jogAxis('Y', 1, fixedJogStep);
                      event.preventDefault();
                      break;
                  case 'ArrowDown':
                      // Jog Y-
                      if (!isMoving && !isHoming) jogAxis('Y', -1, fixedJogStep);
                      event.preventDefault();
                      break;
              }
          }
      });

      function setOffset() {
          const xVal = offsetXInput.value;
          const yVal = offsetYInput.value;
          // Basic validation (ensure they are numbers)
          if (isNaN(parseFloat(xVal)) || isNaN(parseFloat(yVal))) {
              alert("Invalid offset values. Please enter numbers.");
              return;
          }
          const command = `SET_PNP_OFFSET ${xVal} ${yVal}`;
          sendCommand(command);
      }

      function setFirstPlaceAbs() { // Renamed function
          console.log("JS: setFirstPlaceAbs() called"); // DEBUG
          const xVal = firstPlaceXAbsInput.value; // Updated ID
          const yVal = firstPlaceYAbsInput.value; // Updated ID
          if (isNaN(parseFloat(xVal)) || isNaN(parseFloat(yVal))) {
              alert("Invalid first place values. Please enter numbers.");
              return;
          }
          const command = `SET_FIRST_PLACE_ABS ${xVal} ${yVal}`;
          sendCommand(command);
      }

      function setGridSpacing() {
          const colsVal = gridColsInput.value;
          const rowsVal = gridRowsInput.value;
          // Removed sxVal, syVal
          // Basic validation for cols/rows only
          if (isNaN(parseInt(colsVal)) || parseInt(colsVal) <= 0 ||
              isNaN(parseInt(rowsVal)) || parseInt(rowsVal) <= 0) {
              alert("Invalid grid columns/rows. Must be positive integers.");
              return;
          }
          // Send only cols and rows
          const command = `SET_GRID_SPACING ${colsVal} ${rowsVal}`;
          sendCommand(command);
      }

      function setSpeedAccel() {
          // Read displayed values (e.g., 20)
          const xS_display = xSpeedInput.value;
          const xA_display = xAccelInput.value;
          const yS_display = ySpeedInput.value;
          const yA_display = yAccelInput.value;
          // Basic validation
          if (isNaN(parseFloat(xS_display)) || isNaN(parseFloat(xA_display)) || isNaN(parseFloat(yS_display)) || isNaN(parseFloat(yA_display))) {
              alert("Invalid speed/accel values. Please enter numbers.");
              return;
          }
          // Convert displayed values (e.g., 20) back to actual values (e.g., 20000) for the command
          const xS_actual = parseFloat(xS_display) * 1000;
          const xA_actual = parseFloat(xA_display) * 1000;
          const yS_actual = parseFloat(yS_display) * 1000;
          const yA_actual = parseFloat(yA_display) * 1000;

          if (xS_actual <= 0 || xA_actual <= 0 || yS_actual <= 0 || yA_actual <= 0) {
              alert("Speed/accel values must be positive.");
              return;
          }
          // Send command with the actual values
          const command = `SET_SPEED_ACCEL ${xS_actual} ${xA_actual} ${yS_actual} ${yA_actual}`;
          sendCommand(command);

          // Save to Preferences
          preferences.begin("machineCfg", false); // Namespace
          preferences.putFloat("patXSpd", xS_actual); // Save X Speed
          preferences.putFloat("patXAcc", xA_actual); // Save X Accel
          preferences.putFloat("patYSpd", yS_actual); // Save Y Speed
          preferences.putFloat("patYAcc", yA_actual); // Save Y Accel
          preferences.end();
          Serial.printf("[DEBUG] Saved Speed/Accel to NVS: X(S:%.0f, A:%.0f), Y(S:%.0f, A:%.0f)\n", xS_actual, xA_actual, yS_actual, yA_actual);
      }

      function jogAxis(axis, direction, fixedStep) {
          // If fixedStep is provided, use it (for arrow keys), otherwise use the jogStep input
          const stepVal = fixedStep || parseFloat(jogStepInput.value);
          if (isNaN(stepVal) || stepVal <= 0) {
              alert("Invalid jog step value. Must be a positive number.");
              return;
          }

          // Update UI state for responsiveness
          isMoving = true;

          const distance = stepVal * direction;
          const command = `JOG ${axis} ${distance.toFixed(3)}`; // Use 3 decimal places for precision
          sendCommand(command);
      }

      function moveToCoords() {
          const xVal = document.getElementById('gotoX').value;
          const yVal = document.getElementById('gotoY').value;
          
          // Basic validation
          if (isNaN(parseFloat(xVal)) || isNaN(parseFloat(yVal))) {
              alert("Invalid coordinates. Please enter numbers.");
              return;
          }
          
          if (parseFloat(xVal) < 0 || parseFloat(yVal) < 0) {
              alert("Coordinates must be non-negative.");
              return;
          }

          // Update UI state for responsiveness
          isMoving = true;
          
          const command = `MOVE_TO_COORDS ${xVal} ${yVal}`;
          sendCommand(command);
      }

      function setPaintOffsets() {
          const patX = paintPatternOffsetXInput.value;
          const patY = paintPatternOffsetYInput.value;
          const gunX = paintGunOffsetXInput.value;
          const gunY = paintGunOffsetYInput.value;
          // Basic validation
          if (isNaN(parseFloat(patX)) || isNaN(parseFloat(patY)) || isNaN(parseFloat(gunX)) || isNaN(parseFloat(gunY)) ) {
              alert("Invalid paint offset values. Please enter numbers.");
              return;
          }
          // Send both in separate commands for simplicity on ESP32 side
          sendCommand(`SET_PAINT_PATTERN_OFFSET ${patX} ${patY}`);
          // Add a small delay or wait for confirmation before sending the next?
          // For now, send immediately after.
          sendCommand(`SET_PAINT_GUN_OFFSET ${gunX} ${gunY}`);
      }

      function setPaintSideSettings(sideIndex) {
          if (sideIndex < 0 || sideIndex > 3) return; // Invalid index

          const zVal = paintZInputs[sideIndex].value;
          const pVal = paintPInputs[sideIndex].value;
          const rVal = paintRInputs[sideIndex].value;
          const sVal = paintSInputs[sideIndex].value;

          // Basic validation
          if (isNaN(parseFloat(zVal)) || isNaN(parseInt(pVal)) || isNaN(parseInt(rVal)) || isNaN(parseFloat(sVal))) {
               alert(`Invalid settings for side ${sideIndex}. Please enter numbers.`);
              return;
          }

          // Further validation (already constrained on ESP side, but good practice here too)
          // Pitch:
          const minP = parseInt(paintPInputs[sideIndex].min);
          const maxP = parseInt(paintPInputs[sideIndex].max);
          if (parseInt(pVal) < minP || parseInt(pVal) > maxP) {
              alert(`Pitch Angle for side ${sideIndex} must be between ${minP} and ${maxP}.`);
              return;
          }
          // Roll:
          const minR = parseInt(paintRInputs[sideIndex].min);
          const maxR = parseInt(paintRInputs[sideIndex].max);
           if (parseInt(rVal) < minR || parseInt(rVal) > maxR) {
              alert(`Roll Angle for side ${sideIndex} must be between ${minR} and ${maxR}.`);
              return;
          }
           // Speed:
          const minS = parseInt(paintSInputs[sideIndex].min);
          const maxS = parseInt(paintSInputs[sideIndex].max);
          if (parseInt(sVal) < minS || parseInt(sVal) > maxS) {
              alert(`Paint Speed for side ${sideIndex} must be between ${minS} and ${maxS}.`);
              return;
          }

          const command = `SET_PAINT_SIDE_SETTINGS ${sideIndex} ${zVal} ${pVal} ${rVal} ${sVal}`;
          sendCommand(command);
      }

      // Helper to update slider value display
      function updateSliderDisplay(sliderId) {
          const slider = document.getElementById(sliderId);
          const displaySpanId = sliderId.replace(/_(\d)$/, 'Display_$1').replace(/([PR])/, '$1Display'); // More robust replace
          const displaySpan = document.getElementById(displaySpanId);

          if (slider && displaySpan) {
              let value = slider.value;
              let suffix = sliderId.includes('paintS_') ? '' : '°'; // Add degree symbol for angles
              displaySpan.innerHTML = value + suffix;
          }
      }

      // Function to download settings as a JSON file
      function downloadSettings() {
          // Create an object to hold all the settings
          const settings = {
              pnpOffset: {
                  x: parseFloat(offsetXInput.value) || 0,
                  y: parseFloat(offsetYInput.value) || 0
              },
              placeFirstAbs: {
                  x: parseFloat(firstPlaceXAbsInput.value) || 0,
                  y: parseFloat(firstPlaceYAbsInput.value) || 0
              },
              patternSpeed: {
                  xSpeed: parseFloat(xSpeedInput.value) || 0,
                  xAccel: parseFloat(xAccelInput.value) || 0,
                  ySpeed: parseFloat(ySpeedInput.value) || 0,
                  yAccel: parseFloat(yAccelInput.value) || 0
              },
              grid: {
                  cols: parseInt(gridColsInput.value) || 0,
                  rows: parseInt(gridRowsInput.value) || 0
              },
              traySize: { // NEW: Added Tray Size to download
                  width: parseFloat(trayWidthInput.value) || 0,
                  height: parseFloat(trayHeightInput.value) || 0
              },
              paintOffsets: {
                  patternX: parseFloat(paintPatternOffsetXInput.value) || 0,
                  patternY: parseFloat(paintPatternOffsetYInput.value) || 0,
                  gunX: parseFloat(paintGunOffsetXInput.value) || 0,
                  gunY: parseFloat(paintGunOffsetYInput.value) || 0
              },
              paintSides: []
          };

          // Add paint side settings
          for (let i = 0; i < 4; i++) {
              settings.paintSides.push({
                  z: parseFloat(paintZInputs[i].value) || 0,
                  pitch: parseInt(paintPInputs[i].value) || 0,
                  roll: parseInt(paintRInputs[i].value) || 0,
                  speed: parseFloat(paintSInputs[i].value) || 0
              });
          }

          // Convert to JSON string
          const settingsJson = JSON.stringify(settings, null, 2);
          
          // Create a Blob with the JSON data
          const blob = new Blob([settingsJson], { type: 'application/json' });
          
          // Create a temporary link element
          const a = document.createElement('a');
          a.href = URL.createObjectURL(blob);
          
          // Get current date and time for filename
          const date = new Date();
          const dateStr = date.toISOString().replace(/[:.]/g, '-').slice(0, 19);
          
          // Set download filename with date
          a.download = `paint-machine-settings-${dateStr}.json`;
          
          // Append to body, click to trigger download, then remove
          document.body.appendChild(a);
          a.click();
          
          // Clean up
          window.URL.revokeObjectURL(a.href);
          document.body.removeChild(a);
          
          console.log("Settings downloaded successfully");
      }

      // Function to upload settings from a JSON file
      function uploadSettings(files) {
          if (files.length === 0) return;
          
          const file = files[0];
          const reader = new FileReader();
          
          reader.onload = function(event) {
              try {
                  const settings = JSON.parse(event.target.result);
                  
                  // Apply settings to the UI
                  if (settings.pnpOffset) {
                      offsetXInput.value = settings.pnpOffset.x;
                      offsetYInput.value = settings.pnpOffset.y;
                  }
                  
                  if (settings.placeFirstAbs) {
                      firstPlaceXAbsInput.value = settings.placeFirstAbs.x;
                      firstPlaceYAbsInput.value = settings.placeFirstAbs.y;
                  }
                  
                  if (settings.patternSpeed) {
                      xSpeedInput.value = settings.patternSpeed.xSpeed;
                      xAccelInput.value = settings.patternSpeed.xAccel;
                      ySpeedInput.value = settings.patternSpeed.ySpeed;
                      yAccelInput.value = settings.patternSpeed.yAccel;
                  }
                  
                  if (settings.grid) {
                      gridColsInput.value = settings.grid.cols;
                      gridRowsInput.value = settings.grid.rows;
                  }
                  
                  if (settings.traySize) {
                      trayWidthInput.value = settings.traySize.width;
                      trayHeightInput.value = settings.traySize.height;
                  }
                  
                  if (settings.paintOffsets) {
                      paintPatternOffsetXInput.value = settings.paintOffsets.patternX;
                      paintPatternOffsetYInput.value = settings.paintOffsets.patternY;
                      paintGunOffsetXInput.value = settings.paintOffsets.gunX;
                      paintGunOffsetYInput.value = settings.paintOffsets.gunY;
                  }
                  
                  if (settings.paintSides && settings.paintSides.length === 4) {
                      for (let i = 0; i < 4; i++) {
                          if (settings.paintSides[i]) {
                              paintZInputs[i].value = settings.paintSides[i].z;
                              paintPInputs[i].value = settings.paintSides[i].pitch;
                              paintRInputs[i].value = settings.paintSides[i].roll;
                              paintSInputs[i].value = settings.paintSides[i].speed;
                              
                              // Update slider displays if applicable
                              updateSliderDisplay('paintP_' + i);
                              updateSliderDisplay('paintR_' + i);
                              updateSliderDisplay('paintS_' + i);
                          }
                      }
                  }
                  
                  // Reset the file input
                  document.getElementById('settingsFileInput').value = '';
                  
                  // Show confirmation
                  alert('Settings loaded successfully! Click Apply buttons to send to machine.');
                  console.log('Settings loaded from file');
              } catch (error) {
                  console.error('Error parsing settings file:', error);
                  alert('Error loading settings: ' + error.message);
              }
          };
          
          reader.readAsText(file);
      }

      // NEW Rotation Control Functions
      function rotateTray(degrees) {
          const command = `ROTATE ${degrees}`;
          sendCommand(command);
      }

      function setRotationZero() {
          const command = `SET_ROT_ZERO`;
          sendCommand(command);
      }

      // NEW Function to set Tray Size
      function setTraySize() {
          const widthVal = trayWidthInput.value;
          const heightVal = trayHeightInput.value;
          // Basic validation
          if (isNaN(parseFloat(widthVal)) || parseFloat(widthVal) <= 0 ||
              isNaN(parseFloat(heightVal)) || parseFloat(heightVal) <= 0) {
              alert("Invalid tray dimensions. Width and Height must be positive numbers.");
              return;
          }
          const command = `SET_TRAY_SIZE ${widthVal} ${heightVal}`;
          sendCommand(command);
      }
      // END NEW Rotation Control Functions
    </script>
  </body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H 