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
            
            // Log sample value before sending settings
            Serial.printf("[DEBUG] CONNECTED: pnpOffsetX before sendAllSettings = %.2f\n", pnpOffsetX_inch);
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
                             saveSettings(); // Save updated settings
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
                 } else if (command.startsWith("SET_PNP_SPEEDS ")) { // Renamed command
                     Serial.println("WebSocket: Received SET_PNP_SPEEDS command.");
                     if (isMoving || isHoming) { // Allow setting even in PnP mode if idle? Maybe not best idea. Only allow when not moving/homing.
                         Serial.println("[DEBUG] SET_SPEED_ACCEL denied: Machine busy.");
                         webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Cannot set speed/accel while machine is busy.\"}");
                     } else {
                         float receivedXS, receivedYS;
                         // Use C-style string for sscanf - receive actual values (e.g., 20000)
                         int parsed = sscanf(command.c_str() + strlen("SET_PNP_SPEEDS "), "%f %f", &receivedXS, &receivedYS);
                         // Check if parsing was successful AND values are positive
                         if (parsed == 2 && receivedXS > 0 && receivedYS > 0) {
                             // Update speed variables directly with received actual values
                             patternXSpeed = receivedXS;
                             patternYSpeed = receivedYS;
                             // Acceleration variables remain unchanged
 
                              saveSettings(); // Save updated settings
                             Serial.printf("[DEBUG] Saved Speeds to NVS: X(S:%.0f), Y(S:%.0f)\n", patternXSpeed, patternYSpeed);
 
                              // Send confirmation with all current settings (using actual values)
                              char msgBuffer[400]; // Increased size
                              // Removed acceleration values from response JSON
                              sprintf(msgBuffer, "{\"status\":\"Ready\",\"message\":\"Speeds updated.\",\"pnpOffsetX\":%.2f,\"pnpOffsetY\":%.2f,\"placeFirstXAbs\":%.2f,\"placeFirstYAbs\":%.2f,\"patXSpeed\":%.0f,\"patYSpeed\":%.0f,\"gridCols\":%d,\"gridRows\":%d,\"gapX\":%.3f,\"gapY\":%.3f}",
                                      pnpOffsetX_inch, pnpOffsetY_inch,
                                      placeFirstXAbsolute_inch, placeFirstYAbsolute_inch,
                                      patternXSpeed, patternYSpeed,
                                      placeGridCols, placeGridRows,
                                      placeGapX_inch, placeGapY_inch);
                              webSocket.broadcastTXT(msgBuffer);
                          } else {
                              Serial.println("[ERROR] Failed to parse SET_PNP_SPEEDS values or values invalid.");
                              webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format/values for SET_PNP_SPEEDS. Use: SET_PNP_SPEEDS XS YS (all positive)\"}");
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
                        saveSettings(); // Save updated settings
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
                        webSocket.sendTXT(num, "{\\\"status\\\":\\\"Error\\\", \\\"message\\\":\\\"Must be in calibration mode to jog.\\\"}");
                    } else if (isMoving || isHoming) {
                        webSocket.sendTXT(num, "{\\\"status\\\":\\\"Busy\\\", \\\"message\\\":\\\"Cannot jog while machine is moving.\\\"}");
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
                                speed = patternXSpeed; // Use pattern speed/accel for consistency? Or define specific jog speeds?
                                accel = patternXAccel;
                            } else if (axis == 'Y' && stepper_y_left && stepper_y_right) { // Assuming both Y move together
                                stepper_to_move = stepper_y_left; // Use left for current pos, command both
                                current_steps = stepper_y_left->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_XY);
                                speed = patternYSpeed;
                                accel = patternYAccel;
                            } else if (axis == 'Z' && stepper_z) { // ADDED JOG Z
                                stepper_to_move = stepper_z;
                                current_steps = stepper_z->getCurrentPosition();
                                jog_steps = (long)(distance_inch * STEPS_PER_INCH_Z); // Use Z steps per inch
                                speed = patternZSpeed; // Use Z speed/accel
                                accel = patternZAccel;
                             } else {
                                Serial.printf("[ERROR] Invalid axis '%c' or stepper not available for JOG.\\n", axis);
                                webSocket.sendTXT(num, "{\\\"status\\\":\\\"Error\\\", \\\"message\\\":\\\"Invalid axis for jog.\\\"}");
                            }

                            if (stepper_to_move) {
                                isMoving = true; // Set flag, loop() will handle completion
                                webSocket.broadcastTXT("{\\\"status\\\":\\\"Busy\\\", \\\"message\\\":\\\"Jogging...\\\"}");

                                long target_steps = current_steps + jog_steps;
                                //Serial.printf("  Jogging %c by %.3f inches (%ld steps) from %ld to %ld\\n", axis, distance_inch, jog_steps, current_steps, target_steps); // DEBUG

                                // Apply speed and acceleration
                                stepper_to_move->setSpeedInHz(speed);
                                stepper_to_move->setAcceleration(accel);

                                // If Z axis, check bounds BEFORE moving
                                if (axis == 'Z') {
                                     float target_pos_inch = (float)target_steps / STEPS_PER_INCH_Z;
                                     if (target_pos_inch < Z_MAX_TRAVEL_NEG_INCH || target_pos_inch > Z_MAX_TRAVEL_POS_INCH) {
                                         Serial.printf("[WARN] Jog Z target (%.3f in) out of bounds (%.3f to %.3f in). Cancelling jog.\\n", target_pos_inch, Z_MAX_TRAVEL_NEG_INCH, Z_MAX_TRAVEL_POS_INCH);
                                         webSocket.sendTXT(num, "{\\\"status\\\":\\\"Error\\\", \\\"message\\\":\\\"Jog target Z out of bounds.\\\"}");
                                         isMoving = false; // Reset flag
                                         // Send Ready status if nothing else is moving? Might be complex. Rely on next position update.
                                     } else {
                                         stepper_to_move->moveTo(target_steps);
                                     }
                                } else {
                                     // For X and Y, just move (assuming limits are less critical or handled elsewhere)
                                     stepper_to_move->moveTo(target_steps);
                                }


                                // Command right Y too if Y axis was selected
                                if (axis == 'Y' && stepper_y_right) {
                                    stepper_y_right->setSpeedInHz(speed);
                                    stepper_y_right->setAcceleration(accel);
                                    stepper_y_right->moveTo(target_steps); // Assume same target steps
                                }
                            }
                        } else {
                            Serial.println("[ERROR] Failed to parse JOG command.");
                            webSocket.sendTXT(num, "{\\\"status\\\":\\\"Error\\\", \\\"message\\\":\\\"Invalid JOG format. Use: JOG X/Y/Z distance\\\"}");
                        }
                    }
                 } else if (command.startsWith("SET_SERVO_PITCH ")) { // <<< ADDED THIS BLOCK
                    Serial.println("WebSocket: Received SET_SERVO_PITCH command.");
                    int angle;
                    // Expecting \"SET_SERVO_PITCH angle\"
                    int parsed = sscanf(command.c_str() + strlen("SET_SERVO_PITCH "), "%d", &angle);
                    Serial.printf("[DEBUG webSocketEvent] Parsed SET_SERVO_PITCH: parsed=%d, angle=%d\n", parsed, angle); // <<< ADDED DEBUG
                    if (parsed == 1) {
                        // Basic validation (0-180 is typical for servos)
                        if (angle >= 0 && angle <= 180) {
                             // Call the function to set the angle (will be defined in main.cpp)
                             setPitchServoAngle(angle);
                             Serial.printf("[DEBUG] Set Pitch Servo Angle to: %d\\n", angle);
                             // Send confirmation back to the specific client
                             char msgBuffer[100];
                             sprintf(msgBuffer, "{\\\"status\\\":\\\"Info\\\", \\\"message\\\":\\\"Pitch servo angle set to %d\\\"}", angle);
                             webSocket.sendTXT(num, msgBuffer);
                         } else {
                             Serial.println("[ERROR] Invalid angle for SET_SERVO_PITCH. Must be 0-180.");
                             webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid angle. Must be 0-180.\"}");
                         }
                    } else {
                        Serial.println("[ERROR] Failed to parse SET_SERVO_PITCH angle.");
                        webSocket.sendTXT(num, "{\"status\":\"Error\", \"message\":\"Invalid format for SET_SERVO_PITCH. Use: SET_SERVO_PITCH angle\"}");
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
    doc["patYSpeed"] = patternYSpeed; // Send actual value

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
        // Map the stored servo angle (MIN-MAX) back to UI range (0-90)
        // int uiPitch = map(paintPitchAngle[i], PITCH_SERVO_MIN, PITCH_SERVO_MAX, 0, 90); // REMOVED MAPPING
        int uiPitch = paintPitchAngle[i]; // Send raw servo angle (0-180)
        doc[keyZ] = paintZHeight_inch[i];
        doc[keyP] = uiPitch; // Send the potentially raw angle
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