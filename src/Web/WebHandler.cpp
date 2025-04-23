#include "WebHandler.h"
#include <WebServer.h>       // For WebServer
#include <WebSocketsServer.h> // For WebSocketsServer
#include <ArduinoJson.h>     // For JSON handling
#include <WiFi.h>            // For IPAddress
#include "../Main/html_content.h"   // For HTML_PROGMEM (Adjusted path)
#include "../Main/GeneralSettings_PinDef.h" // For constants like PITCH_SERVO_MIN/MAX (Adjusted path)
#include "../PickPlace/PickPlace.h" // For PnP functions like enterPickPlaceMode, skipPickPlaceLocation etc.
#include "../Painting/Painting.h" // For paintSide function

// --- Define Web Server and WebSocket Server Objects ---
WebServer webServer(80);
WebSocketsServer webSocket(81);

// --- Forward declarations for functions defined in main.cpp ---
// (These are declared extern in WebHandler.h, but needed here for the compiler)
// It might be cleaner to have a main.h eventually.
void homeAllAxes();
void moveToPositionInches(float targetX_inch, float targetY_inch, float targetZ_inch);
void calculateAndSetGridSpacing(int cols, int rows);
// Note: PickPlace and Painting functions are included via their headers above.

// --- Function Definitions ---

void handleRoot() {
  // Serial.println("Serving root page.");
  // webServer.send(200, "text/html", HTML_PROGMEM); // Original line - sends entire C string
  webServer.send_P(200, "text/html", HTML_PROGMEM); // Correct way to send PROGMEM content
}

// Setup function for the web server and WebSocket server
void setupWebServerAndWebSocket() {
    // Configure web server routes
    webServer.on("/", handleRoot);
    
    // Start the web server
    webServer.begin();
    Serial.println("[INFO] HTTP server started on port 80");
    
    // Start the WebSocket server
    webSocket.begin();
    // Use the webSocketEvent handler from main.cpp
    webSocket.onEvent(webSocketEvent);
    Serial.println("[INFO] WebSocket server started on port 81");
}

// Function to send current position updates via WebSocket
void sendCurrentPositionUpdate() {
    if (!allHomed) return; // Skip if not homed
    
    // Create a simple JSON response with current position
    char buffer[200];
    sprintf(buffer, "{\"position\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"rot\":%.2f}}",
            stepper_x ? (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY : 0.0f,
            stepper_y_left ? (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY : 0.0f,
            stepper_z ? (float)stepper_z->getCurrentPosition() / STEPS_PER_INCH_Z : 0.0f,
            stepper_rot ? (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE : 0.0f);
    
    // Broadcast the position to all connected clients
    webSocket.broadcastTXT(buffer);
}

// Function to send all settings to a client or broadcast to all clients
void sendAllSettingsUpdate(uint8_t specificClientNum, String message) {
    // Create JSON buffer with all settings
    char buffer[600]; // Make sure this is large enough
    
    // Format: Basic message + all current settings
    sprintf(buffer, 
        "{\"status\":\"Settings\",\"message\":\"%s\","
        "\"homed\":%s,"
        "\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,"
        "\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,"
        "\"gridCols\":%d,\"gridRows\":%d,"
        "\"gapX\":%.3f,\"gapY\":%.3f,"
        "\"trayWidth\":%.2f,\"trayHeight\":%.2f,"
        "\"patXSpeed\":%.0f,\"patYSpeed\":%.0f,"
        "\"paintGunOffsetX\":%.2f,\"paintGunOffsetY\":%.2f}",
        message.c_str(),
        allHomed ? "true" : "false",
        pnpOffsetX_inch, pnpOffsetY_inch,
        placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
        placeGridCols, placeGridRows,
        placeGapX_inch, placeGapY_inch,
        trayWidth_inch, trayHeight_inch,
        patternXSpeed, patternYSpeed,
        paintGunOffsetX_inch, paintGunOffsetY_inch);
    
    // Send to specific client or broadcast
    if (specificClientNum < 255) {
        webSocket.sendTXT(specificClientNum, buffer);
    } else {
        webSocket.broadcastTXT(buffer);
    }
}