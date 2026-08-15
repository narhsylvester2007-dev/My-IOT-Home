/*
  esp32_control.ino

  Example ESP32 Arduino sketch exposing simple endpoints:
    GET /control?device={device}&action={on|off}
    GET /status

  Replace WIFI_SSID and WIFI_PASS with your network credentials.
  Map device names to pins below as needed for your hardware.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

// --- configure these ---
const char* WIFI_SSID = "YOUR_SSID";    // replace
const char* WIFI_PASS = "YOUR_PASS";    // replace
// pins for each device (change to match your wiring)
const int PIN_BULB1 = 2;
const int PIN_BULB2 = 4;
const int PIN_FAN1  = 16;
const int PIN_FAN2  = 17;

// Voice Recognition Module V3 UART connection
// Change these pins if your wiring is different.
const int VOICE_RX_PIN = 32;
const int VOICE_TX_PIN = 33;
const int VOICE_BAUD   = 9600;

// Map the trained voice command IDs to device actions.
// Example: command ID 0 = bulb1 ON, 1 = bulb1 OFF, etc.
// Train the module with your chosen words and set these numbers to match.
const uint8_t VOICE_CMD_BULB1_ON  = 0;
const uint8_t VOICE_CMD_BULB1_OFF = 1;
const uint8_t VOICE_CMD_BULB2_ON  = 2;
const uint8_t VOICE_CMD_BULB2_OFF = 3;
const uint8_t VOICE_CMD_FAN1_ON   = 4;
const uint8_t VOICE_CMD_FAN1_OFF  = 5;
const uint8_t VOICE_CMD_FAN2_ON   = 6;
const uint8_t VOICE_CMD_FAN2_OFF  = 7;

WebServer server(80);

// in-memory state
bool state_bulb1 = false;
bool state_bulb2 = false;
bool state_fan1  = false;
bool state_fan2  = false;

String boolToOnOff(bool v){ return v?"on":"off"; }

void applyDeviceState(String device, bool enable){
  if(device == "bulb1"){
    state_bulb1 = enable; digitalWrite(PIN_BULB1, enable?HIGH:LOW);
  } else if(device == "bulb2"){
    state_bulb2 = enable; digitalWrite(PIN_BULB2, enable?HIGH:LOW);
  } else if(device == "fan1"){
    state_fan1 = enable; digitalWrite(PIN_FAN1, enable?HIGH:LOW);
  } else if(device == "fan2"){
    state_fan2 = enable; digitalWrite(PIN_FAN2, enable?HIGH:LOW);
  }
}

void executeVoiceCommand(uint8_t id){
  if(id == VOICE_CMD_BULB1_ON){
    applyDeviceState("bulb1", true);
    Serial.println("Voice: bulb1 ON");
  } else if(id == VOICE_CMD_BULB1_OFF){
    applyDeviceState("bulb1", false);
    Serial.println("Voice: bulb1 OFF");
  } else if(id == VOICE_CMD_BULB2_ON){
    applyDeviceState("bulb2", true);
    Serial.println("Voice: bulb2 ON");
  } else if(id == VOICE_CMD_BULB2_OFF){
    applyDeviceState("bulb2", false);
    Serial.println("Voice: bulb2 OFF");
  } else if(id == VOICE_CMD_FAN1_ON){
    applyDeviceState("fan1", true);
    Serial.println("Voice: fan1 ON");
  } else if(id == VOICE_CMD_FAN1_OFF){
    applyDeviceState("fan1", false);
    Serial.println("Voice: fan1 OFF");
  } else if(id == VOICE_CMD_FAN2_ON){
    applyDeviceState("fan2", true);
    Serial.println("Voice: fan2 ON");
  } else if(id == VOICE_CMD_FAN2_OFF){
    applyDeviceState("fan2", false);
    Serial.println("Voice: fan2 OFF");
  }
}

String readVoiceSerialLine(){
  String input = "";
  while(Serial2.available() > 0){
    char ch = (char)Serial2.read();
    if(ch == '\r' || ch == '\n'){
      if(input.length() > 0){
        break;
      }
      continue;
    }
    if((ch >= '0' && ch <= '9') || ch == '-' || ch == ' ' || ch == ':'){
      input += ch;
    }
  }
  return input;
}

void handleVoiceSerial(){
  String line = readVoiceSerialLine();
  if(line.length() == 0) return;

  line.trim();
  if(line.length() == 0) return;

  int id = line.toInt();
  if(id >= 0 && id <= 255){
    executeVoiceCommand((uint8_t)id);
  }
}

void handleNotFound(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(404, "text/plain", "Not found");
}

void handleOptions(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204, "text/plain", "");
}

void handleControl(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if(!server.hasArg("device") || !server.hasArg("action")){
    server.send(400, "text/plain", "Missing device or action");
    return;
  }
  String device = server.arg("device");
  String action = server.arg("action");
  action.toLowerCase();

  bool enable = (action == "on");
  if(device == "bulb1"){
    state_bulb1 = enable; digitalWrite(PIN_BULB1, enable?HIGH:LOW);
  } else if(device == "bulb2"){
    state_bulb2 = enable; digitalWrite(PIN_BULB2, enable?HIGH:LOW);
  } else if(device == "fan1"){
    state_fan1 = enable; digitalWrite(PIN_FAN1, enable?HIGH:LOW);
  } else if(device == "fan2"){
    state_fan2 = enable; digitalWrite(PIN_FAN2, enable?HIGH:LOW);
  } else {
    server.send(400, "text/plain", "Unknown device");
    return;
  }
  Serial.printf("Control: %s -> %s\n", device.c_str(), action.c_str());
  server.send(200, "text/plain", "OK");
}

void handleStatus(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"bulb1\":\"" + boolToOnOff(state_bulb1) + "\",";
  json += "\"bulb2\":\"" + boolToOnOff(state_bulb2) + "\",";
  json += "\"fan1\":\"" + boolToOnOff(state_fan1)  + "\",";
  json += "\"fan2\":\"" + boolToOnOff(state_fan2)  + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setupPins(){
  pinMode(PIN_BULB1, OUTPUT); digitalWrite(PIN_BULB1, state_bulb1?HIGH:LOW);
  pinMode(PIN_BULB2, OUTPUT); digitalWrite(PIN_BULB2, state_bulb2?HIGH:LOW);
  pinMode(PIN_FAN1, OUTPUT);  digitalWrite(PIN_FAN1,  state_fan1?HIGH:LOW);
  pinMode(PIN_FAN2, OUTPUT);  digitalWrite(PIN_FAN2,  state_fan2?HIGH:LOW);
}

void setup(){
  Serial.begin(115200);
  Serial2.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);
  delay(10);
  setupPins();

  // Use WiFiManager to provide captive-portal for Wi-Fi configuration
  WiFiManager wm;
  // Uncomment the next line to clear saved Wi-Fi credentials (useful for testing)
  // wm.resetSettings();
  Serial.println("Starting WiFiManager (captive portal if needed)...");
  // Set a timeout (seconds) for the config portal; remove to wait indefinitely
  wm.setConfigPortalTimeout(180);
  if(!wm.autoConnect("ESP32-Setup")){
    Serial.println("Failed to connect using WiFiManager, restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.print("Connected, IP: "); Serial.println(WiFi.localIP());

  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/", HTTP_GET, [](){ server.sendHeader("Location", "/status"); server.send(302, "text/plain", ""); });
  server.onNotFound(handleNotFound);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/control", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_OPTIONS, handleOptions);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Voice module UART ready on RX=" + String(VOICE_RX_PIN) + " TX=" + String(VOICE_TX_PIN));
}

void loop(){
  handleVoiceSerial();
  server.handleClient();
}
