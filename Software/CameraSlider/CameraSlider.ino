
#include <WiFi.h>
#include <Arduino.h>
#include "MultiStepperLite.h"
#include <WebServer.h>
#include "index_html.h"
#include <ESPmDNS.h>

WebServer server(80);

bool apMode = false;

// ----- USER CONFIGURABLE VALUES ----- //

const char* apName = "PRO SLIDER";
const char* apPass = "12345678";

const char* ssid = "YOURSSID";
const char* password = "YOURPASSWORD";

int MICROSTEPS = 16;
int gearRatio = 4;
int xAxisPulleyT = 20;
int toothPitch = 2;

int R_AXIS_STEPSREV = 200;
int T_AXIS_STEPSREV = 200;
int X_AXIS_STEPSMM = 200;

int maxX = 650;

// ----- GPIO ESP32-S3 N16R8 ----- //

const int R_STEP_PIN = 13;
const int R_DIRECTION_PIN = 14;

const int X_STEP_PIN = 4;
const int X_DIRECTION_PIN = 5;

const int T_STEP_PIN = 6;
const int T_DIRECTION_PIN = 7;

const int X_AXIS_LIMIT = 16;

bool debug = false;

// ----- END CONFIGURATION ----- //

#define XStepper 0
#define RStepper 1
#define TStepper 2

// IO Debounce

int limitState;
int lastLimitState = LOW;

unsigned long lastLimitDebounce = 0;
unsigned long debounceDelay = 80;

// Motor positions

long targetX = 0;
long currentX = 0;

float currentR = 0;
float targetR = 0;

float currentT = 0;
float targetT = 0;

// Acceleration

int accelSteps = 30;

int jogRTAccelMultiplier = 6;
int jogXAccelMultiplier = 20;

// Keyframes

long key_X[5];
float key_R[5];
float key_T[5];

bool key_Set[5] = {false};

int key_Time[5];
int key_Delay[5];

String lastCommand;

bool inStartLocation = false;

WiFiClient sseClient;

MultiStepperLite steppers(3);

// ----- FUNCTION DECLARATIONS ----- //

bool connectWiFi();
void startAP();
void checkIO();

void handleCommand();
void handleEvents();
void handleText();

void sendEvent(const String& msg);

long convertMMtoSteps(int input);
long convertStepstoMM(int input);

long convertRevstoSteps(float input);
float convertStepstoRevs(float input);

long convertDegtoSteps(float input);
float convertStepstoDegrees(float input);

// These functions must also exist in your original
// MultiStepperLite slider program.

void homeXAxis();
void jogX(int value);
void jogR(int value);
void jogT(int value);

void returnToKey1();
void programmedMove();
void gotoKey(int direction);

// ----- SETUP ----- //

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("      PRO SLIDER ESP32");
  Serial.println("==============================");

  // GPIO configuration

  Serial.println("Configuring GPIO...");

  pinMode(X_AXIS_LIMIT, INPUT_PULLUP);

  pinMode(X_DIRECTION_PIN, OUTPUT);
  pinMode(R_DIRECTION_PIN, OUTPUT);
  pinMode(T_DIRECTION_PIN, OUTPUT);

  digitalWrite(X_DIRECTION_PIN, LOW);
  digitalWrite(R_DIRECTION_PIN, LOW);
  digitalWrite(T_DIRECTION_PIN, LOW);

  // Initialize steppers

  Serial.println("Initializing steppers...");

  steppers.init_stepper(XStepper, X_STEP_PIN);
  delay(10);

  steppers.init_stepper(RStepper, R_STEP_PIN);
  delay(10);

  steppers.init_stepper(TStepper, T_STEP_PIN);
  delay(10);

  // Axis step calculations

  R_AXIS_STEPSREV =
    R_AXIS_STEPSREV * gearRatio * MICROSTEPS;

  T_AXIS_STEPSREV =
    T_AXIS_STEPSREV * gearRatio * MICROSTEPS;

  X_AXIS_STEPSMM =
    ((X_AXIS_STEPSMM * MICROSTEPS) /
    xAxisPulleyT) / toothPitch;

  Serial.println("Motor configuration complete.");

  // WiFi connection

  Serial.println("Connecting to network...");

  if (!connectWiFi()) {

    Serial.println("Starting AP mode...");

    startAP();

  }

  // Web server routes

  Serial.println("Configuring web server...");

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_POST, handleCommand);

  server.on("/events", HTTP_GET, handleEvents);

  server.on("/text", HTTP_POST, handleText);

  server.begin();

  Serial.println("Web server started.");

  // Check limit switch

  Serial.println("Checking limit switch...");

  for (int i = 0; i < 100; i++) {

    checkIO();

    delay(1);

  }

  delay(100);

  // Homing

  Serial.println("Starting X axis homing...");

  homeXAxis();

  Serial.println("Setup completed!");

}

// ----- WIFI CONNECTION ----- //

bool connectWiFi() {

  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.setHostname("slider");

  WiFi.begin(ssid, password);

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 10000
  ) {

    delay(500);

    Serial.print(".");

  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("WiFi connected!");

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("slider")) {

      Serial.println("mDNS started.");

      Serial.println("http://slider.local");

    } else {

      Serial.println("mDNS initialization failed.");

    }

    return true;

  }

  Serial.println();
  Serial.println("WiFi connection failed.");

  WiFi.disconnect(true);

  delay(100);

  return false;

}

// ----- ACCESS POINT ----- //

void startAP() {

  Serial.println("Starting Access Point...");

  WiFi.mode(WIFI_AP);

  WiFi.softAP(apName, apPass);

  IPAddress IP = WiFi.softAPIP();

  Serial.print("AP IP: ");
  Serial.println(IP);

  apMode = true;

}

// ----- MAIN LOOP ----- //

void loop() {

  server.handleClient();

  checkIO();

  delay(2);

}

// ----- POSITION CONVERSIONS ----- //

long convertMMtoSteps(int input) {

  return (input * MICROSTEPS) * 5;

}

long convertStepstoMM(int input) {

  return (input / MICROSTEPS) / 5;

}

long convertRevstoSteps(float input) {

  return R_AXIS_STEPSREV * input;

}

float convertStepstoRevs(float input) {

  return input / (float)R_AXIS_STEPSREV;

}

long convertDegtoSteps(float input) {

  return (R_AXIS_STEPSREV * input) / 360;

}

float convertStepstoDegrees(float input) {

  return (input * 360.0) / R_AXIS_STEPSREV;

}

// ----- INPUT CHECK ----- //

void checkIO() {

  int limitReading = digitalRead(X_AXIS_LIMIT);

  if (limitReading != lastLimitState) {

    lastLimitDebounce = millis();

  }

  if ((millis() - lastLimitDebounce) > debounceDelay) {

    if (limitReading != limitState) {

      limitState = limitReading;

    }

  }

  lastLimitState = limitReading;

}

// ----- SERVER EVENTS ----- //

void handleEvents() {

  sseClient = server.client();

  sseClient.println("HTTP/1.1 200 OK");

  sseClient.println("Content-Type: text/event-stream");

  sseClient.println("Cache-Control: no-cache");

  sseClient.println("Connection: keep-alive");

  sseClient.println();

}

void sendEvent(const String& msg) {

  if (sseClient && sseClient.connected()) {

    sseClient.print("data: ");

    sseClient.println(msg);

    Serial.print(msg);

    sseClient.println();

    Serial.println();

  }

}

// ----- COMMAND HANDLER ----- //

void handleCommand() {

  String cmd = server.arg("plain");

  Serial.println("Received command: " + cmd);

  // X Axis

  if (cmd == "+100") {

    jogX(100);

  } else if (cmd == "-100") {

    jogX(-100);

  } else if (cmd == "+10") {

    jogX(10);

  } else if (cmd == "-10") {

    jogX(-10);

  } else if (cmd == "+1") {

    jogX(1);

  } else if (cmd == "-1") {

    jogX(-1);

  }

  // R Axis

  if (cmd == "+30degR") {

    jogR(30);

  } else if (cmd == "-30degR") {

    jogR(-30);

  } else if (cmd == "+15degR") {

    jogR(15);

  } else if (cmd == "-15degR") {

    jogR(-15);

  } else if (cmd == "+1degR") {

    jogR(1);

  } else if (cmd == "-1degR") {

    jogR(-1);

  }

  // T Axis

  if (cmd == "+30degT") {

    jogT(30);

  } else if (cmd == "-30degT") {

    jogT(-30);

  } else if (cmd == "+15degT") {

    jogT(15);

  } else if (cmd == "-15degT") {

    jogT(-15);

  } else if (cmd == "+1degT") {

    jogT(1);

  } else if (cmd == "-1degT") {

    jogT(-1);

  }

  // Program commands

  if (cmd == "GotoStart") {

    sendEvent("LOG:Returning to Key 1");

    returnToKey1();

  } else if (cmd == "Begin") {

    if (inStartLocation) {

      sendEvent("LOG:Begin");

      programmedMove();

    } else {

      sendEvent("LOG:Goto Start first!");

    }

  } else if (cmd == "PrevKey") {

    gotoKey(0);

  } else if (cmd == "NextKey") {

    gotoKey(1);

  }

  server.send(200, "text/plain", "OK");

}

// ----- TEXT HANDLER ----- //

void handleText() {

  String payload = server.arg("plain");

  int sep = payload.indexOf(':');

  int sep2 = payload.indexOf(':', sep + 1);

  if (sep < 0 || sep2 < 0) {

    server.send(400, "text/plain", "Bad format");

    return;

  }

  String btn = payload.substring(0, sep);

  String time = payload.substring(sep + 1, sep2);

  String delayValue = payload.substring(sep2 + 1);

  String output = "SET";

  if (debug) {

    Serial.println("Raw payload: " + payload);

    Serial.println("Button: " + btn);

    Serial.println("Time: " + time);

  }

  // Set keyframes

  int keyIndex = -1;

  if (btn.startsWith("Set")) {

    keyIndex = btn.substring(3).toInt() - 1;

    if (keyIndex < 0 || keyIndex >= 5) {

      server.send(400, "text/plain", "Invalid keyframe");

      return;

    }

    if (time == "-1") {

      time = "60";

    }

    key_X[keyIndex] = currentX;

    key_R[keyIndex] = currentR;

    key_T[keyIndex] = currentT;

    key_Set[keyIndex] = true;

    key_Time[keyIndex] = time.toInt();

    key_Delay[keyIndex] = delayValue.toInt();

    output += String(keyIndex + 1);

    output += ":Slide=" + String(currentX);

    output += ", Pan=" + String(currentR);

    output += ", Tilt=" + String(currentT);

    if (keyIndex > 0) {

      output += ", Time=" + time;

      output += ", Delay=" + delayValue;

    }

    sendEvent("LOG:Set Key " + String(keyIndex + 1));

  }

  // Clear keyframes

  else if (btn.startsWith("Clear")) {

    keyIndex = btn.substring(5).toInt() - 1;

    if (keyIndex < 0 || keyIndex >= 5) {

      server.send(400, "text/plain", "Invalid keyframe");

      return;

    }

    key_Set[keyIndex] = false;

    sendEvent("LOG:Cleared Key " + String(keyIndex + 1));

  }

  sendEvent(output);

  server.send(200, "text/plain", "OK");

}
