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
  webServer.send(200, "text/html", HTML_PROGMEM); // Use HTML_PROGMEM from html_content.h
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            
            // ADDED: Log values before sending update
            Serial.println("[DEBUG] Values before sendAllSettingsUpdate on connect:");
            for (int i = 0; i < 4; ++i) {
                 Serial.printf("  Side %d: Z=%.2f, P=%d, Pat=%d, S=%.0f\n", 
                      i, paintZHeight_inch[i], paintPitchAngle[i], paintPatternType[i], paintSpeed[i]);
            }
            
            // Send initial status and all settings using the helper function
            String welcomeMsg = "Welcome! Connected to Paint + PnP Machine.";
            sendAllSettingsUpdate(num, welcomeMsg);
            
            // Also send initial position update immediately after settings if homed
            if (allHomed) {
                 sendCurrentPositionUpdate();
             }
            }
            break;
        case WStype_TEXT:
            // Serial.printf("[%u] get Text: %s\n", num, payload); // Reduced verbosity
            if (length > 0) {
                String command = String((char*)payload);
                Serial.printf("[DEBUG] WebSocket [%u] Received Command: %s\n", num, command.c_str()); // DEBUG
                
                // --- NEW: Request for current status ---
                if (command == "GET_STATUS") { 
                    Serial.printf("[%u] WebSocket Received: GET_STATUS request.\n", num);
                    sendAllSettingsUpdate(num, "Current status requested"); // Send update only to requesting client
                }
                // --- End NEW ---
                // --- STOP Command Handler --- << ADDED
                else if (command == "STOP") {
                    Serial.println("WebSocket: Received STOP command.");
                    // Force stop all motors individually
                    if(stepper_x) stepper_x->forceStop();
                    if(stepper_y_left) stepper_y_left->forceStop();
                    if(stepper_y_right) stepper_y_right->forceStop();
                    if(stepper_z) stepper_z->forceStop();
                    if(stepper_rot) stepper_rot->forceStop(); // Check if stepper_rot exists before stopping
                    Serial.println("[DEBUG] All motors force stopped.");

                    // Reset state flags
                    isMoving = false; 
                    isHoming = false; // Prevent conflicts if homing was interrupted
                    inPickPlaceMode = false; // Exit PnP mode if active
                    inCalibrationMode = false; // Exit Calibration mode if active
                    stopRequested = true; // Set the flag for any loops that might still check it briefly

                    // Send immediate feedback
                    webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"STOP initiated. Homing axes...\"}"); 
                    
                    // Initiate Homing Sequence
                    homeAllAxes(); // This will set isHoming = true and send Homing status updates

                    // After homing, move to (0,0,0)
                    moveToPositionInches(0.0, 0.0, 0.0);
                    // Wait for all axes to finish moving
                    while ((stepper_x && stepper_x->isRunning()) || (stepper_y_left && stepper_y_left->isRunning()) || (stepper_y_right && stepper_y_right->isRunning()) || (stepper_z && stepper_z->isRunning())) {
                        webSocket.loop();
                        yield();
                    }
                    isMoving = false;
                    webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"STOP complete. Machine returned to zero.\"}");
                }
                // --- End STOP Command Handler ---
                
                else if (command == "HOME") { // Changed to else if
                    Serial.println("WebSocket: Received HOME command.");
                    if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot home, machine is busy.\"}");
                    } else {
                        // REMOVED block that checked inPickPlaceMode here.
                        // homeAllAxes() function will handle resetting PnP/Calibration modes internally.
                        Serial.println("[DEBUG] HOME command received while idle. Calling homeAllAxes()."); // Added log
                        homeAllAxes(); // Start homing (this function sends its own status updates and resets modes)
                    }
                } else if (command == "GOTO_5_5_0") {
                    Serial.println("WebSocket: Received GOTO_5_5_0 command.");
                    moveToPositionInches(5.0, 5.0, 0.0); // This function now sends its own status updates
                } else if (command == "GOTO_20_20_0") {
                    Serial.println("WebSocket: Received GOTO_20_20_0 command.");
                    moveToPositionInches(20.0, 20.0, 0.0); // This function now sends its own status updates
                } else if (command == "ENTER_PICKPLACE") {
                     Serial.println("WebSocket: Received ENTER_PICKPLACE command.");
                     if (!allHomed) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Not homed."); // DEBUG
                         // Fixed quotes
                         webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Machine not homed.\"}");
                     } else if (isMoving || isHoming) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Busy."); // DEBUG
                         // Fixed quotes
                         webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Machine is busy.\"}");
                     } else if (inPickPlaceMode) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: Already in PnP mode."); // DEBUG
                         // Exit command should be used instead
                         webSocket.sendTXT(num, "{\"status\":\"PickPlaceReady\", \"message\":\"Already in Pick/Place mode. Use Exit button.\"}");
                     } else if (inCalibrationMode) {
                         Serial.println("[DEBUG] ENTER_PICKPLACE denied: In Calibration mode."); // DEBUG
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Exit calibration before entering PnP mode.\"}");
                     } else {
                         // Call the function defined in PickPlace.h
                         enterPickPlaceMode();
                     }
                 } else if (command == "EXIT_PICKPLACE") {
                     Serial.println("WebSocket: Received EXIT_PICKPLACE command.");
                     exitPickPlaceMode(true); // Call function from PickPlace.h, REQUEST homing after exit
                 } else if (command == "PNP_NEXT_STEP") { // Renamed command
                    Serial.println("WebSocket: Received PNP_NEXT_STEP command.");
                    executeNextPickPlaceStep(); // Call function from PickPlace.h
                 } else if (command == "PNP_SKIP_LOCATION") { // ADDED
                    Serial.println("WebSocket: Received PNP_SKIP_LOCATION command.");
                    skipPickPlaceLocation(); // Call function from PickPlace.h
                 } else if (command == "PNP_BACK_LOCATION") { // ADDED
                    Serial.println("WebSocket: Received PNP_BACK_LOCATION command.");
                    goBackPickPlaceLocation(); // Call function from PickPlace.h
                 } else if (command.startsWith("SET_PNP_OFFSET ")) {
                     Serial.println("WebSocket: Received SET_PNP_OFFSET command.");
                     if (isMoving || isHoming || inPickPlaceMode) {
                         Serial.println("[DEBUG] SET_PNP_OFFSET denied: Machine busy or in PnP mode.");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set offset while machine is busy or in PnP mode.\"}");
                     } else {
                         float newX, newY;
                         // Use C-style string for sscanf
                         int parsed = sscanf(command.c_str() + strlen("SET_PNP_OFFSET "), "%f %f", &newX, &newY);
                         if (parsed == 2) {
                             pnpOffsetX_inch = newX;
                             pnpOffsetY_inch = newY;
                             Serial.printf("[DEBUG] Set PnP Offset to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch);
                             // Send confirmation with all current settings
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"PnP offset updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Failed to parse SET_PNP_OFFSET values.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_PNP_OFFSET. Use: SET_PNP_OFFSET X Y\"}");
                         }
                     }
                 } else if (command.startsWith("SET_SPEED_ACCEL ")) {
                     Serial.println("WebSocket: Received SET_SPEED_ACCEL command.");
                     if (isMoving || isHoming) { // Allow setting even in PnP mode if idle? Maybe not best idea. Only allow when not moving/homing.
                         Serial.println("[DEBUG] SET_SPEED_ACCEL denied: Machine busy.");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set speed/accel while machine is busy.\"}");
                     } else {
                         float receivedXS, receivedXA, receivedYS, receivedYA;
                         // Use C-style string for sscanf - receive actual values (e.g., 20000)
                         int parsed = sscanf(command.c_str() + strlen("SET_SPEED_ACCEL "), "%f %f %f %f", &receivedXS, &receivedXA, &receivedYS, &receivedYA);
                         // Check if parsing was successful AND values are positive
                         if (parsed == 4 && receivedXS > 0 && receivedXA > 0 && receivedYS > 0 && receivedYA > 0) {
                             // Update variables directly with received actual values
                             patternXSpeed = receivedXS;
                             patternXAccel = receivedXA;
                             patternYSpeed = receivedYS;
                             patternYAccel = receivedYA;

                             Serial.printf("[DEBUG] Saved Speed/Accel to NVS: X(S:%.0f, A:%.0f), Y(S:%.0f, A:%.0f)\n", patternXSpeed, patternXAccel, patternYSpeed, patternYAccel);

                             // Send confirmation with all current settings (using actual values)
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"Speed/Accel updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Failed to parse SET_SPEED_ACCEL values or values invalid.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format/values for SET_SPEED_ACCEL. Use: SET_SPEED_ACCEL XS XA YS YA (all positive)\"}");
                         }
                     }
                 } else if (command.startsWith("SET_FIRST_PLACE_ABS ")) { // UPDATED
                         Serial.println("[DEBUG] webSocketEvent: Handling SET_FIRST_PLACE_ABS command."); // DEBUG
                         float newX, newY;
                    // Expecting "SET_FIRST_PLACE_ABS X_val Y_val"
                    int parsed = sscanf(command.c_str() + strlen("SET_FIRST_PLACE_ABS "), "%f %f", &newX, &newY);
                         if (parsed == 2) {
                        placeFirstXAbsolute_inch = newX;
                        placeFirstYAbsolute_inch = newY;
                        Serial.printf("[DEBUG] Set First Place Absolute to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
                              // Send confirmation with all current settings
                        char msgBuffer[400]; // Increased size
                        sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"First Place Absolute Pos updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                            webSocket.broadcastTXT(msgBuffer);
                         } else {
                        Serial.printf("[DEBUG] Failed to parse SET_FIRST_PLACE_ABS command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_FIRST_PLACE_ABS format.\"}");
                    }
                 }
                 // --- Calibration Commands ---
                 else if (command == "ENTER_CALIBRATION") {
                    Serial.println("WebSocket: Received ENTER_CALIBRATION command.");
                    if (!allHomed) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine must be homed before entering calibration.\"}");
                    } else if (isMoving || isHoming || inPickPlaceMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Machine busy. Cannot enter calibration.\"}");
                 } else {
                        inCalibrationMode = true;
                        webSocket.broadcastTXT("{\"status\":\"CalibrationActive\", \"message\":\"Calibration mode entered.\"}");
                        sendCurrentPositionUpdate(); // Send initial position
                    }
                 } else if (command == "EXIT_CALIBRATION") {
                    Serial.println("WebSocket: Received EXIT_CALIBRATION command.");
                    inCalibrationMode = false;
                    // Send a standard Ready status update when exiting
                    webSocket.broadcastTXT("{\"status\":\"Ready\", \"message\":\"Exited calibration mode.\"}");
                 } else if (command.startsWith("SET_OFFSET_FROM_CURRENT")) {
                    Serial.println("WebSocket: Received SET_OFFSET_FROM_CURRENT command.");
                     if (!inCalibrationMode) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode.\"}");
                     } else if (isMoving || isHoming) {
                         webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot set while moving.\"}");
                     } else {
                         if (stepper_x && stepper_y_left) { // Use Y-Left as reference
                            pnpOffsetX_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
                            pnpOffsetY_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
                            
                              Serial.printf("[DEBUG] Set PnP Offset from current pos to X: %.2f, Y: %.2f\n", pnpOffsetX_inch, pnpOffsetY_inch);
                             char msgBuffer[400]; // Increased size
                             sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"PnP Offset set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                     pnpOffsetX_inch, pnpOffsetY_inch,
                                     placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                     patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                     placeGridCols, placeGridRows,
                                     placeGapX_inch, placeGapY_inch);
                             webSocket.broadcastTXT(msgBuffer);
                         } else {
                              webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Steppers not available.\"}");
                         }
                     }
                 } else if (command == "SET_FIRST_PLACE_ABS_FROM_CURRENT") { // UPDATED
                     if (!inCalibrationMode) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in Calibration Mode to set First Place position from current.\"}");
                     } else {
                         if (stepper_x && stepper_y_left) { // Use Y-Left as reference
                             placeFirstXAbsolute_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
                             placeFirstYAbsolute_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
                            
                               Serial.printf("[DEBUG] Set First Place Absolute from current pos to X: %.2f, Y: %.2f\n", placeFirstXAbsolute_inch, placeFirstYAbsolute_inch);
                              char msgBuffer[400]; // Increased size
                               sprintf(msgBuffer, "{\"status\":\"CalibrationActive\",\"message\":\"First Place Absolute Pos set from current position.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patXAccel\":%.0f,\"patYSpeed\":%.0f,\"patYAccel\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                       pnpOffsetX_inch, pnpOffsetY_inch,
                                       placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                       patternXSpeed, patternXAccel, patternYSpeed, patternYAccel,
                                       placeGridCols, placeGridRows,
                                       placeGapX_inch, placeGapY_inch);
                              webSocket.broadcastTXT(msgBuffer);
                         } else {
                             Serial.println("[ERROR] Stepper X or Y_Left not initialized when setting First Place Abs from current.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Internal stepper error.\"}");
                         }
                     }
                 } else if (command.startsWith("JOG ")) {
                    // Serial.println("WebSocket: Received JOG command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}");
                    } else {
                        char axis;
                        float distance_inch;
                        int parsed = sscanf(command.c_str() + strlen("JOG "), "%c %f", &axis, &distance_inch);
                        
                        if (parsed == 2) {
                            long current_steps = 0;
                            long jog_steps = 0;
                            FastAccelStepper* stepper_to_move = NULL;
                            float speed = 0, accel = 0;

                            if (axis == 'X' && stepper_x) {
                                stepper_to_move = stepper_x;
                                current_steps = stepper_x->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternXSpeed;
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\n", axis);
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                                
                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\n", axis, distance_inch, jog_steps, current_steps, target_steps);

                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);
                                stepper_to_move->moveTo(target_steps);

                                if (axis == 'Y' && stepper_y_right) { // Command right Y too
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps);
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y distance\"}");
                        }
                    }
                 } else if (command.startsWith("MOVE_TO_COORDS ")) {
                    Serial.println("WebSocket: Received MOVE_TO_COORDS command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to move to coordinates.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot move while machine is busy.\"}");
                    } else {
                        float targetX_inch, targetY_inch;
                        int parsed = sscanf(command.c_str() + strlen("MOVE_TO_COORDS "), "%f %f", &targetX_inch, &targetY_inch);
                        
                        if (parsed == 2) {
                            // Validate coordinates (optional, add bounds checking if needed)
                            if (targetX_inch < 0 || targetY_inch < 0) {
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid coordinates. Must be non-negative.\"}");
                                return;
                            }
                            
                            // Set up the move
                            isMoving = true; // Set flag, loop() will handle completion
                            webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Moving to coordinates...\"}");
                            
                            // Convert to steps
                            long targetX_steps = (long)(targetX_inch * STEPS_PER_INCH_XY);
                            long targetY_steps = (long)(targetY_inch * STEPS_PER_INCH_XY);
                            
                            // Command the move
                            if (stepper_x) {
                                stepper_x->setSpeedInHz(patternXSpeed);
                                stepper_x->setAcceleration(patternXAccel);
                                stepper_x->moveTo(targetX_steps);
                            }
                            
                            if (stepper_y_left && stepper_y_right) {
                                stepper_y_left->setSpeedInHz(patternYSpeed);
                                stepper_y_left->setAcceleration(patternYAccel);
                                stepper_y_left->moveTo(targetY_steps);
                                
                                stepper_y_right->setSpeedInHz(patternYSpeed);
                                stepper_y_right->setAcceleration(patternYAccel);
                                stepper_y_right->moveTo(targetY_steps);
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse MOVE_TO_COORDS command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format. Use: MOVE_TO_COORDS X Y\"}");
                         }
                     }
                 } else if (command.startsWith("SET_GRID_SPACING ")) { // UPDATED
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_GRID_SPACING command."); // DEBUG
                        int newCols, newRows;
                    // Expecting "SET_GRID_SPACING C R"
                    int parsed = sscanf(command.c_str() + strlen("SET_GRID_SPACING "), "%d %d", &newCols, &newRows);
                    if (parsed == 2 && newCols > 0 && newRows > 0) {
                        // Call the new calculation function
                        calculateAndSetGridSpacing(newCols, newRows);
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_GRID_SPACING command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_GRID_SPACING format. Use C R (integers > 0)\"}");
                    }
                 } else if (command.startsWith("SET_TRAY_SIZE ")) { // NEW
                    Serial.println("[DEBUG] webSocketEvent: Handling SET_TRAY_SIZE command."); // DEBUG
                    float newW, newH;
                    // Expecting "SET_TRAY_SIZE W H"
                    int parsed = sscanf(command.c_str() + strlen("SET_TRAY_SIZE "), "%f %f", &newW, &newH);
                    if (parsed == 2 && newW > 0 && newH > 0) {
                        Serial.printf("[DEBUG] New tray dimensions: width=%.2f, height=%.2f (old: width=%.2f, height=%.2f)\n", 
                                    newW, newH, trayWidth_inch, trayHeight_inch);
                        
                        // Save the new tray dimensions
                        trayWidth_inch = newW;
                        trayHeight_inch = newH;
                        
                          
                        Serial.printf("[DEBUG] Set Tray Size to W=%.2f, H=%.2f\n", trayWidth_inch, trayHeight_inch);
                        
                        // Recalculate grid spacing with current grid dimensions to reflect new tray size
                        float oldGapX = placeGapX_inch;
                        float oldGapY = placeGapY_inch;
                        calculateAndSetGridSpacing(placeGridCols, placeGridRows);
                        
                        Serial.printf("[DEBUG] Gap values changed: X: %.3f → %.3f, Y: %.3f → %.3f\n", 
                                    oldGapX, placeGapX_inch, oldGapY, placeGapY_inch);
                        
                        // Instead of using a redundant call to sendAllSettingsUpdate, create a more specific message
                        // about the changes that were made
                        char msgBuffer[200];
                        sprintf(msgBuffer, "Tray Size updated to W=%.2f, H=%.2f. Gap recalculated: X=%.3f, Y=%.3f", 
                               trayWidth_inch, trayHeight_inch, placeGapX_inch, placeGapY_inch);
                        
                        // Send settings update to the specific client that requested the change
                        sendAllSettingsUpdate(num, msgBuffer);
                        // Also broadcast to all other clients
                        if (num != 255) {
                            sendAllSettingsUpdate(255, msgBuffer);
                        }
                    } else {
                        Serial.printf("[DEBUG] Failed to parse SET_TRAY_SIZE command: %s\n", command.c_str());
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid SET_TRAY_SIZE format.\"}");
                    }
                 } else if (command.startsWith("SET_PAINT_PATTERN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_PATTERN_OFFSET command.");
                    if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_PATTERN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintPatternOffsetX_inch = newX;
                            paintPatternOffsetY_inch = newY;
                              Serial.printf("[DEBUG] Set Paint Pattern Offset to X: %.2f, Y: %.2f\n", paintPatternOffsetX_inch, paintPatternOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Pattern Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_PATTERN_OFFSET format. Use: SET_PAINT_PATTERN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_GUN_OFFSET ")) {
                    Serial.println("WebSocket: Received SET_PAINT_GUN_OFFSET command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set paint offsets while machine is busy.\"}");
                    } else {
                        float newX, newY;
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_GUN_OFFSET "), "%f %f", &newX, &newY);
                        if (parsed == 2) {
                            paintGunOffsetX_inch = newX;
                            paintGunOffsetY_inch = newY;
                              Serial.printf("[DEBUG] Set Paint Gun Offset to X: %.2f, Y: %.2f\n", paintGunOffsetX_inch, paintGunOffsetY_inch);
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint Gun Offset updated.");
                        } else {
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_GUN_OFFSET format. Use: SET_PAINT_GUN_OFFSET X Y\"}");
                        }
                    }
                } else if (command.startsWith("SET_PAINT_SIDE_SETTINGS ")) {
                    Serial.println("WebSocket: Received SET_PAINT_SIDE_SETTINGS command.");
                     if (isMoving || isHoming) { // Prevent changes while moving
                        webSocket.sendTXT(num, "{\"status\":\"Busy\",\"message\":\"Cannot set side settings while machine is busy.\"}");
                    } else {
                        int sideIndex, newPitch, newPattern; // Added newPattern
                        float newZ, newSpeed;
                        // Format: SET_PAINT_SIDE_SETTINGS sideIndex zHeight pitch pattern speed <-- Added pattern
                        int parsed = sscanf(command.c_str() + strlen("SET_PAINT_SIDE_SETTINGS "), "%d %f %d %d %f", // Added %d for pattern
                                           &sideIndex, &newZ, &newPitch, &newPattern, &newSpeed); // Added &newPattern

                        if (parsed == 5 && sideIndex >= 0 && sideIndex < 4) { // Changed parsed count to 5
                            // Validate UI pitch value first (0-90)
                            if (newPitch < 0 || newPitch > 90) {
                                Serial.printf("[ERROR] Invalid pitch value %d from UI (must be 0-90)\n", newPitch);
                                webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Pitch Angle for side " + String(sideIndex) + " must be between 0 and 90.\"}");
                                return; // Changed from continue to return for clarity
                            }
                            // Validate UI pattern value (must be 0 or 90)
                            if (newPattern != 0 && newPattern != 90) {
                                Serial.printf("[ERROR] Invalid pattern value %d from UI (must be 0 or 90)\n", newPattern);
                                webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Pattern for side " + String(sideIndex) + " must be 0 (Up-Down) or 90 (Left-Right).\"}");
                                return; // Changed from continue to return
                            }

                            // Map the UI pitch value (0-90) to servo value (PITCH_SERVO_MIN to PITCH_SERVO_MAX)
                            int mappedPitch = map(newPitch, 0, 90, PITCH_SERVO_MIN, PITCH_SERVO_MAX);
                            Serial.printf("[DEBUG] Mapped UI pitch %d to servo pitch %d\n", newPitch, mappedPitch);

                            // Speed constraint
                            newSpeed = constrain(newSpeed, 5000.0f, 20000.0f);

                            // Store the validated and mapped/constrained values
                            paintZHeight_inch[sideIndex] = newZ;
                            paintPitchAngle[sideIndex] = mappedPitch; // Use mapped value
                            paintPatternType[sideIndex] = newPattern; // Store pattern
                            paintSpeed[sideIndex] = newSpeed;

                              
                            Serial.printf("[DEBUG] Set Side %d Settings: Z=%.2f, P=%d, Pat=%d, S=%.0f\n", // Updated log, removed "and saved to NVS"
                                          sideIndex, paintZHeight_inch[sideIndex], paintPitchAngle[sideIndex],
                                          paintPatternType[sideIndex], paintSpeed[sideIndex]); // Added pattern
                            
                            // Send confirmation with all settings
                            sendAllSettingsUpdate(num, "Paint settings for side " + String(sideIndex) + " updated.");

                        } else {
                            Serial.printf("[ERROR] Invalid SET_PAINT_SIDE_SETTINGS format or values. Parsed %d args. Command: %s\n", parsed, command.c_str());
                            webSocket.sendTXT(num, "{\"status\":\"Error\",\"message\":\"Invalid SET_PAINT_SIDE_SETTINGS format. Use: SET_PAINT_SIDE_SETTINGS sideIndex zHeight pitch pattern(0/90) speed\"}");
                        }
                    }
                 } else if (command.startsWith("JOG ")) {
                    // Serial.println("WebSocket: Received JOG command.");
                    if (!inCalibrationMode) {
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Must be in calibration mode to jog.\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\"status\":\"Busy\", \"message\":\"Cannot jog while machine is moving.\"}");
                    } else {
                        char axis;
                        float distance_inch;
                        int parsed = sscanf(command.c_str() + strlen("JOG "), "%c %f", &axis, &distance_inch);
                        
                        if (parsed == 2) {
                            long current_steps = 0;
                            long jog_steps = 0;
                            FastAccelStepper* stepper_to_move = NULL;
                            float speed = 0, accel = 0;

                            if (axis == 'X' && stepper_x) {
                                stepper_to_move = stepper_x;
                                current_steps = stepper_x->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternXSpeed;
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\n", axis);
                                webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid axis for jog.\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\"status\":\"Busy\", \"message\":\"Jogging...\"}");
                                
                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\n", axis, distance_inch, jog_steps, current_steps, target_steps);

                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);
                                stepper_to_move->moveTo(target_steps);

                                if (axis == 'Y' && stepper_y_right) { // Command right Y too
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps);
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG format. Use: JOG X/Y distance\"}");
                        }
                    }
                 } else if (command == "START_PAINTING") {
                    Serial.println("WebSocket: Received START_PAINTING command.");
                    // startPaintingSequence(); // Call the (placeholder) function // COMMENTED OUT FOR NOW
                 } else if (command.startsWith("ROTATE ")) {
                    Serial.println("WebSocket: Received ROTATE command.");
                    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("DEBUG: Cannot rotate - Machine busy or not in correct mode");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot rotate: Machine busy or not in correct mode.\"}");
                    } else if (!stepper_rot) {
                        // ADDED: Check if stepper_rot is NULL and report error
                        Serial.println("DEBUG: Rotation command received, but stepper_rot is NULL (likely due to pin conflict)");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation control unavailable (pin conflict?)\"}");
                    } else {
                        float degrees;
                        int parsed = sscanf(command.c_str() + strlen("ROTATE "), "%f", &degrees);
                        if (parsed == 1) {
                            // Get current angle before we start
                            float currentAngle = (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE;
                            // Calculate target angle
                            float targetAngle = currentAngle + degrees;
                            
                            // Round to nearest step to avoid floating point errors
                            long relative_steps = round(degrees * STEPS_PER_DEGREE);
                            
                            Serial.printf("DEBUG: Rotating by %.1f degrees (%.4f steps per degree, %ld total steps)\n", 
                                        degrees, STEPS_PER_DEGREE, relative_steps);
                            Serial.printf("DEBUG: Current angle: %.2f degrees, Target angle: %.2f degrees\n", 
                                        currentAngle, targetAngle);
                            Serial.printf("DEBUG: Current position before rotation: %ld steps\n", stepper_rot->getCurrentPosition());
                            
                            isMoving = true;
                            webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Rotating tray...\"}");
                            
                            // Use different speeds for large vs. small movements
                            if (abs(degrees) > 10.0) {
                                stepper_rot->setSpeedInHz(patternRotSpeed); // Use full speed for larger moves
                            } else {
                                stepper_rot->setSpeedInHz(patternRotSpeed / 2); // Use reduced speed for precision
                            }
                            stepper_rot->setAcceleration(patternRotAccel);
                            stepper_rot->move(relative_steps); // Use move() for relative rotation
                            
                            Serial.printf("DEBUG: move() called with %ld steps\n", relative_steps);
                        } else {
                            Serial.println("DEBUG: Invalid ROTATE format");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid ROTATE format. Use: ROTATE degrees\"}");
                        }
                    }
                 } else if (command.startsWith("JOG ROT ")) {
                     Serial.println("WebSocket: Received JOG ROT command.");
                     if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("DEBUG: Cannot jog rotation - Machine busy or not in correct mode");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot jog rotation: Machine busy or not in correct mode.\"}");
                     } else if (!stepper_rot) {
                         // ADDED: Check if stepper_rot is NULL and report error
                         Serial.println("DEBUG: Rotation jog received, but stepper_rot is NULL (likely due to pin conflict)");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation control unavailable (pin conflict?)\"}");
                     } else {
                         float degrees;
                         int parsed = sscanf(command.c_str() + strlen("JOG ROT "), "%f", &degrees);
                         if (parsed == 1) {
                            // Get current angle before we start
                            float currentAngle = (float)stepper_rot->getCurrentPosition() / STEPS_PER_DEGREE;
                            // Calculate target angle
                            float targetAngle = currentAngle + degrees;
                            
                            // Round to nearest step to avoid floating point errors
                            long relative_steps = round(degrees * STEPS_PER_DEGREE);
                            
                            Serial.printf("DEBUG: Jogging rotation by %.1f degrees (%.4f steps per degree, %ld total steps)\n", 
                                        degrees, STEPS_PER_DEGREE, relative_steps);
                            Serial.printf("DEBUG: Current angle: %.2f degrees, Target angle: %.2f degrees\n", 
                                        currentAngle, targetAngle);
                            Serial.printf("DEBUG: Current position before jog: %ld steps\n", stepper_rot->getCurrentPosition());
                            
                            isMoving = true; // Set isMoving flag, let loop() handle completion
                            webSocket.broadcastTXT("{\"status\":\"Moving\", \"message\":\"Jogging rotation...\"}");
                            
                            // Use reduced speed for precision jogging (consistent with ROTATE small moves)
                            stepper_rot->setSpeedInHz(patternRotSpeed / 2); // Half speed for precision
                            stepper_rot->setAcceleration(patternRotAccel); // Use full accel like ROTATE small moves
                            stepper_rot->move(relative_steps);
                            
                            Serial.printf("DEBUG: move() called with %ld steps\n", relative_steps);
                        } else {
                            Serial.println("DEBUG: Invalid JOG ROT format");
                            webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid JOG ROT format. Use: JOG ROT degrees\"}");
                        }
                    }
                 } else if (command == "SET_ROT_ZERO") { // UPDATED COMMAND NAME
                    Serial.println("WebSocket: Received SET_ROT_ZERO command.");
                    if (!allHomed || isMoving || isHoming || inPickPlaceMode || inCalibrationMode) {
                        Serial.println("[DEBUG] SET_ROT_ZERO denied: Invalid state.");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set zero while machine is busy or in special mode.\"}");
                    } else if (!stepper_rot) {
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Rotation stepper not enabled.\"}");
                    } else {
                         Serial.println("[DEBUG] Setting current rotation position as zero.");
                         stepper_rot->setCurrentPosition(0);
                         // Save to Preferences? Not currently implemented
                         webSocket.sendTXT(num, "{\"status\":\"Ready\", \"message\":\"Current rotation set to zero.\"}");
                         sendCurrentPositionUpdate(); // Update display
                    }
                } else if (command == "PAINT_SIDE_0") { // <<< NEW COMMAND HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_0 command.");
                    paintSide(0); // Call the painting function for Side 0
                } else if (command == "PAINT_SIDE_1") { // <<< ADDED HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_1 command.");
                    paintSide(1); // Call the painting function for Side 1 (Right)
                } else if (command == "PAINT_SIDE_2") { // <<< ADDED HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_2 command.");
                    paintSide(2); // Call the painting function for Side 2 (Front)
                } else if (command == "PAINT_SIDE_3") { // <<< ADDED HANDLER
                    Serial.println("WebSocket: Received PAINT_SIDE_3 command.");
                    paintSide(3); // Call the painting function for Side 3 (Left)
                } else {
                    // Handle unknown commands
                    Serial.printf("[DEBUG] WebSocket [%u] Unknown command received: %s\n", num, command.c_str());
                    // Fixed quotes and backslashes
                    webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Unknown command\"}");
                }
            }
            break;
        // Removed WStype_BIN and others as they are empty
    }
}

// Function to send current position via WebSocket
void sendCurrentPositionUpdate() {
    if (!allHomed) return; // Don't send if not homed

    float currentX_inch = 0.0;
    float currentY_inch = 0.0;
    float currentZ_inch = 0.0;
    // Rotation angle removed

    if (stepper_x) {
        currentX_inch = (float)stepper_x->getCurrentPosition() / STEPS_PER_INCH_XY;
    }
    if (stepper_y_left) { // Use Y-Left as reference for Y position
        currentY_inch = (float)stepper_y_left->getCurrentPosition() / STEPS_PER_INCH_XY;
    }
     if (stepper_z) {
        currentZ_inch = (float)stepper_z->getCurrentPosition() / STEPS_PER_INCH_Z;
    }
    // Rotation angle calculation removed

    char posBuffer[150]; // Buffer size reduced slightly
    // Removed rotAngle field from JSON
    sprintf(posBuffer, "{\"type\":\"positionUpdate\", \"posX\":%.3f, \"posY\":%.3f, \"posZ\":%.3f}", 
            currentX_inch, currentY_inch, currentZ_inch);
    webSocket.broadcastTXT(posBuffer);
    // Serial.printf("[DEBUG] Sent Position Update (No Angle): %s\n", posBuffer); // Optional debug
}

// Helper function to send ALL current settings
// NOTE: Also does not include rotation angle
void sendAllSettingsUpdate(uint8_t specificClientNum, String message) {
    JsonDocument doc; // Declare JsonDocument, let library manage memory

    // Base status and message
    doc["status"] = "Ready"; // Assume Ready unless overridden
    if (isMoving) doc["status"] = "Busy";
    if (isHoming) doc["status"] = "Homing";
    if (inPickPlaceMode) doc["status"] = "PickPlaceReady"; // More specific PnP status
    if (inCalibrationMode) doc["status"] = "CalibrationActive";
    // TODO: Add other relevant states if needed (e.g., painting)

    // ADD EXPLICIT HOMED STATUS FIELD
    doc["homed"] = allHomed; // Add explicit field for homed status

    doc["message"] = message;

    // Speed/Accel Settings (showing displayed values)
    doc["patXSpeed"] = patternXSpeed; // Send actual value
    doc["patXAccel"] = patternXAccel; // Send actual value
    doc["patYSpeed"] = patternYSpeed; // Send actual value
    doc["patYAccel"] = patternYAccel; // Send actual value

    // Pick and Place Settings
    doc["pnpOffsetX"] = pnpOffsetX_inch;
    doc["pnpOffsetY"] = pnpOffsetY_inch;
    doc["placeFirstXAbs"] = placeFirstXAbsolute_inch; // Key updated
    doc["placeFirstYAbs"] = placeFirstYAbsolute_inch; // Key updated
    doc["gridCols"] = placeGridCols;
    doc["gridRows"] = placeGridRows;
    doc["gapX"] = placeGapX_inch; // Key updated
    doc["gapY"] = placeGapY_inch; // Key updated
    doc["trayWidth"] = trayWidth_inch; // Added
    doc["trayHeight"] = trayHeight_inch; // Added

    // Paint Settings
    doc["paintPatOffX"] = paintPatternOffsetX_inch;
    doc["paintPatOffY"] = paintPatternOffsetY_inch;
    doc["paintGunOffX"] = paintGunOffsetX_inch;
    doc["paintGunOffY"] = paintGunOffsetY_inch;

    // Paint Side Settings - Read from global arrays and add to JSON
    for (int i = 0; i < 4; ++i) {
        String keyZ = "paintZ_" + String(i);
        String keyP = "paintP_" + String(i);
        String keyPat = "paintPat_" + String(i);
        String keyS = "paintS_" + String(i);
        
        // --- ADDED: Add values from global arrays to JSON --- 
        // Need to map servo value back to UI value (0-90) for pitch
        int uiPitch = map(paintPitchAngle[i], PITCH_SERVO_MIN, PITCH_SERVO_MAX, 0, 90);
        doc[keyZ] = paintZHeight_inch[i];
        doc[keyP] = uiPitch; // Send the UI-mapped value
        doc[keyPat] = paintPatternType[i];
        doc[keyS] = paintSpeed[i];
        // --- END ADDED ---
    }

    // Serialize JSON to string
    String output;
    serializeJson(doc, output);

    // Send to specific client or broadcast
    if (specificClientNum < 255) {
        // Serial.printf("[DEBUG] Sending settings update to client %d: %s\n", specificClientNum, output.c_str()); // DEBUG
        webSocket.sendTXT(specificClientNum, output);
    } else {
        // Serial.printf("[DEBUG] Broadcasting settings update: %s\n", output.c_str()); // DEBUG
        webSocket.broadcastTXT(output);
    }
}

void setupWebServerAndWebSocket() { // Renamed from setupWebServer
    webServer.on("/", HTTP_GET, handleRoot); // Route for root page
    webServer.begin(); // Start HTTP server
    // Serial.println("HTTP server started.");

    webSocket.begin(); // Start WebSocket server
    webSocket.onEvent(webSocketEvent); // Assign event handler
    // Serial.println("WebSocket server started on port 81.");
} 