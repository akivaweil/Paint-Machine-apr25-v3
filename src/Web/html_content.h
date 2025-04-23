#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <Arduino.h>

const char HTML_PROGMEM[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Stepper Control - Test Page</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #222; color: #eee; } 
    .button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px;
              text-align: center; text-decoration: none; display: inline-block;
              font-size: 16px; margin: 10px; cursor: pointer; border-radius: 8px; }
    .button:active { transform: scale(0.95); background-color: #367c39; }
    .button:disabled { background-color: #cccccc; cursor: not-allowed; transform: none; }
    .exit-button { background-color: #f44336; }
    #status { margin-top: 20px; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Paint + Pick and Place Machine - Test Page</h1>
  <div id="main-controls">
    <h2>Basic Controls</h2>
    <button id="homeButton" class="button" onclick="sendCommand('HOME')">Home All Axes</button>
    <button id="paintButton" class="button" onclick="sendCommand('PAINT_SIDE_0')">Paint Back</button>
    <button id="cleanButton" class="button" onclick="sendCommand('CLEAN_GUN')">Clean Gun</button>
    <button id="pnpButton" class="button" onclick="sendCommand('ENTER_PICKPLACE')">Pick and Place Mode</button>
    <button id="stopButton" class="button exit-button" onclick="sendCommand('STOP')">STOP</button>
  </div>
  
  <div id="status">Connecting to ESP32...</div>
  <div id="debug" style="margin-top: 20px; text-align: left; padding: 10px; border: 1px solid #444; background-color: #333; white-space: pre; font-family: monospace;">Debug log will appear here</div>

  <script>
    // Simple WebSocket connection
    var gateway = `ws://${window.location.hostname}:81/`;
    var websocket;
    var statusDiv = document.getElementById('status');
    var debugDiv = document.getElementById('debug');
    var connected = false;
    
    function addDebugLog(message) {
      var timestamp = new Date().toLocaleTimeString();
      debugDiv.innerHTML = `[${timestamp}] ${message}\n` + debugDiv.innerHTML;
      if (debugDiv.innerHTML.length > 5000) {
        // Trim if too long
        debugDiv.innerHTML = debugDiv.innerHTML.substring(0, 5000) + "...";
      }
    }
    
    function initWebSocket() {
      addDebugLog('Trying to open a WebSocket connection...');
      statusDiv.innerHTML = 'Connecting...';
      statusDiv.style.color = 'orange';
      
      // Create WebSocket connection
      websocket = new WebSocket(gateway);
      
      websocket.onopen = function(event) {
        addDebugLog('Connection opened');
        statusDiv.innerHTML = 'Connected';
        statusDiv.style.color = 'green';
        connected = true;
        enableButtons(true);
        sendCommand('GET_STATUS');
      };
      
      websocket.onclose = function(event) {
        addDebugLog('Connection closed');
        statusDiv.innerHTML = 'Disconnected - Retrying...';
        statusDiv.style.color = 'red';
        connected = false;
        enableButtons(false);
        setTimeout(initWebSocket, 2000);
      };
      
      websocket.onmessage = function(event) {
        addDebugLog(`Received: ${event.data}`);
        try {
          const data = JSON.parse(event.data);
          statusDiv.innerHTML = `Status: ${data.status} - ${data.message}`;
        } catch (e) {
          addDebugLog(`Error parsing message: ${e.message}`);
          statusDiv.innerHTML = `Received: ${event.data}`;
        }
      };
      
      websocket.onerror = function(event) {
        addDebugLog(`WebSocket error: ${event}`);
        statusDiv.innerHTML = 'WebSocket Error';
        statusDiv.style.color = 'red';
      };
    }
    
    function sendCommand(command) {
      if (!connected) {
        addDebugLog('Cannot send command - not connected');
        return;
      }
      
      addDebugLog(`Sending command: ${command}`);
      websocket.send(command);
      statusDiv.innerHTML = `Command sent: ${command}`;
      statusDiv.style.color = 'blue';
    }
    
    function enableButtons(enable) {
      var buttons = document.querySelectorAll('.button');
      buttons.forEach(button => {
        button.disabled = !enable;
      });
    }
    
    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H 