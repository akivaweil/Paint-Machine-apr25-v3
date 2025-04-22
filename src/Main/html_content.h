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
    .input-group input { padding: 8px; width: 45px; text-align: right; margin-bottom: 5px; background-color: #555; color: #eee; border: 1px solid #777; border-radius: 5px; } /* Reduced width to 45px (half of 90px) */
    .input-group select { padding: 8px; width: 110px; background-color: #555; color: #eee; border: 1px solid #777; border-radius: 5px; margin-bottom: 5px; } /* Styling for select elements to match inputs */
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
        <input type="number" id="gridCols" step="1" value="4" onchange="setGridSpacing()">
        <label for="gridRows">Rows:</label>
        <input type="number" id="gridRows" step="1" value="5" onchange="setGridSpacing()">
        <br>
        <span id="spacingDisplay">Spacing: X=Auto, Y=Auto</span> <!-- Updated display text -->
        <!-- <button id="setGridSpacingButton" class="button setting-button" onclick="setGridSpacing()">Set Grid Columns/Rows</button> --> <!-- REMOVED -->
    </div>
    <div class="input-group"> <!-- Added Tray Dimensions Group -->
        <h3>Tray Dimensions</h3>
        <label for="trayWidth">Width:</label>
        <input type="number" id="trayWidth" step="0.1" value="24.0" onchange="setTraySize()">
        <label for="trayHeight">Height:</label>
        <input type="number" id="trayHeight" step="0.1" value="18.0" onchange="setTraySize()">
        <!-- <button id="setTraySizeButton" class="button setting-button" onclick="setTraySize()">Set Tray Size</button> --> <!-- REMOVED -->
    </div>
  </div> <!-- ADDED -->
  <hr>
  <div class="section-pnp"> <!-- ADDED -->
    <h2>Pick & Place Control</h2>
    <div class="input-group">
        <h3>Pickup Location</h3> <!-- Changed header -->
        <label for="offsetX">X:</label>
        <input type="number" id="offsetX" step="0.1" value="15.0" onchange="setOffset()">
        <label for="offsetY">Y:</label>
        <input type="number" id="offsetY" step="0.1" value="0.0" onchange="setOffset()">
        <!-- <button id="setOffsetButton" class="button setting-button" onclick="setOffset()">Set Offset</button> --> <!-- REMOVED -->
    </div>

    <div class="input-group">
        <h3>First Drop Off Location</h3> <!-- Changed header -->
        <label for="firstPlaceXAbs">Abs X:</label>
        <input type="number" id="firstPlaceXAbs" step="0.1" value="20.0" onchange="setFirstPlaceAbs()">
        <label for="firstPlaceYAbs">Abs Y:</label>
        <input type="number" id="firstPlaceYAbs" step="0.1" value="20.0" onchange="setFirstPlaceAbs()">
        <!-- <button id="setFirstPlaceAbsButton" class="button setting-button" onclick="setFirstPlaceAbs()">Set Drop Off</button> --> <!-- REMOVED -->
    </div>

    <div class="input-group">
        <h3>Speed/Accel (k steps/s)</h3>
        <label for="xSpeed">X Spd:</label>
        <input type="number" id="xSpeed" step="1" value="20" onchange="setSpeedAccel()">
        <label for="xAccel">X Acc:</label>
        <input type="number" id="xAccel" step="1" value="20" onchange="setSpeedAccel()">
        <br>
        <label for="ySpeed">Y Spd:</label>
        <input type="number" id="ySpeed" step="1" value="20" onchange="setSpeedAccel()">
        <label for="yAccel">Y Acc:</label>
        <input type="number" id="yAccel" step="1" value="20" onchange="setSpeedAccel()">
        <!-- <button id="setSpeedButton" class="button setting-button" onclick="setSpeedAccel()">Set Speed/Accel</button> --> <!-- REMOVED -->
    </div>

    <button id="pnpButton" class="button" onclick="sendCommand('ENTER_PICKPLACE')">Pick and Place Mode</button>
    <button id="pnpStepButton" class="button" onclick="sendCommand('PNP_NEXT_STEP')">Next PnP Step (N)</button>
    <br style="margin-bottom: 5px;">
    <button id="pnpSkipButton" class="button" onclick="sendCommand('PNP_SKIP_LOCATION')" style="width: 70px; height: 70px; padding: 10px; font-size: 14px; line-height: 1.2; vertical-align: middle; margin-right: 5px;">Skip Next</button>
    <button id="pnpBackButton" class="button" onclick="sendCommand('PNP_BACK_LOCATION')" style="width: 70px; height: 70px; padding: 10px; font-size: 14px; line-height: 1.2; vertical-align: middle;">Back One</button>
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
          <button id="setFirstPlaceAbsCurrentButton" class="button setting-button" onclick="sendCommand('SET_FIRST_PLACE_ABS_FROM_CURRENT')">Set First Place Abs<br>From Current</button <!-- ID, text, and command updated -->
       </div>
       <div class="input-group">
          <h3>Current Position</h3>
          <span id="currentPosDisplay">X: 0.000, Y: 0.000, Z: 0.000</span>
       </div>
       <!-- ADDED SERVO CONTROL GROUP -->
       <div class="input-group">
          <h3>Pitch Servo</h3>
          <label for="manualPitchServo">Angle:</label>
          <input type="range" id="manualPitchServo" min="0" max="180" value="90" step="1" style="width: 150px; vertical-align: middle;" oninput="updateSliderDisplay('manualPitchServo')">
          <span id="manualPitchServoDisplay" style="margin-left: 10px; display: inline-block; width: 30px;">90</span> <!-- Added display span -->
          <button class="button setting-button" onclick="setManualServoPitch()" style="margin-top: 10px;">Set Pitch Angle</button>
       </div>
       <!-- END ADDED SERVO CONTROL GROUP -->
       <br style="clear:both;">
     </div>
     <button id="enterCalibrationButton" class="button" onclick="sendCommand('ENTER_CALIBRATION')">Manual Mode</button> <!-- Changed text -->
  </div> <!-- ADDED -->
   <hr style="clear:both;">

  <div class="section-paint"> <!-- ADDED -->
    <h2>Paint Control</h2>
    <div id="paintControls">
      <!-- Settings for Paint Gun Offset Only -->
      <div class="input-group">
          <h3>Paint Gun Offset</h3>
          <label for="paintGunOffsetX">Gun X:</label>
          <input type="number" id="paintGunOffsetX" step="0.1" value="0.0" onchange="setPaintOffsets()">
          <label for="paintGunOffsetY">Gun Y:</label>
          <input type="number" id="paintGunOffsetY" step="0.1" value="1.5" onchange="setPaintOffsets()">
          <!-- <button id="setPaintOffsetsButton" class="button setting-button" onclick="setPaintOffsets()">Set Gun Offset</button> --> <!-- REMOVED -->
      </div>

      <!-- Side Buttons -->
      <div style="margin: 15px 0;">
        <button id="stopButton" class="button" onclick="sendCommand('STOP')" style="background-color: #dc3545;">STOP</button>
      </div>

      <!-- Side Settings Panels -->
      <div style="clear:both;"></div>
      
      <!-- Back Side Settings (Side 0) - Stays First -->
      <div class="input-group">
        <h3>Back Side Settings</h3>
        <button id="paintBackButton" class="button" onclick="sendCommand('PAINT_SIDE_0')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Back Side</button>
        <label for="paintZ_0">Z Height:</label>
        <input type="number" id="paintZ_0" step="0.1" value="1.0" onchange="setPaintSideSettings(0)">
        
        <label for="paintP_0">Pitch:</label>
        <input type="number" id="paintP_0" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(0)">
        
        <label>Pattern:</label>
        <select id="paintR_0" onchange="setPaintSideSettings(0)">
          <option value="0" selected>Up-Down</option>
          <option value="90">Left-Right</option>
        </select>
        
        <label for="paintS_0">Speed:</label>
        <input type="range" id="paintS_0" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_0')" onchange="setPaintSideSettings(0)">
        <span id="paintSDisplay_0">20</span>
      </div>
      
      <!-- Front Side Settings (Side 2) - Moves to Second -->
      <div class="input-group">
        <h3>Front Side Settings</h3>
        <button id="paintFrontButton" class="button" onclick="sendCommand('PAINT_SIDE_2')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Front Side</button>
        <label for="paintZ_2">Z Height:</label>
        <input type="number" id="paintZ_2" step="0.1" value="1.0" onchange="setPaintSideSettings(2)">
        
        <label for="paintP_2">Pitch:</label>
        <input type="number" id="paintP_2" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(2)">
        
        <label>Pattern:</label>
        <select id="paintR_2" onchange="setPaintSideSettings(2)">
          <option value="0" selected>Up-Down</option>
          <option value="90">Left-Right</option>
        </select>
        
        <label for="paintS_2">Speed:</label>
        <input type="range" id="paintS_2" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_2')" onchange="setPaintSideSettings(2)">
        <span id="paintSDisplay_2">20</span>
      </div>

      <!-- Left Side Settings (Side 3) - Moves to Third -->
      <div class="input-group">
        <h3>Left Side Settings</h3>
        <button id="paintLeftButton" class="button" onclick="sendCommand('PAINT_SIDE_3')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Left Side</button>
        <label for="paintZ_3">Z Height:</label>
        <input type="number" id="paintZ_3" step="0.1" value="1.0" onchange="setPaintSideSettings(3)">
        
        <label for="paintP_3">Pitch:</label>
        <input type="number" id="paintP_3" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(3)">
        
        <label>Pattern:</label>
        <select id="paintR_3" onchange="setPaintSideSettings(3)">
          <option value="0" selected>Up-Down</option>
          <option value="90">Left-Right</option>
        </select>
        
        <label for="paintS_3">Speed:</label>
        <input type="range" id="paintS_3" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_3')" onchange="setPaintSideSettings(3)">
        <span id="paintSDisplay_3">20</span>
      </div>

      <!-- Right Side Settings (Side 1) - Moves to Fourth/Last -->
      <div class="input-group">
        <h3>Right Side Settings</h3>
        <button id="paintRightButton" class="button" onclick="sendCommand('PAINT_SIDE_1')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Right Side</button>
        <label for="paintZ_1">Z Height:</label>
        <input type="number" id="paintZ_1" step="0.1" value="1.0" onchange="setPaintSideSettings(1)">
        
        <label for="paintP_1">Pitch:</label>
        <input type="number" id="paintP_1" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(1)">
        
        <label>Pattern:</label>
        <select id="paintR_1" onchange="setPaintSideSettings(1)">
          <option value="0" selected>Up-Down</option>
          <option value="90">Left-Right</option>
        </select>
        
        <label for="paintS_1">Speed:</label>
        <input type="range" id="paintS_1" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_1')" onchange="setPaintSideSettings(1)">
        <span id="paintSDisplay_1">20</span>
      </div>
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
      // var offsetDisplaySpan = document.getElementById('offsetDisplay');

      // Track machine state
      var firstPlaceXAbsInput = document.getElementById('firstPlaceXAbs'); // Updated ID
      var firstPlaceYAbsInput = document.getElementById('firstPlaceYAbs'); // Updated ID
      // var firstPlaceAbsDisplaySpan = document.getElementById('firstPlaceAbsDisplay'); // Updated ID

      // Grid/Spacing Elements
      var gridColsInput = document.getElementById('gridCols');
      var gridRowsInput = document.getElementById('gridRows');
      // var gridDisplaySpan = document.getElementById('gridDisplay');
      var spacingDisplaySpan = document.getElementById('spacingDisplay');

      // Tray Size Elements (NEW)
      var trayWidthInput = document.getElementById('trayWidth');
      var trayHeightInput = document.getElementById('trayHeight');
      // var traySizeDisplaySpan = document.getElementById('traySizeDisplay');

      var xSpeedInput = document.getElementById('xSpeed');
      var xAccelInput = document.getElementById('xAccel');
      var ySpeedInput = document.getElementById('ySpeed');
      var yAccelInput = document.getElementById('yAccel');
      // var xSpeedDisplaySpan = document.getElementById('xSpeedDisplay');
      // var xAccelDisplaySpan = document.getElementById('xAccelDisplay');
      // var ySpeedDisplaySpan = document.getElementById('ySpeedDisplay');
      // var yAccelDisplaySpan = document.getElementById('yAccelDisplay');

      // Calibration Elements
      var enterCalibrationButton = document.getElementById('enterCalibrationButton');
      var calibrationControlsDiv = document.getElementById('calibrationControls');
      var jogStepInput = document.getElementById('jogStep');
      var currentPosDisplaySpan = document.getElementById('currentPosDisplay');

      // Paint Offset Elements
      var paintGunOffsetXInput = document.getElementById('paintGunOffsetX');
      var paintGunOffsetYInput = document.getElementById('paintGunOffsetY');
      // var paintGunOffsetDisplaySpan = document.getElementById('paintGunOffsetDisplay');

      // Paint Side Settings Elements (Arrays)
      var paintZInputs = [];
      var paintPInputs = [];
      var paintRInputs = [];
      var paintSInputs = [];
      var paintZDisplays = []; // Keep Z for side 0 if needed, but mostly removed
      var paintPDisplays = []; // Keep P for side 0 if needed, but mostly removed
      // var paintRDisplays = []; // R displays were removed entirely
      var paintSDisplays = [];
      var setPaintSideButtons = [];
      for (let i = 0; i < 4; i++) {
          paintZInputs[i] = document.getElementById(`paintZ_${i}`);
          paintPInputs[i] = document.getElementById(`paintP_${i}`);
          paintRInputs[i] = document.getElementById(`paintR_${i}`);
          paintSInputs[i] = document.getElementById(`paintS_${i}`);
          // Only get Z/P displays if they exist (e.g., for side 0 or if added back)
          paintZDisplays[i] = document.getElementById(`paintZDisplay_${i}`); 
          paintPDisplays[i] = document.getElementById(`paintPDisplay_${i}`);
          // R displays are gone
          paintSDisplays[i] = document.getElementById(`paintSDisplay_${i}`); // S displays exist for sliders
          // Assuming button IDs follow a pattern like setPaintSideButton_0 (adjust if needed)
          // setPaintSideButtons[i] = document.getElementById(`setPaintSideButton_${i}`);
          // Simpler: get button references from within the setPaintSideSettings function based on index
      }

      // Paint Control Elements
      // var startPaintingButton = document.getElementById('startPaintingButton');
      var paintBackButton = document.getElementById('paintBackButton');
      var stopButton = document.getElementById('stopButton');
      // var calibrateButton = document.getElementById('calibrateButton');

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
        sendCommand('GET_STATUS'); // Request current status from ESP32
        enableButtons(); // Enable/disable buttons based on initial state (will be updated by GET_STATUS response)
      }

      function onClose(event) {
        console.log('Connection closed');
        statusDiv.innerHTML = 'Disconnected - Retrying...';
        statusDiv.style.color = 'red';
        connectionIndicatorSpan.innerHTML = ' (Offline)'; // Show indicator on close
        
        // Reset mode flags on disconnect
        isMoving = false;
        isHoming = false;
        allHomed = false;
        
        enableButtons(); // Fixed: Removed parameters that don't match function signature
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
              console.log(`JS Debug: Before state update: isMoving=${isMoving}, isHoming=${isHoming}`);
              // --- End JS Debugging ---

              // Update JavaScript state variables FIRST
              isMoving = (data.status === "Busy" || data.status === "Moving" || data.status === "Homing");
              isHoming = (data.status === "Homing");
              
              // Check for explicit homed status if available
              if (data.hasOwnProperty('homed')) {
                  console.log(`JS Debug: Received explicit homed status=${data.homed}`);
                  allHomed = data.homed; // Use explicit homed status from server
              }

              // --- Update UI Elements based on data ---
              // Update offset display and inputs if offset info is present
              if (data.hasOwnProperty('pnpOffsetX') && data.hasOwnProperty('pnpOffsetY')) {
                  let currentX = parseFloat(data.pnpOffsetX).toFixed(1); // Changed to 1 decimal place
                  let currentY = parseFloat(data.pnpOffsetY).toFixed(1); // Changed to 1 decimal place
                  // offsetDisplaySpan.innerHTML = `X=${currentX}, Y=${currentY}`;
                  offsetXInput.value = currentX;
                  offsetYInput.value = currentY;
              }

              // Update first place display and inputs if info is present
              if (data.hasOwnProperty('placeFirstXAbs') && data.hasOwnProperty('placeFirstYAbs')) { // Updated keys
                  let currentX = parseFloat(data.placeFirstXAbs).toFixed(1); // Changed to 1 decimal place
                  let currentY = parseFloat(data.placeFirstYAbs).toFixed(1); // Changed to 1 decimal place
                  // firstPlaceAbsDisplaySpan.innerHTML = `X=${currentX}, Y=${currentY}`; // Updated span ID
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
                  // xSpeedDisplaySpan.innerHTML = `${xS_display}`;
                  // xAccelDisplaySpan.innerHTML = `${xA_display}`;
                  // ySpeedDisplaySpan.innerHTML = `${yS_display}`;
                  // yAccelDisplaySpan.innerHTML = `${yA_display}`;
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
                  
                  console.log(`[DEBUG] Processing grid settings from server: cols=${currentCols}, rows=${currentRows}, gapX=${currentGapX}, gapY=${currentGapY}`);
                  
                  // gridDisplaySpan.innerHTML = `${currentCols} x ${currentRows}`;
                  // Update display text to show Gap with more information
                  spacingDisplaySpan.innerHTML = `Gap: X=${currentGapX}, Y=${currentGapY}`;
                  
                  // Update the input fields
                  gridColsInput.value = currentCols;
                  gridRowsInput.value = currentRows;
                  // Note: We don't update input fields for gapX/gapY as they are calculated, not set directly.
              }

              // Update Tray Size display and inputs if info is present (NEW)
              if (data.hasOwnProperty('trayWidth') && data.hasOwnProperty('trayHeight')) {
                  let currentW = parseFloat(data.trayWidth).toFixed(2);
                  let currentH = parseFloat(data.trayHeight).toFixed(2);
                  
                  console.log(`[DEBUG] Processing tray dimensions from server: width=${currentW}, height=${currentH}`);
                  
                  // traySizeDisplaySpan.innerHTML = `W=${currentW}, H=${currentH}`;
                  trayWidthInput.value = currentW;
                  trayHeightInput.value = currentH;
              }

              // Update Paint Offsets display and inputs if info is present
              if (data.hasOwnProperty('paintGunOffX') && data.hasOwnProperty('paintGunOffY')) {
                  let currentX = parseFloat(data.paintGunOffX).toFixed(2);
                  let currentY = parseFloat(data.paintGunOffY).toFixed(2);
                  // paintGunOffsetDisplaySpan.innerHTML = `X=${currentX}, Y=${currentY}`;
                  paintGunOffsetXInput.value = currentX;
                  paintGunOffsetYInput.value = currentY;
              }

              // Update Paint Side Settings display and inputs
              for (let i = 0; i < 4; i++) {
                  let keyZ = `paintZ_${i}`;
                  let keyP = `paintP_${i}`;
                  let keyPat = `paintPat_${i}`; // USE THIS KEY (matches C++)
                  let keyS = `paintS_${i}`;
                  if (data.hasOwnProperty(keyZ) && paintZInputs[i]) { // Check only for input
                      let currentZ = parseFloat(data[keyZ]).toFixed(2);
                      paintZInputs[i].value = currentZ;
                      // Update display span only if it exists
                      if (paintZDisplays[i]) paintZDisplays[i].innerHTML = `${currentZ}`;
                  }
                  if (data.hasOwnProperty(keyP) && paintPInputs[i]) { // Check only for input
                      let currentP = parseInt(data[keyP]);
                      paintPInputs[i].value = currentP;
                      // Update display span only if it exists
                      if (paintPDisplays[i]) paintPDisplays[i].innerHTML = `${currentP}&deg;`;
                  }
                  if (data.hasOwnProperty(keyPat) && paintRInputs[i]) { 
                      let currentPattern = parseInt(data[keyPat]);
                      paintRInputs[i].value = currentPattern; 
                  }
                  if (data.hasOwnProperty(keyS) && paintSInputs[i] && paintSDisplays[i]) {
                      // Receive raw speed (e.g., 15000), divide by 1000 for UI display
                      let rawSpeed = parseFloat(data[keyS]); 
                      let displaySpeed = Math.round(rawSpeed / 1000); // Divide here
                      paintSInputs[i].value = displaySpeed; // Update slider value (5-25)
                      paintSDisplays[i].innerHTML = `${displaySpeed}`; // Update display span - this one exists
                  }
              }

              // Update current position display if it's a position update message
              if (data.type === "positionUpdate") {
                  currentPosDisplaySpan.innerHTML = `X: ${parseFloat(data.posX).toFixed(3)}, Y: ${parseFloat(data.posY).toFixed(3)}, Z: ${parseFloat(data.posZ).toFixed(3)}`;
              }

              // --- Update UI Mode (Buttons, Controls visibility) based on status ---
              if (data.status === "Ready" || data.status === "Error") {
                  console.log("JS Debug: Executing 'Ready' or 'Error' block"); // JS Debug
                  statusDiv.style.color = (data.status === "Ready") ? 'green' : 'red';
                  if (data.status === "Ready") allHomed = true; // Set homed on Ready but don't unset on Error
                  
                  // Reset the Pick & Place mode flag (isMoving was already reset based on status)
                  console.log("JS Debug: Resetting PnP/Calib UI elements");
                  
                  pnpButton.innerHTML = "Enter Pick/Place"; // Reset button text
                  pnpButton.className = "button"; // Reset to default button style
                  pnpButton.onclick = function() { sendCommand('ENTER_PICKPLACE'); }; // Reset function
                  enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
              } else if (data.status === "PickPlaceReady") {
                  console.log("JS Debug: Executing 'PickPlaceReady' block"); // JS Debug
                  statusDiv.style.color = 'blue'; // Indicate special mode
                  allHomed = true; // ADDED: Assume homed if ready for PnP
                  
                  pnpButton.innerHTML = "Exit Pick and Place Mode";
                  pnpButton.className = "button exit-button"; // Add red styling
                  pnpButton.disabled = false; // Ensure enabled here, enableButtons will handle final state 
                  pnpButton.onclick = function() { sendCommand('EXIT_PICKPLACE'); }; // Change function to exit
                  enterCalibrationButton.style.display = 'none'; // Hide Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
              } else if (data.status === "Busy" || data.status === "Moving" || data.status === "Homing") {
                  console.log("JS Debug: Executing 'Busy/Moving/Homing' block"); // JS Debug
                  statusDiv.style.color = 'orange';
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls on Homing
                  enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button on Homing
              } else if (data.status === "CalibrationActive") {
                  console.log("JS Debug: Executing 'CalibrationActive' block"); // JS Debug
                  statusDiv.style.color = 'purple'; // Indicate calibration mode
                  allHomed = true; // Assume homed when entering calibration
                  enterCalibrationButton.style.display = 'none'; // Hide Enter Cal button
                  calibrationControlsDiv.style.display = 'block'; // Show Cal controls
                  pnpButton.innerHTML = "Enter Pick/Place"; // Ensure PnP button is reset
                  pnpButton.className = "button"; // Reset to default button style
                  pnpButton.onclick = function() { sendCommand('ENTER_PICKPLACE'); };
              } else if (data.status === "PickPlaceComplete") {
                  console.log("JS Debug: Executing 'PickPlaceComplete' block"); // JS Debug
                  statusDiv.style.color = 'green';
                  if(pnpButton) { // Add null check for safety
                     pnpButton.innerHTML = "Sequence Complete"; // Update PnP button text
                     pnpButton.disabled = true; // Disable PnP button as sequence is done
                  }
                  enterCalibrationButton.style.display = 'inline-block'; // Show Enter Cal button
                  calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
              }

              // --- Final step: Update all button states based on the latest JS state variables ---
              console.log(`JS Debug: Before final enableButtons: isMoving=${isMoving}, isHoming=${isHoming}`); // JS Debug
              enableButtons(); // ADDED single call here

          } catch (e) {
              console.error("Error processing WebSocket message:", e); // Added error logging
              statusDiv.innerHTML = `Received raw: ${event.data}`; // Show raw data if JSON parse fails
              statusDiv.style.color = 'blue';
              enableButtons(); // Also update buttons on error, might be disconnected state
          }
      }

       function onError(event) {
          console.error("WebSocket Error: ", event);
          statusDiv.innerHTML = 'WebSocket Error';
          statusDiv.style.color = 'red';
          connectionIndicatorSpan.innerHTML = ' (Error)'; // Show indicator on error
          enterCalibrationButton.style.display = 'inline-block'; // Ensure Enter Cal button is visible on error/disconnect
          calibrationControlsDiv.style.display = 'none'; // Ensure Cal controls are hidden on error/disconnect
          enableButtons(); // Update buttons on error
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
        let connected = websocket && websocket.readyState === WebSocket.OPEN;
        console.log("enableButtons() - Debug State: Connected="+connected+", AllHomed="+allHomed+", IsMoving="+isMoving+", IsHoming="+isHoming);
        
        let inCalibMode = calibrationControlsDiv.style.display === 'block';
        let inPickAndPlaceMode = pnpButton.innerHTML.includes('Exit'); // Check if in PnP mode by button text
        let canStop = connected && (isMoving || isHoming || inCalibMode); // Can stop if doing anything interruptible

        homeButton.disabled = !connected; // Home All button is always enabled if connected
        stopButton.disabled = !canStop;

        // PnP Button Logic - UPDATED
        // Disable if not connected, not homed, moving, homing, in calibration, or sequence complete
        pnpButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode || pnpButton.innerHTML.includes('Complete');
        
        // Add detailed debugging for PnP button
        if (pnpButton.disabled) {
            console.log(`PnP button disabled because: connected=${connected}, homed=${allHomed}, moving=${isMoving}, homing=${isHoming}, inCalibMode=${inCalibMode}, complete=${pnpButton.innerHTML.includes('Complete')}`);
        }

        // PnP Step Button Logic (Only enabled in PnPReady state)
        pnpStepButton.disabled = !(connected && isMoving && !isHoming);
        pnpSkipButton.disabled = pnpStepButton.disabled; // Skip/Back follow Step button logic
        pnpBackButton.disabled = pnpStepButton.disabled; // Skip/Back follow Step button logic

        // Calibration Button Logic
        enterCalibrationButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode;

        // Simple, safe debugging for Manual Mode button state
        if (enterCalibrationButton.disabled) {
            console.log(`Manual Mode button disabled: connected=${connected}, homed=${allHomed}, moving=${isMoving}, homing=${isHoming}, inCalib=${inCalibMode}`);
        }

        // --- Painting Buttons ---
        // Disable painting if not connected, not homed, moving, homing, in PnP, or calibrating
        // startPaintingButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode; // Requires homing
        paintBackButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode; // Requires homing

        // Rotation Buttons
        // MODIFIED: Only disable if not connected or actively moving/homing
        // Now matching the Grid and Tray Size buttons behavior
        rotateMinus90Button.disabled = !connected || isMoving || isHoming;
        rotatePlus90Button.disabled = rotateMinus90Button.disabled;
        rotateMinus5Button.disabled = rotateMinus90Button.disabled;
        rotatePlus5Button.disabled = rotateMinus90Button.disabled;
        setRotationZeroButton.disabled = rotateMinus90Button.disabled;

        // --- Settings Buttons/Inputs ---
        // Generally disable settings inputs/buttons if disconnected, moving, homing, in PnP, or calibrating
        // Special exceptions for Grid/Tray buttons
        let generalDisableCondition = !connected || isMoving || isHoming || inCalibMode;

        // *** Keep Grid and Tray Size buttons enabled if connected ***
        // setGridSpacingButton.disabled = !connected; // REMOVED
        // setTraySizeButton.disabled = !connected; // REMOVED
        // *** End Special Exception ***

        // setOffsetButton.disabled = generalDisableCondition || !allHomed; // REMOVED
        // setFirstPlaceAbsButton.disabled = generalDisableCondition || !allHomed; // REMOVED
        // setSpeedButton.disabled = generalDisableCondition || !allHomed; // REMOVED
        // setPaintOffsetsButton.disabled = generalDisableCondition || !allHomed; // REMOVED

        // Determine input field disable state based on conditions, not removed buttons
        let settingsInputsDisableCondition = generalDisableCondition || !allHomed; // Most settings need homing

        offsetXInput.disabled = settingsInputsDisableCondition;
        offsetYInput.disabled = settingsInputsDisableCondition;
        firstPlaceXAbsInput.disabled = settingsInputsDisableCondition;
        firstPlaceYAbsInput.disabled = settingsInputsDisableCondition;
        xSpeedInput.disabled = settingsInputsDisableCondition;
        xAccelInput.disabled = settingsInputsDisableCondition;
        ySpeedInput.disabled = settingsInputsDisableCondition;
        yAccelInput.disabled = settingsInputsDisableCondition;
        paintGunOffsetXInput.disabled = settingsInputsDisableCondition;
        paintGunOffsetYInput.disabled = settingsInputsDisableCondition;

        // Grid/Tray inputs are linked to their buttons (only disabled if disconnected)
        gridColsInput.disabled = !connected;
        gridRowsInput.disabled = !connected;
        trayWidthInput.disabled = !connected;
        trayHeightInput.disabled = !connected;

        // Calibration Controls (only enabled when in calibration mode and not moving/homing)
        let calibControlsDisable = !connected || !inCalibMode || isMoving || isHoming;
        let calibInputs = calibrationControlsDiv.querySelectorAll('input, button');
        calibInputs.forEach(el => {
            el.disabled = calibControlsDisable;
        });

        // Download/Upload Buttons (only need connection)
        document.getElementById('downloadSettingsButton').disabled = !connected;
        document.getElementById('uploadSettingsButton').disabled = !connected;

        // Specific overrides based on state machine logic elsewhere (e.g., PnPComplete disabling pnpButton)
        if (pnpButton.innerHTML.includes('Complete')) {
             pnpButton.disabled = true;
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
        const gunX = paintGunOffsetXInput.value;
        const gunY = paintGunOffsetYInput.value;
        // Basic validation
        if (isNaN(parseFloat(gunX)) || isNaN(parseFloat(gunY))) {
            alert("Invalid paint gun offset values. Please enter numbers.");
            return;
        }
        // Send command to set gun offset
        sendCommand(`SET_PAINT_GUN_OFFSET ${gunX} ${gunY}`);
    }

    function setPaintSideSettings(sideIndex) {
        if (sideIndex < 0 || sideIndex > 3) return; // Invalid index

        const zVal = paintZInputs[sideIndex].value;
        const pVal = paintPInputs[sideIndex].value;
        const rVal = paintRInputs[sideIndex].value;
        const sVal = paintSInputs[sideIndex].value;

        // Convert speed from slider value (5-25) back to steps/sec (5k-25k) -> Max speed updated to 25k
        const speedSteps = parseFloat(sVal) * 1000; 
        const constrainedSpeed = Math.max(5000, Math.min(25000, speedSteps)); // Use 25k max

        // REMOVED MAPPING - UI value (pVal) is now 0-180 and sent directly
        const pitchToSend = parseInt(pVal); // Send the value directly (0-180)

        const patternValue = parseInt(rVal); // Convert pattern string '0' or '90' to integer

        // Validate pitch (0-180)
        if (pitchToSend < 0 || pitchToSend > 180) {
            alert(`Error: Pitch for side ${sideIndex} must be between 0 and 180.`);
            return; // Stop before sending command
        }
        // Validate pattern (0 or 90)
        if (patternValue !== 0 && patternValue !== 90) {
             alert(`Error: Pattern for side ${sideIndex} must be 0 or 90.`);
             return; // Stop before sending command
        }

        // Send command with direct pitch value
        const command = `SET_PAINT_SIDE_SETTINGS ${sideIndex} ${zVal} ${pitchToSend} ${patternValue} ${constrainedSpeed}`;
        console.log("Sending Paint Side Settings:", command); // DEBUG
        sendCommand(command);
    }

    // Helper to update slider value display
    function updateSliderDisplay(sliderId) {
        const slider = document.getElementById(sliderId);
        // Simpler approach: Assume display ID is slider ID + 'Display'
        const displaySpanId = sliderId + 'Display';
        const displaySpan = document.getElementById(displaySpanId);

        if (slider && displaySpan) {
            let value = slider.value;
            let suffix = ''; // No suffix needed for pitch angle

            displaySpan.innerHTML = value + suffix;
        } else {
             console.error(`Could not find slider '${sliderId}' or display span '${displaySpanId}'`); // Add error logging
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
                pattern: parseInt(paintRInputs[i].value) || 0, // Changed key to pattern
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
                    paintGunOffsetXInput.value = settings.paintOffsets.gunX;
                    paintGunOffsetYInput.value = settings.paintOffsets.gunY;
                }
                
                if (settings.paintSides && settings.paintSides.length === 4) {
                    for (let i = 0; i < 4; i++) {
                        if (settings.paintSides[i]) {
                            paintZInputs[i].value = settings.paintSides[i].z;
                            paintPInputs[i].value = settings.paintSides[i].pitch;
                            
                            // For pattern, we need to set the select element
                            const patternSelect = document.getElementById(`paintR_${i}`);
                            if (patternSelect) {
                                patternSelect.value = settings.paintSides[i].pattern; // Use pattern key
                            }
                            
                            paintSInputs[i].value = settings.paintSides[i].speed; // Assume saved speed is 5-25 range
                            
                            // Update displays
                            updateSliderDisplay(`paintS_${i}`);
                            document.getElementById(`paintZDisplay_${i}`).innerText = `Current: ${settings.paintSides[i].z}`;
                            document.getElementById(`paintPDisplay_${i}`).innerText = `${settings.paintSides[i].pitch}&deg;`;
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

    // Function to send the manual servo pitch command
    function setManualServoPitch() {
        const angle = document.getElementById('manualPitchServo').value;
        sendCommand(`SET_SERVO_PITCH ${angle}`);
    }
    </script>
  </body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H 