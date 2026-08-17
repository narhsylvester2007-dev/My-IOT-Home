
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <HTTPClient.h>

// Firebase Realtime Database (no-auth prototype)
// Set this to your DB URL (no trailing slash)
const char* FIREBASE_DB = "https://my-iot-project-442-default-rtdb.firebaseio.com";
// If you use a database secret or auth token, set it here (optional)
const char* FIREBASE_AUTH = "";

// --- WI-FI Configuration ---
const char* WIFI_SSID = "YOUR_SSID";   
const char* WIFI_PASS = "YOUR_PASS";   
// pins for each device (change to match your wiring)
const int PIN_BULB1 = 2;
const int PIN_BULB2 = 4;
const int PIN_FAN1  = 16;
const int PIN_FAN2  = 17;

// Voice Recognition Module V3 connection
const int VOICE_RX_PIN = 32;
const int VOICE_TX_PIN = 33;
const int VOICE_BAUD   = 9600;


// Training the module
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

// --- Simple newline-delimited JSON queue stored in LittleFS ---
void enqueueAction(const String &device, const String &action){
  String id = String(millis()) + String(random(1000,9999));
  unsigned long ts = millis();
  String obj = "{";
  obj += "\"id\":\"" + id + "\",";
  obj += "\"device\":\"" + device + "\",";
  obj += "\"action\":\"" + action + "\",";
  obj += "\"ts\":" + String(ts);
  obj += "}";
  File f = LittleFS.open("/queue.nd","a");
  if(f){
    f.println(obj);
    f.close();
    Serial.println("Enqueued action: " + obj);
  } else {
    Serial.println("Failed to open queue file for append");
  }
}

void flushQueueToFirebase(){
  if(!LittleFS.exists("/queue.nd")) return; // nothing to flush
  File f = LittleFS.open("/queue.nd","r");
  if(!f){ Serial.println("Failed to open queue for reading"); return; }
  File tmp = LittleFS.open("/queue.tmp","w");
  if(!tmp){ Serial.println("Failed to open temp file"); f.close(); return; }

  while(f.available()){
    String line = f.readStringUntil('\n');
    line.trim();
    if(line.length() == 0) continue;
    // parse id quickly
    int idPos = line.indexOf("\"id\":\"");
    String idKey = "";
    if(idPos >= 0){
      int start = idPos + 6; // position after "id":"
      int end = line.indexOf('"', start);
      if(start > 0 && end > start) idKey = line.substring(start, end);
    }
    String url = String(FIREBASE_DB) + "/actions" + (idKey.length() ? ("/" + idKey) : "") + ".json";
    if(strlen(FIREBASE_AUTH) > 0){
      url += "?auth=";
      url += FIREBASE_AUTH;
    }

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT(line);
    if(code >= 200 && code < 300){
      Serial.println("Flushed action to Firebase: " + url + " (" + String(code) + ")");
    } else {
      Serial.println("Failed to flush action (kept in queue): code=" + String(code));
      tmp.println(line); // keep for later
    }
    http.end();
    delay(50);
  }

  f.close(); tmp.close();
  // replace queue with remaining items
  LittleFS.remove("/queue.nd");
  if(LittleFS.exists("/queue.tmp")) LittleFS.rename("/queue.tmp","/queue.nd");
}

void executeVoiceCommand(uint8_t id){
  if(id == VOICE_CMD_BULB1_ON){
    applyDeviceState("bulb1", true);
    Serial.println("Voice: bulb1 ON");
    enqueueAction("bulb1", "on");
  } else if(id == VOICE_CMD_BULB1_OFF){
    applyDeviceState("bulb1", false);
    Serial.println("Voice: bulb1 OFF");
    enqueueAction("bulb1", "off");
  } else if(id == VOICE_CMD_BULB2_ON){
    applyDeviceState("bulb2", true);
    Serial.println("Voice: bulb2 ON");
    enqueueAction("bulb2", "on");
  } else if(id == VOICE_CMD_BULB2_OFF){
    applyDeviceState("bulb2", false);
    Serial.println("Voice: bulb2 OFF");
    enqueueAction("bulb2", "off");
  } else if(id == VOICE_CMD_FAN1_ON){
    applyDeviceState("fan1", true);
    Serial.println("Voice: fan1 ON");
    enqueueAction("fan1", "on");
  } else if(id == VOICE_CMD_FAN1_OFF){
    applyDeviceState("fan1", false);
    Serial.println("Voice: fan1 OFF");
    enqueueAction("fan1", "off");
  } else if(id == VOICE_CMD_FAN2_ON){
    applyDeviceState("fan2", true);
    Serial.println("Voice: fan2 ON");
    enqueueAction("fan2", "on");
  } else if(id == VOICE_CMD_FAN2_OFF){
    applyDeviceState("fan2", false);
    Serial.println("Voice: fan2 OFF");
    enqueueAction("fan2", "off");
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
  // try to serve from filesystem first
  String path = server.uri();
  if(path == "/") path = "/index.html";
  if(LittleFS.exists(path)){
    File f = LittleFS.open(path, "r");
    String contentType = "text/plain";
    if(path.endsWith(".html")) contentType = "text/html";
    else if(path.endsWith(".js")) contentType = "application/javascript";
    else if(path.endsWith(".css")) contentType = "text/css";
    else if(path.endsWith(".png")) contentType = "image/png";
    else if(path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
    server.streamFile(f, contentType);
    f.close();
    return;
  }
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
  // enqueue this user action for cloud sync
  enqueueAction(device, action);
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
  // Initialize LittleFS to serve web UI files from flash
  if(!LittleFS.begin()){
    Serial.println("LittleFS mount failed");
  } else {
    Serial.println("LittleFS mounted");
  }

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
  Serial.print("Connected (STA), IP: "); Serial.println(WiFi.localIP());

  // Also enable an AP so devices can connect locally even without upstream internet
  WiFi.mode(WIFI_AP_STA);
  bool apOK = WiFi.softAP("ESP32-AP");
  if(apOK){
    Serial.print("SoftAP started, IP: "); Serial.println(WiFi.softAPIP());
  }

  server.on("/control", HTTP_GET, handleControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/", HTTP_GET, [](){
    if(LittleFS.exists("/index.html")){
      File f = LittleFS.open("/index.html","r"); server.streamFile(f, "text/html"); f.close();
    } else {
      server.sendHeader("Location", "/status"); server.send(302, "text/plain", "");
    }
  });
  server.onNotFound(handleNotFound);
  server.on("/", HTTP_OPTIONS, handleOptions);
  server.on("/control", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_OPTIONS, handleOptions);

  // start a background task to periodically check internet availability (non-blocking)
  // we'll perform a simple HTTP GET to test connectivity when needed in loop()

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Voice module UART ready on RX=" + String(VOICE_RX_PIN) + " TX=" + String(VOICE_TX_PIN));
}

void loop(){
  handleVoiceSerial();
  server.handleClient();

  // Optionally detect internet availability and log it (non-blocking minimal check)
  static unsigned long lastCheck = 0;
  if(millis() - lastCheck > 15000){
    lastCheck = millis();
    if(WiFi.status() == WL_CONNECTED){
      HTTPClient http;
      http.begin("http://clients3.google.com/generate_204");
      int code = http.GET();
      if(code == 204){
        Serial.println("Internet reachable");
        // try to flush any queued actions to Firebase
        flushQueueToFirebase();
      } else {
        Serial.println("No upstream internet, but WiFi connected (local network only)");
      }
      http.end();
    } else {
      Serial.println("WiFi not connected (AP-only or offline)");
    }
  }
}
