#ifndef HTML_TEST_H
#define HTML_TEST_H

#include <Arduino.h>

const char HTML_TEST[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Test Page</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #222; color: #eee; }
  </style>
</head>
<body>
  <h1>Simple Test Page</h1>
  <p>If you can see this, basic HTML is working.</p>
  <script>
    window.onload = function() {
      document.body.innerHTML += '<p>JavaScript is also working!</p>';
      
      // Try a basic WebSocket connection
      try {
        var gateway = `ws://${window.location.hostname}:81/`;
        document.body.innerHTML += '<p>Attempting WebSocket connection to: ' + gateway + '</p>';
        
        var testSocket = new WebSocket(gateway);
        
        testSocket.onopen = function(event) {
          document.body.innerHTML += '<p>WebSocket connection successful!</p>';
        };
        
        testSocket.onerror = function(event) {
          document.body.innerHTML += '<p>WebSocket Error: ' + JSON.stringify(event) + '</p>';
        };
        
        testSocket.onclose = function(event) {
          document.body.innerHTML += '<p>WebSocket connection closed.</p>';
        };
      } catch(e) {
        document.body.innerHTML += '<p>Exception in WebSocket setup: ' + e.message + '</p>';
      }
    };
  </script>
</body>
</html>
)rawliteral";

#endif // HTML_TEST_H 