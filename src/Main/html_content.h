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
    .setting-button { padding: 10px 15px !important; display: block; margin: 5px auto !important; } /* Fixed conflicting margin declarations */
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
    .section-main-control .input-group { border-color: #f44336; } /* Red for main controls */

    /* Section Header Colors */
    .section-grid h2 { color: #4285F4; } /* Blue */
    .section-pnp h2 { color: #34A853; } /* Green */
    .section-rotation h2 { color: #E91E63; } /* Pink */
    .section-manual h2 { color: #FBBC05; } /* Orange */
    .section-paint h2 { color: #A142F4; } /* Purple */
    .section-main-control h2 { color: #f44336; } /* Red for main controls */

    /* Hide number input spinners */
    input[type=number]::-webkit-outer-spin-button,
    input[type=number]::-webkit-inner-spin-button {
      -webkit-appearance: none;
      margin: 0;
    }
    input[type=number] {
      -moz-appearance: textfield; /* Firefox */
    }

    /* Apply consistent styling to number inputs in setting-box */
    .setting-box input[type=number] {
      padding: 8px;
      width: 45px; /* Match input-group width */
      text-align: right;
      margin-bottom: 5px;
      background-color: #555;
      color: #eee;
      border: 1px solid #777;
      border-radius: 5px;
    }

    .exit-button { background-color: #f44336; } /* Red for Exit button */
    .exit-button:active { background-color: #d32f2f; } /* Darker red on click */

  </style>
</head>
<body>
  <h1>Paint + Pick and Place Machine</h1> <!-- Changed header -->

  <!-- == Status Area == -->
  <div id="status">Connecting to ESP32...</div> <span id="connectionIndicator" style="color:red; font-weight:bold;"></span>
  <hr>

  <!-- == Main Navigation/Action Buttons == -->
  <button id="homeButton" class="button" onclick="sendCommand('HOME')">Home All Axes</button>
  <button id="gotoButton" class="button" onclick="sendCommand('GOTO_5_5_0')">Go to (5, 5, 0)</button>
  <button id="gotoButton2020" class="button" onclick="sendCommand('GOTO_20_20_0')">Go to (20, 20, 0)</button>
  <button class="button exit-button" onclick="sendCommand('STOP')" style="margin-left: 20px;">STOP</button>
  <hr>

  <!-- == Mode Selection / High-Level Actions == -->
  <div class="section-main-control">
    <h2>Main Controls</h2>
    <div class="input-group" style="text-align: center; width: 100%;">
      <button class="button" onclick="sendCommand('PAINT_SIDE_0')">Paint Back</button>
      <button class="button" onclick="sendCommand('PAINT_SIDE_2')">Paint Front</button>
      <button class="button" onclick="sendCommand('PAINT_SIDE_3')">Paint Left</button>
      <button class="button" onclick="sendCommand('PAINT_SIDE_1')">Paint Right</button>
      <button class="button" onclick="sendCommand('PAINT_ALL')" style="background-color: #ffc107;">Paint All Sides</button>
      <br>
      <button class="button" onclick="sendCommand('CLEAN_GUN')" style="background-color: #00bcd4;">Clean Gun</button>
      <button id="pressurizeButton" class="button" onclick="togglePressure()" style="background-color: #ff9800;">Pressurize</button>
      <button id="pnpButton" class="button" onclick="sendCommand('ENTER_PICKPLACE')">Pick and Place Mode</button> 
      <button id="enterCalibrationButton" class="button" onclick="sendCommand('ENTER_CALIBRATION')">Manual Mode</button> <!-- Moved from Manual section -->
    </div>
  </div>
  <hr>
  <!-- ======================================== -->

  <!-- == Pick & Place Section == -->
  <div class="section-pnp">
    <h2>Pick & Place Control</h2>
    <!-- PnP Settings -->
    <div class="input-group">
        <h3>Pickup Location</h3>
        <label for="offsetX">X:</label>
        <input type="number" id="offsetX" step="0.1" value="15.0" onchange="setOffset()">
        <label for="offsetY">Y:</label>
        <input type="number" id="offsetY" step="0.1" value="0.0" onchange="setOffset()">
    </div>
    <div class="input-group">
        <h3>First Drop Off Location</h3>
        <label for="firstPlaceXAbs">Abs X:</label>
        <input type="number" id="firstPlaceXAbs" step="0.1" value="20.0" onchange="setFirstPlaceAbs()">
        <label for="firstPlaceYAbs">Abs Y:</label>
        <input type="number" id="firstPlaceYAbs" step="0.1" value="20.0" onchange="setFirstPlaceAbs()">
    </div>
    <div class="setting-box"> <!-- Consider making this an input-group -->
        <h4>Movement Speed</h4>
        <label for="patternXSpeed">X Speed:</label>
        <input type="number" id="patternXSpeed" value="20">
        <br>
        <label for="patternYSpeed">Y Speed:</label>
        <input type="number" id="patternYSpeed" value="20">
    </div>
    <div style="clear: both; margin-top: 10px;"></div> <!-- Clear float/inline-block -->
    <!-- PnP Action Buttons -->
    <button id="pnpStepButton" class="button" onclick="sendCommand('PNP_NEXT_STEP')" style="display: none;">Next PnP Step (N)</button>
    <button id="pnpSkipButton" class="button" onclick="sendCommand('PNP_SKIP_LOCATION')" style="width: 70px; height: 70px; padding: 10px; font-size: 14px; line-height: 1.2; vertical-align: middle; margin-right: 5px; display: none;">Skip Next</button>
    <button id="pnpBackButton" class="button" onclick="sendCommand('PNP_BACK_LOCATION')" style="width: 70px; height: 70px; padding: 10px; font-size: 14px; line-height: 1.2; vertical-align: middle; display: none;">Back One</button>
  </div>
  <hr>
  <!-- ========================== -->

  <!-- == Manual Mode Section == -->
  <div class="section-manual">
    <h2>Manual Mode</h2> 
    <!-- Enter button moved to Mode Selection -->
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
          <button id="setFirstPlaceAbsCurrentButton" class="button setting-button" onclick="sendCommand('SET_FIRST_PLACE_ABS_FROM_CURRENT')">Set First Place Abs<br>From Current</button>
       </div>
       <div class="input-group">
          <h3>Current Position</h3>
          <span id="currentPosDisplay">X: 0.000, Y: 0.000, Z: 0.000</span>
       </div>
       <div class="input-group">
          <h3>Pitch Servo</h3>
          <label for="manualPitchServo">Angle:</label>
          <input type="range" id="manualPitchServo" min="0" max="180" value="90" step="1" style="width: 150px; vertical-align: middle;" oninput="updateSliderDisplay('manualPitchServo')">
          <span id="manualPitchServoDisplay" style="margin-left: 10px; display: inline-block; width: 30px;">90</span>
          <button class="button setting-button" onclick="setManualServoPitch()" style="margin-top: 10px;">Set Pitch Angle</button>
       </div>
       <br style="clear:both;">
     </div>
  </div>
  <hr style="clear:both;">
  <!-- ========================= -->

  <!-- == Paint Control Section == -->
  <div class="section-paint">
    <h2>Paint Control</h2>
    <div id="paintControls">
      <!-- Settings for Paint Gun Offset - RESTORED -->
      <div class="input-group" style="background-color: #444;">
        <h3>Paint Gun Offset (inches from TCP)</h3>
        <label for="paintGunOffsetX">Offset X:</label>
        <input type="number" id="paintGunOffsetX" step="0.1" value="0.0" onchange="setPaintGunOffset()">
        <label for="paintGunOffsetY">Offset Y:</label>
        <input type="number" id="paintGunOffsetY" step="0.1" value="1.5" onchange="setPaintGunOffset()">
        <button class="button setting-button" onclick="setPaintGunOffset()">Save Gun Offset</button>
      </div>
      
      <!-- Side Settings Panels -->
      <div style="clear:both;"></div>
      
      <!-- Back Side Settings (Side 0) -->
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
          <option value="90">Sideways</option>
        </select>
        <label for="paintS_0">Speed:</label>
        <input type="range" id="paintS_0" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_0')" onchange="setPaintSideSettings(0)">
        <span id="paintSDisplay_0">20</span>
      </div>
      
      <!-- Front Side Settings (Side 2) -->
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
          <option value="90">Sideways</option>
        </select>
        <label for="paintS_2">Speed:</label>
        <input type="range" id="paintS_2" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_2')" onchange="setPaintSideSettings(2)">
        <span id="paintSDisplay_2">20</span>
      </div>

      <!-- Left Side Settings (Side 3) -->
      <div class="input-group">
        <h3>Left Side Settings</h3>
        <button id="paintLeftButton" class="button" onclick="sendCommand('PAINT_SIDE_3')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Left Side</button>
        <label for="paintZ_3">Z Height:</label>
        <input type="number" id="paintZ_3" step="0.1" value="1.0" onchange="setPaintSideSettings(3)">
        <label for="paintP_3">Pitch:</label>
        <input type="number" id="paintP_3" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(3)">
        <label>Pattern:</label>
        <select id="paintR_3" onchange="setPaintSideSettings(3)">
          <option value="0">Up-Down</option>
          <option value="90" selected>Sideways</option>
        </select>
        <label for="paintS_3">Speed:</label>
        <input type="range" id="paintS_3" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_3')" onchange="setPaintSideSettings(3)">
        <span id="paintSDisplay_3">20</span>
      </div>

      <!-- Right Side Settings (Side 1) -->
      <div class="input-group">
        <h3>Right Side Settings</h3>
        <button id="paintRightButton" class="button" onclick="sendCommand('PAINT_SIDE_1')" style="background-color: #A142F4; display: block; margin: 0 auto 15px auto;">Paint Right Side</button>
        <label for="paintZ_1">Z Height:</label>
        <input type="number" id="paintZ_1" step="0.1" value="1.0" onchange="setPaintSideSettings(1)">
        <label for="paintP_1">Pitch:</label>
        <input type="number" id="paintP_1" step="1" value="90" min="0" max="180" onchange="setPaintSideSettings(1)">
        <label>Pattern:</label>
        <select id="paintR_1" onchange="setPaintSideSettings(1)">
          <option value="0">Up-Down</option>
          <option value="90" selected>Sideways</option>
        </select>
        <label for="paintS_1">Speed:</label>
        <input type="range" id="paintS_1" min="5" max="25" value="20" step="1" oninput="updateSliderDisplay('paintS_1')" onchange="setPaintSideSettings(1)">
        <span id="paintSDisplay_1">20</span>
      </div>
    </div>
  </div>
  <hr>
  <!-- ========================= -->

  <!-- == Rotation Section == -->
  <div class="section-rotation">
    <h2>Tray Rotation Control</h2>
    <div class="input-group">
      <h3>Rotation</h3>
      <button id="rotateMinus90Button" class="button" onclick="rotateTray(-90)" style="width: 80px;">-90&deg;</button>
      <button id="rotatePlus90Button" class="button" onclick="rotateTray(90)" style="width: 80px;">+90&deg;</button>
      <button id="rotateMinus5Button" class="button" onclick="rotateTray(-5)" style="width: 70px; margin-top: 10px;">-5&deg;</button>
      <button id="rotatePlus5Button" class="button" onclick="rotateTray(5)" style="width: 70px; margin-top: 10px;">+5&deg;</button>
      <br>
      <button id="setRotationZeroButton" class="button setting-button" onclick="setRotationZero()" style="background-color: #ff9800;">Set Zero Angle</button>
    </div>
  </div>
  <hr>
  <!-- ======================== -->

  <!-- == Grid & Tray Settings == -->
  <div class="section-grid">
    <h2>Master Grid & Tray Settings</h2>
    <div class="input-group">
        <h3>Grid & Spacing</h3>
        <label for="gridCols">Columns:</label>
        <input type="number" id="gridCols" step="1" value="4" onchange="setGridSpacing()">
        <label for="gridRows">Rows:</label>
        <input type="number" id="gridRows" step="1" value="5" onchange="setGridSpacing()">
        <br>
        <span id="spacingDisplay">Spacing: X=Auto, Y=Auto</span>
    </div>
    <div class="input-group">
        <h3>Tray Dimensions</h3>
        <label for="trayWidth">Width:</label>
        <input type="number" id="trayWidth" step="0.1" value="24.0" onchange="setTraySize()">
        <label for="trayHeight">Height:</label>
        <input type="number" id="trayHeight" step="0.1" value="18.0" onchange="setTraySize()">
    </div>
  </div>
  <hr>
  <!-- ========================== -->

  <!-- == Settings Import/Export == -->
  <button id="downloadSettingsButton" class="button setting-button" onclick="downloadSettings()" style="background-color: #007bff; float: right; margin: 10px;">Download Settings</button>
  <button id="uploadSettingsButton" class="button setting-button" onclick="document.getElementById('settingsFileInput').click()" style="background-color: #28a745; float: right; margin: 10px;">Upload Settings</button>
  <input type="file" id="settingsFileInput" style="display: none;" accept=".json" onchange="uploadSettings(this.files)">
  <div style="clear:both;"></div> <!-- Clear float -->
  <!-- ============================ -->

  <!-- == Keyboard Shortcuts == -->
  <p style="font-size: 0.8em; color: #aaa;">Keyboard Shortcuts: [H] Home All, [N] Next PnP Step (when active)</p>
  <hr>

  <!-- == Debug Section == -->
  <button onclick="document.getElementById('debug-section').style.display = 'block';" style="margin-top: 10px; font-size: 12px;">Show Debug Log</button>
  <div id="debug-section" style="margin-top: 20px; text-align: left; border: 1px solid #444; background-color: #333; display: none;">
    <div style="padding: 5px; background-color: #222; border-bottom: 1px solid #444;">
      <button onclick="document.getElementById('debug-section').style.display = 'none';">Hide Debug</button>
      <button onclick="document.getElementById('debug-log').innerHTML = '';">Clear Log</button>
      <button onclick="initWebSocket();" style="background-color: #00bf4f;">Force Reconnect</button>
      <button onclick="testConnection();" style="background-color: #4b9fe3;">Test Connection</button>
      <label><input type="checkbox" id="debug-enabled" checked> Enable Logging</label>
    </div>
    <div style="padding: 5px; background-color: #333; border-bottom: 1px solid #444;">
      <input type="text" id="manual-command" placeholder="Type command here (e.g., HOME)" style="width: 300px; padding: 5px;">
      <button onclick="sendCommand(document.getElementById('manual-command').value);" style="background-color: #4CAF50;">Send Command</button>
    </div>
    <div id="debug-log" style="padding: 10px; white-space: pre; font-family: monospace; height: 200px; overflow-y: scroll; font-size: 12px;"></div>
  </div>

  <!-- == Paint Start Positions Section (NEW) == -->
  <div class="section-paint-starts">
    <details>
        <summary style="cursor: pointer; font-weight: bold;">Paint Start Positions &#9662;</summary>
        <div style="padding-left: 20px;">
          <p>Define the starting X, Y coordinates (inches) for each painting side. Used for both Up/Down and Sideways patterns.</p>
          
          <!-- Back Side Start (Side 0) -->
          <div class="input-group-start">
            <h4>Back (Side 0)</h4>
            <label for="paintStartX_0">Start X:</label>
            <input type="number" id="paintStartX_0" step="0.1" value="11.5"> <!-- Default example -->
            <label for="paintStartY_0">Start Y:</label>
            <input type="number" id="paintStartY_0" step="0.1" value="20.5"> <!-- Default example -->
          </div>

          <!-- Right Side Start (Side 1) -->
          <div class="input-group-start">
            <h4>Right (Side 1)</h4>
            <label for="paintStartX_1">Start X:</label>
            <input type="number" id="paintStartX_1" step="0.1" value="29.5"> <!-- Default from code -->
            <label for="paintStartY_1">Start Y:</label>
            <input type="number" id="paintStartY_1" step="0.1" value="20.0"> <!-- Default from code -->
          </div>
          
          <!-- Front Side Start (Side 2) -->
          <div class="input-group-start">
            <h4>Front (Side 2)</h4>
            <label for="paintStartX_2">Start X:</label>
            <input type="number" id="paintStartX_2" step="0.1" value="11.5"> <!-- Default from code -->
            <label for="paintStartY_2">Start Y:</label>
            <input type="number" id="paintStartY_2" step="0.1" value="0.5"> <!-- Default from code -->
          </div>

          <!-- Left Side Start (Side 3) -->
          <div class="input-group-start">
            <h4>Left (Side 3)</h4>
            <label for="paintStartX_3">Start X:</label>
            <input type="number" id="paintStartX_3" step="0.1" value="8.0"> <!-- Default from code -->
            <label for="paintStartY_3">Start Y:</label>
            <input type="number" id="paintStartY_3" step="0.1" value="6.5"> <!-- Default from code -->
          </div>

          <button class="button setting-button" onclick="setPaintStartPositions()" style="margin-top: 15px;">Save Start Positions</button>
        </div>
    </details>
  </div>
  <hr style="clear:both;">
  <!-- ========================= -->

  <script>
    // Hard-code IP address to ensure connection works
    var gateway = "ws://192.168.1.249:81/"; // Use IP instead of hostname for reliability
    var websocket;
    var statusDiv = document.getElementById('status');
    var connectionIndicatorSpan = document.getElementById('connectionIndicator');
    var debugLog = document.getElementById('debug-log');
    var debugEnabled = document.getElementById('debug-enabled');
    
    // Initialize state variables
    var isMoving = false;
    var isHoming = false;
    var allHomed = false;
    var inPickPlaceMode = false;
    var inCalibrationMode = false;
    var stopRequested = false;
    var isPressurized = false; // New state variable for pressure pot
    
    // UI element references
    var homeButton = document.getElementById('homeButton');
    var gotoButton = document.getElementById('gotoButton');
    var gotoButton2020 = document.getElementById('gotoButton2020');
    var pnpButton = document.getElementById('pnpButton');
    var pnpStepButton = document.getElementById('pnpStepButton');
    var pnpSkipButton = document.getElementById('pnpSkipButton');
    var pnpBackButton = document.getElementById('pnpBackButton');
    var calibrationControlsDiv = document.getElementById('calibrationControls');
    var enterCalibrationButton = document.getElementById('enterCalibrationButton');
    var jogStepInput = document.getElementById('jogStep');
    var currentPosDisplaySpan = document.getElementById('currentPosDisplay');
    var stopButton = document.querySelector('.exit-button');
    var pressurizeButton = document.getElementById('pressurizeButton');

    // Paint control elements
    var paintBackButton = document.getElementById('paintBackButton');
    var rotateMinus90Button = document.getElementById('rotateMinus90Button');
    var rotatePlus90Button = document.getElementById('rotatePlus90Button');
    var rotateMinus5Button = document.getElementById('rotateMinus5Button');
    var rotatePlus5Button = document.getElementById('rotatePlus5Button');
    var setRotationZeroButton = document.getElementById('setRotationZeroButton');
    
    // Settings inputs
    var offsetXInput = document.getElementById('offsetX');
    var offsetYInput = document.getElementById('offsetY');
    var firstPlaceXAbsInput = document.getElementById('firstPlaceXAbs');
    var firstPlaceYAbsInput = document.getElementById('firstPlaceYAbs');
    var xSpeedInput = document.getElementById('patternXSpeed');
    var ySpeedInput = document.getElementById('patternYSpeed');
    var gridColsInput = document.getElementById('gridCols');
    var gridRowsInput = document.getElementById('gridRows');
    var spacingDisplaySpan = document.getElementById('spacingDisplay');
    var trayWidthInput = document.getElementById('trayWidth');
    var trayHeightInput = document.getElementById('trayHeight');
    var paintGunOffsetXInput = document.getElementById('paintGunOffsetX');
    var paintGunOffsetYInput = document.getElementById('paintGunOffsetY');

    // Paint side settings arrays
    var paintZInputs = [
      document.getElementById('paintZ_0'),
      document.getElementById('paintZ_1'),
      document.getElementById('paintZ_2'),
      document.getElementById('paintZ_3')
    ];
    var paintPInputs = [
      document.getElementById('paintP_0'),
      document.getElementById('paintP_1'),
      document.getElementById('paintP_2'),
      document.getElementById('paintP_3')
    ];
    var paintRInputs = [
      document.getElementById('paintR_0'),
      document.getElementById('paintR_1'),
      document.getElementById('paintR_2'),
      document.getElementById('paintR_3')
    ];
    var paintSInputs = [
      document.getElementById('paintS_0'),
      document.getElementById('paintS_1'),
      document.getElementById('paintS_2'),
      document.getElementById('paintS_3')
    ];
    var paintSDisplays = [
      document.getElementById('paintSDisplay_0'),
      document.getElementById('paintSDisplay_1'),
      document.getElementById('paintSDisplay_2'),
      document.getElementById('paintSDisplay_3')
    ];
    
    // Not all paintZ/paintP displays may exist - check first
    paintZDisplays = [];
    paintPDisplays = [];
    for(let i = 0; i < 4; i++) {
        paintZDisplays.push(document.getElementById(`paintZDisplay_${i}`));
        paintPDisplays.push(document.getElementById(`paintPDisplay_${i}`));
    }
    
    // Now that we've initialized everything, start the WebSocket connection
    initWebSocket();
    
    // Set up keyboard event listener
    document.addEventListener('keydown', function(event) {
        // Existing keyboard event handler code...
    });

    function addDebug(message) {
      if (debugEnabled && debugEnabled.checked) {
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = `[${timestamp}] ${message}`;
        console.log(logEntry);
        
        if (debugLog) {
          // Add new message
          debugLog.innerHTML += logEntry + '<br>';
          
          // Limit the log size to 100 entries
          const entries = debugLog.innerHTML.split('<br>');
          if (entries.length > 100) {
            debugLog.innerHTML = entries.slice(entries.length - 100).join('<br>');
          }
          
          // Auto-scroll to bottom
          debugLog.scrollTop = debugLog.scrollHeight;
        }
      }
    }
    
    function testConnection() {
      addDebug("Testing WebSocket connection...");
      
      if (!websocket || websocket.readyState !== WebSocket.OPEN) {
        addDebug("ERROR: WebSocket is not connected! Current state: " + 
                (websocket ? ["CONNECTING", "OPEN", "CLOSING", "CLOSED"][websocket.readyState] : "null"));
        document.getElementById('debug-section').style.display = 'block';
        return;
      }
      
      try {
        websocket.send("PING");
        addDebug("PING sent - check for response in server logs");
        
        // Show connection details
        addDebug(`Connection details:`);
        addDebug(`- URL: ${websocket.url}`);
        addDebug(`- Protocol: ${websocket.protocol || "none"}`);
        addDebug(`- State: ${["CONNECTING", "OPEN", "CLOSING", "CLOSED"][websocket.readyState]}`);
        addDebug(`- Buffer amount: ${websocket.bufferedAmount}`);
      } catch (e) {
        addDebug("ERROR sending test message: " + e.message);
        document.getElementById('debug-section').style.display = 'block';
      }
    }

    var connectionTimeout = null;
    function initWebSocket() {
      // Clear any existing timeout
      if (connectionTimeout) {
        clearTimeout(connectionTimeout);
        connectionTimeout = null;
      }

      addDebug('Trying to open a WebSocket connection...');
      console.log('Trying to open a WebSocket connection to: ' + gateway);
      document.getElementById('debug-section').style.display = 'block'; // Show debug section on connection attempt
      statusDiv.innerHTML = 'Connecting...';
      statusDiv.style.color = 'orange';
      
      try {
        websocket = new WebSocket(gateway);
        
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onmessage = onMessage;
        websocket.onerror = onError;
        
        // Set a timeout in case the connection attempt hangs
        setTimeout(function() {
          if (websocket.readyState !== WebSocket.OPEN) {
            addDebug('Connection attempt timed out. Retrying...');
            websocket.close();
          }
        }, 5000);
      } catch (e) {
        addDebug('Exception creating WebSocket: ' + e.message);
        statusDiv.innerHTML = 'Error creating WebSocket';
        statusDiv.style.color = 'red';
        setTimeout(initWebSocket, 5000); // Try again in 5 seconds
      }
    }

    function onOpen(event) {
      addDebug('Connection opened successfully!');
      console.log('Connection opened');
      statusDiv.innerHTML = 'Connected';
      statusDiv.style.color = 'green';
      connectionIndicatorSpan.innerHTML = ''; // Clear indicator on connect
      
      // Update debug log with connection status
      addDebug('WebSocket State: ' + websocket.readyState + ' (OPEN=1)');
      addDebug('Sending GET_STATUS command...');
      
      sendCommand('GET_STATUS'); // Request current status from ESP32
      enableButtons(); // Enable/disable buttons based on initial state (will be updated by GET_STATUS response)
    }

    function onClose(event) {
      addDebug('Connection closed. Code: ' + event.code + ', Reason: ' + event.reason);
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
      
      // Try to reconnect after a delay, with exponential backoff
      setTimeout(initWebSocket, 2000); // Try to reconnect every 2 seconds
    }

    function onMessage(event) {
        addDebug('Received message: ' + event.data);
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
            
            // Check for pressure state if available
            if (data.hasOwnProperty('pressurized')) {
                isPressurized = data.pressurized;
                updatePressurizeButtonUI();
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
            if (data.hasOwnProperty('patXSpeed') && data.hasOwnProperty('patYSpeed')) {
                // Divide received actual values (e.g., 20000) by 1000 for display (e.g., 20)
                let xS_display = parseInt(data.patXSpeed) / 1000;
                let yS_display = parseInt(data.patYSpeed) / 1000;
                // xSpeedDisplaySpan.innerHTML = `${xS_display}`;
                // ySpeedDisplaySpan.innerHTML = `${yS_display}`;
                xSpeedInput.value = xS_display;
                ySpeedInput.value = yS_display;
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
                pnpStepButton.style.display = 'none'; // Hide PnP Step button
                pnpSkipButton.style.display = 'none'; // Hide Skip button
                pnpBackButton.style.display = 'none'; // Hide Back button
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
                pnpStepButton.style.display = 'inline-block'; // Show PnP Step button
                pnpSkipButton.style.display = 'inline-block'; // Show Skip button
                pnpBackButton.style.display = 'inline-block'; // Show Back button
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
                pnpStepButton.style.display = 'none'; // Hide PnP Step button
                pnpSkipButton.style.display = 'none'; // Hide Skip button
                pnpBackButton.style.display = 'none'; // Hide Back button
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
                pnpStepButton.style.display = 'none'; // Hide PnP Step button
                pnpSkipButton.style.display = 'none'; // Hide Skip button
                pnpBackButton.style.display = 'none'; // Hide Back button
                calibrationControlsDiv.style.display = 'none'; // Hide Cal controls
            }

            // --- Final step: Update all button states based on the latest JS state variables ---
            // Determine if PnP mode is active based on statusDiv content or pnpButton text
            let inPnPMode = statusDiv.innerHTML.includes('PickPlaceReady') || pnpButton.innerHTML.includes('Exit');
            console.log(`JS Debug: Before final enableButtons: isMoving=${isMoving}, isHoming=${isHoming}, inPnPMode=${inPnPMode}`); // JS Debug
            enableButtons(); // ADDED single call here

            // NEW: Update Paint Start Position fields
            // Check if data.settings exists before trying to access it
            if (data.settings) { 
              if (data.settings.paintStartX_0 !== undefined) document.getElementById('paintStartX_0').value = data.settings.paintStartX_0;
              if (data.settings.paintStartY_0 !== undefined) document.getElementById('paintStartY_0').value = data.settings.paintStartY_0;
              if (data.settings.paintStartX_1 !== undefined) document.getElementById('paintStartX_1').value = data.settings.paintStartX_1;
              if (data.settings.paintStartY_1 !== undefined) document.getElementById('paintStartY_1').value = data.settings.paintStartY_1;
              if (data.settings.paintStartX_2 !== undefined) document.getElementById('paintStartX_2').value = data.settings.paintStartX_2;
              if (data.settings.paintStartY_2 !== undefined) document.getElementById('paintStartY_2').value = data.settings.paintStartY_2;
              if (data.settings.paintStartX_3 !== undefined) document.getElementById('paintStartX_3').value = data.settings.paintStartX_3;
              if (data.settings.paintStartY_3 !== undefined) document.getElementById('paintStartY_3').value = data.settings.paintStartY_3;
            } // End check for data.settings

        } catch (e) {
            console.error("Error processing WebSocket message:", e); // Added error logging
            statusDiv.innerHTML = `Received raw: ${event.data}`; // Show raw data if JSON parse fails
            statusDiv.style.color = 'blue';
            enableButtons(); // Also update buttons on error, might be disconnected state
        }
    }

     function onError(event) {
        addDebug(`WebSocket Error: ${JSON.stringify(event)}`);
        console.error("WebSocket Error: ", event);
        statusDiv.innerHTML = 'WebSocket Error';
        statusDiv.style.color = 'red';
        connectionIndicatorSpan.innerHTML = ' (Error)'; // Show indicator on error
        enterCalibrationButton.style.display = 'inline-block'; // Ensure Enter Cal button is visible on error/disconnect
        calibrationControlsDiv.style.display = 'none'; // Ensure Cal controls are hidden on error/disconnect
        
        // Force close and reconnect on error
        if (websocket) {
          try {
            websocket.close();
          } catch (e) {
            addDebug('Error closing websocket: ' + e.message);
          }
        }
        
        setTimeout(initWebSocket, 3000); // Try to reconnect after error
        enableButtons(); // Update buttons on error
     }

    function sendCommand(command) {
      if (!websocket || websocket.readyState !== WebSocket.OPEN) {
        addDebug("ERROR: WebSocket is not connected!");
        document.getElementById('debug-section').style.display = 'block';
        return false;
      }
      
      try {
        websocket.send(command);
        console.log(`Command sent: ${command}`);
        return true;
      } catch (e) {
        addDebug(`ERROR sending command: ${e.message}`);
        document.getElementById('debug-section').style.display = 'block';
        return false;
      }
    }

    function enableButtons() {
      let connected = websocket && websocket.readyState === WebSocket.OPEN;
      // Add more detailed logging here
      let inCalibMode = calibrationControlsDiv.style.display === 'block';
      let inPickAndPlaceMode = pnpStepButton.style.display === 'inline-block';
      console.log(`[enableButtons] States: connected=${connected}, allHomed=${allHomed}, isMoving=${isMoving}, isHoming=${isHoming}, inCalib=${inCalibMode}, inPnP=${inPickAndPlaceMode}`);

      let canStop = connected && (isMoving || isHoming || inCalibMode);

      homeButton.disabled = !connected || isMoving || isHoming;
      console.log(`  -> homeButton.disabled = ${homeButton.disabled} (!${connected} || ${isMoving} || ${isHoming})`);
      
      stopButton.disabled = !canStop;
      console.log(`  -> stopButton.disabled = ${stopButton.disabled} (!${canStop})`);

      // PnP Button Logic - UPDATED
      pnpButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode || statusDiv.innerHTML.includes('PickPlaceComplete');
      console.log(`  -> pnpButton.disabled = ${pnpButton.disabled} (!${connected} || !${allHomed} || ${isMoving} || ${isHoming} || ${inCalibMode} || ${statusDiv.innerHTML.includes('PickPlaceComplete')})`);

      // PnP Step/Skip/Back Button Logic (Only enabled when in PnP mode and NOT moving/homing)
      pnpStepButton.disabled = !(connected && inPickAndPlaceMode && !isMoving && !isHoming);
      console.log(`  -> pnpStepButton.disabled = ${pnpStepButton.disabled} (!( ${connected} && ${inPickAndPlaceMode} && !${isMoving} && !${isHoming} ))`);
      pnpSkipButton.disabled = pnpStepButton.disabled; 
      pnpBackButton.disabled = pnpStepButton.disabled; 

      // Calibration Button Logic
      enterCalibrationButton.disabled = !connected || !allHomed || isMoving || isHoming || inCalibMode || inPickAndPlaceMode;
      console.log(`  -> enterCalibrationButton.disabled = ${enterCalibrationButton.disabled} (!${connected} || !${allHomed} || ${isMoving} || ${isHoming} || ${inCalibMode} || ${inPickAndPlaceMode})`);

      // --- Painting Buttons ---
      let paintDisableCondition = !connected || !allHomed || isMoving || isHoming || inCalibMode || inPickAndPlaceMode;
      paintBackButton.disabled = paintDisableCondition;
      document.getElementById('paintRightButton').disabled = paintDisableCondition;
      document.getElementById('paintFrontButton').disabled = paintDisableCondition;
      document.getElementById('paintLeftButton').disabled = paintDisableCondition;
      console.log(`  -> Paint Buttons Disabled = ${paintDisableCondition}`);

      // Rotation Buttons
      let rotateDisableCondition = !connected || isMoving || isHoming;
      rotateMinus90Button.disabled = rotateDisableCondition;
      rotatePlus90Button.disabled = rotateDisableCondition;
      rotateMinus5Button.disabled = rotateDisableCondition;
      rotatePlus5Button.disabled = rotateDisableCondition;
      setRotationZeroButton.disabled = rotateDisableCondition;
      console.log(`  -> Rotation Buttons Disabled = ${rotateDisableCondition}`);

      // --- Settings Buttons/Inputs ---
      let generalDisableCondition = !connected || isMoving || isHoming || inCalibMode;
      let settingsInputsDisableCondition = generalDisableCondition || !allHomed; // Most settings need homing

      offsetXInput.disabled = settingsInputsDisableCondition;
      offsetYInput.disabled = settingsInputsDisableCondition;
      firstPlaceXAbsInput.disabled = settingsInputsDisableCondition;
      firstPlaceYAbsInput.disabled = settingsInputsDisableCondition;
      xSpeedInput.disabled = settingsInputsDisableCondition;
      ySpeedInput.disabled = settingsInputsDisableCondition;
      paintGunOffsetXInput.disabled = settingsInputsDisableCondition;
      paintGunOffsetYInput.disabled = settingsInputsDisableCondition;
      console.log(`  -> General Settings Inputs Disabled = ${settingsInputsDisableCondition}`);

      // Grid/Tray inputs are linked to their buttons (only disabled if disconnected)
      gridColsInput.disabled = !connected;
      gridRowsInput.disabled = !connected;
      trayWidthInput.disabled = !connected;
      trayHeightInput.disabled = !connected;
      console.log(`  -> Grid/Tray Inputs Disabled = ${!connected}`);

      // Calibration Controls (only enabled when in calibration mode and not moving/homing)
      let calibControlsDisable = !connected || !inCalibMode || isMoving || isHoming;
      let calibInputs = calibrationControlsDiv.querySelectorAll('input, button');
      calibInputs.forEach(el => {
          el.disabled = calibControlsDisable;
      });
      console.log(`  -> Calibration Controls Disabled = ${calibControlsDisable}`);

      // Download/Upload Buttons (only need connection)
      document.getElementById('downloadSettingsButton').disabled = !connected;
      document.getElementById('uploadSettingsButton').disabled = !connected;
      console.log(`  -> Download/Upload Buttons Disabled = ${!connected}`);

      // Specific overrides 
      if (pnpButton.innerHTML.includes('Complete')) {
           pnpButton.disabled = true;
           console.log("  -> OVERRIDE: pnpButton disabled because sequence complete.");
      }
  }

  // Replace the simple load event listener with a proper initialization function
  function initializeApp() {
      // Re-query all UI elements to ensure they're properly loaded
      statusDiv = document.getElementById('status');
      connectionIndicatorSpan = document.getElementById('connectionIndicator');
      debugLog = document.getElementById('debug-log');
      debugEnabled = document.getElementById('debug-enabled');
      
      // Re-initialize UI references
      homeButton = document.getElementById('homeButton');
      gotoButton = document.getElementById('gotoButton');
      gotoButton2020 = document.getElementById('gotoButton2020');
      pnpButton = document.getElementById('pnpButton');
      pnpStepButton = document.getElementById('pnpStepButton');
      pnpSkipButton = document.getElementById('pnpSkipButton');
      pnpBackButton = document.getElementById('pnpBackButton');
      calibrationControlsDiv = document.getElementById('calibrationControls');
      enterCalibrationButton = document.getElementById('enterCalibrationButton');
      jogStepInput = document.getElementById('jogStep');
      currentPosDisplaySpan = document.getElementById('currentPosDisplay');
      stopButton = document.querySelector('.exit-button');
      pressurizeButton = document.getElementById('pressurizeButton'); // Add pressurize button reference

      // Paint control elements
      paintBackButton = document.getElementById('paintBackButton');
      rotateMinus90Button = document.getElementById('rotateMinus90Button');
      rotatePlus90Button = document.getElementById('rotatePlus90Button');
      rotateMinus5Button = document.getElementById('rotateMinus5Button');
      rotatePlus5Button = document.getElementById('rotatePlus5Button');
      setRotationZeroButton = document.getElementById('setRotationZeroButton');
      
      // Settings inputs
      offsetXInput = document.getElementById('offsetX');
      offsetYInput = document.getElementById('offsetY');
      firstPlaceXAbsInput = document.getElementById('firstPlaceXAbs');
      firstPlaceYAbsInput = document.getElementById('firstPlaceYAbs');
      xSpeedInput = document.getElementById('patternXSpeed');
      ySpeedInput = document.getElementById('patternYSpeed');
      gridColsInput = document.getElementById('gridCols');
      gridRowsInput = document.getElementById('gridRows');
      spacingDisplaySpan = document.getElementById('spacingDisplay');
      trayWidthInput = document.getElementById('trayWidth');
      trayHeightInput = document.getElementById('trayHeight');
      paintGunOffsetXInput = document.getElementById('paintGunOffsetX');
      paintGunOffsetYInput = document.getElementById('paintGunOffsetY');

      // Paint side settings arrays
      paintZInputs = [
          document.getElementById('paintZ_0'),
          document.getElementById('paintZ_1'),
          document.getElementById('paintZ_2'),
          document.getElementById('paintZ_3')
      ];
      paintPInputs = [
          document.getElementById('paintP_0'),
          document.getElementById('paintP_1'),
          document.getElementById('paintP_2'),
          document.getElementById('paintP_3')
      ];
      paintRInputs = [
          document.getElementById('paintR_0'),
          document.getElementById('paintR_1'),
          document.getElementById('paintR_2'),
          document.getElementById('paintR_3')
      ];
      paintSInputs = [
          document.getElementById('paintS_0'),
          document.getElementById('paintS_1'),
          document.getElementById('paintS_2'),
          document.getElementById('paintS_3')
      ];
      paintSDisplays = [
          document.getElementById('paintSDisplay_0'),
          document.getElementById('paintSDisplay_1'),
          document.getElementById('paintSDisplay_2'),
          document.getElementById('paintSDisplay_3')
      ];
      
      // Not all paintZ/paintP displays may exist - check first
      paintZDisplays = [];
      paintPDisplays = [];
      for(let i = 0; i < 4; i++) {
          paintZDisplays.push(document.getElementById(`paintZDisplay_${i}`));
          paintPDisplays.push(document.getElementById(`paintPDisplay_${i}`));
      }
      
      // Now that we've initialized everything, start the WebSocket connection
      initWebSocket();
      
      // Set up keyboard event listener
      document.addEventListener('keydown', function(event) {
          // Existing keyboard event handler code...
      });
  }

  window.addEventListener('load', initializeApp);
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

  function sendSpeedSettings() { // Renamed
      const xs_k = document.getElementById('patternXSpeed').value;
      const ys_k = document.getElementById('patternYSpeed').value;
      // Convert from k steps/s to steps/s
      const xs = parseFloat(xs_k) * 1000;
      const ys = parseFloat(ys_k) * 1000;
      if (!isNaN(xs) && !isNaN(ys) && xs > 0 && ys > 0) {
          // Send actual steps/s values (only speeds)
          sendCommand(`SET_PNP_SPEEDS ${xs} ${ys}`); // Use a new command name
      } else {
          alert("Invalid speed values. Must be positive numbers.");
      }
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
              ySpeed: parseFloat(ySpeedInput.value) || 0
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
                  ySpeedInput.value = settings.patternSpeed.ySpeed;
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

  function handleErrors() {
      addDebug("WebSocket error: connection failed");
      document.getElementById('debug-section').style.display = 'block';
      
      // Try to reconnect automatically after 5 seconds
      connectionTimeout = setTimeout(initWebSocket, 5000);
  }

  function handleDisconnect() {
      addDebug("WebSocket connection closed");
      document.getElementById('debug-section').style.display = 'block';
      
      // Try to reconnect automatically after 5 seconds if not a normal closure
      connectionTimeout = setTimeout(initWebSocket, 5000);
  }

  // --- NEW JavaScript Function ---
  function setPaintStartPositions() {
      const starts = {};
      starts.X0 = document.getElementById('paintStartX_0').value;
      starts.Y0 = document.getElementById('paintStartY_0').value;
      starts.X1 = document.getElementById('paintStartX_1').value;
      starts.Y1 = document.getElementById('paintStartY_1').value;
      starts.X2 = document.getElementById('paintStartX_2').value;
      starts.Y2 = document.getElementById('paintStartY_2').value;
      starts.X3 = document.getElementById('paintStartX_3').value;
      starts.Y3 = document.getElementById('paintStartY_3').value;

      // Basic validation (ensure numbers are entered, though type=number helps)
      for (const key in starts) {
          if (isNaN(parseFloat(starts[key]))) {
              setStatus('Invalid input for ' + key + '. Please enter numbers.', 'Error');
              return;
          }
      }

      const command = JSON.stringify({ command: "SET_PAINT_STARTS", data: starts });
      console.log("Sending command:", command);
      websocket.send(command);
      setStatus('Saving paint start positions...', 'Busy');
  }

  // Add the togglePressure function to handle the pressure button
  function togglePressure() {
      isPressurized = !isPressurized;
      updatePressurizeButtonUI();
      sendCommand('TOGGLE_PRESSURE');
  }
  
  // Function to update the pressurize button appearance
  function updatePressurizeButtonUI() {
      if (pressurizeButton) {
          if (isPressurized) {
              pressurizeButton.innerHTML = "Depressurize";
              pressurizeButton.style.backgroundColor = "#e91e63"; // Pink/red when pressurized
          } else {
              pressurizeButton.innerHTML = "Pressurize";
              pressurizeButton.style.backgroundColor = "#ff9800"; // Orange when not pressurized
          }
      }
  }

  // Function to set Paint Gun Offset
  function setPaintGunOffset() {
      const xOffset = parseFloat(paintGunOffsetXInput.value);
      const yOffset = parseFloat(paintGunOffsetYInput.value);
      const command = `SET_PAINT_GUN_OFFSET ${xOffset} ${yOffset}`;
      sendCommand(command);
      alert("Paint Gun Offset saved successfully!");
  }
  </script>
</body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H 