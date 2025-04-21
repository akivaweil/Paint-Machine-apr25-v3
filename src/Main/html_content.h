#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

#include <Arduino.h>

const char HTML_PROGMEM[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Stepper Control - Debug</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #222; color: #eee; }
    .error { color: red; font-weight: bold; }
    .success { color: green; font-weight: bold; }
  </style>
</head>
<body>
  <h1>ESP32 Stepper Control - Debug Mode</h1>
  <div id="debug">Loading...</div>

    <script>
    // Wrap all code in try-catch to catch any syntax errors
    try {
      var debugDiv = document.getElementById('debug');
      debugDiv.innerHTML = '<p class="success">Basic JavaScript is working!</p>';
      
      // Try WebSocket connection
      debugDiv.innerHTML += '<p>Attempting WebSocket connection...</p>';
      
      var gateway = `ws://${window.location.hostname}:81/`;
      var websocket;

      function initWebSocket() {
        console.log('Trying to open a WebSocket connection to ' + gateway);
        debugDiv.innerHTML += '<p>Creating WebSocket to: ' + gateway + '</p>';
        
        websocket = new WebSocket(gateway);
        websocket.onopen = function(event) {
          debugDiv.innerHTML += '<p class="success">WebSocket connection opened!</p>';
          websocket.send("GET_STATUS");
        };
        
        websocket.onclose = function(event) {
          debugDiv.innerHTML += '<p class="error">WebSocket connection closed! Code: ' + event.code + '</p>';
          setTimeout(initWebSocket, 2000);
        };
        
        websocket.onerror = function(event) {
          debugDiv.innerHTML += '<p class="error">WebSocket error! Details logged to console.</p>';
          console.error("WebSocket Error: ", event);
        };
        
        websocket.onmessage = function(event) {
          debugDiv.innerHTML += '<p>Received: ' + event.data + '</p>';
          console.log('Received: ', event.data);
          try {
              const data = JSON.parse(event.data);
            debugDiv.innerHTML += '<p class="success">Successfully parsed JSON response!</p>';
          } catch (e) {
            debugDiv.innerHTML += '<p class="error">Error parsing JSON: ' + e.message + '</p>';
          }
        };
      }
      
      // Initialize WebSocket connection when page loads
      window.addEventListener('load', initWebSocket);
      
      // Add a button to restart the connection
      var restartButton = document.createElement('button');
      restartButton.innerHTML = 'Restart WebSocket Connection';
      restartButton.onclick = function() {
        if (websocket && websocket.readyState !== WebSocket.CLOSED) {
          websocket.close();
        }
        initWebSocket();
      };
      document.body.appendChild(restartButton);
      
      // Add a button to request status
      var statusButton = document.createElement('button');
      statusButton.innerHTML = 'Request Status';
      statusButton.onclick = function() {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
          websocket.send("GET_STATUS");
          debugDiv.innerHTML += '<p>Sent: GET_STATUS</p>';
        } else {
          debugDiv.innerHTML += '<p class="error">WebSocket not connected!</p>';
        }
      };
      document.body.appendChild(statusButton);
      
    } catch (error) {
      // Catch any syntax errors or exceptions
      document.body.innerHTML = '<h1>JavaScript Error</h1><p class="error">' + error.message + '</p>';
      console.error("JavaScript Error: ", error);
    }
    </script>
  </body>
</html>
)rawliteral";

#endif // HTML_CONTENT_H 