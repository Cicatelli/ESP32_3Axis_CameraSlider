#include <WiFi.h>
#include <Arduino.h>
#include "MultiStepperLite.h" //Library version used at time of writing: 1.2.0 
#include <WebServer.h>
#include "index_html.h"
#include <ESPmDNS.h>

WebServer server(80);
bool apMode = false;

// ----- USER CONFIGURABLE VALUES ----- //

  const char* apName = "ESP32-Slider"; //The controller will create an access point with this name if the configured wifi SSID isn't available to connect.
  const char* apPass = "12345678"; //Password for the access point

  const char* ssid = "YOURSSID"; //Enter your wifi SSID here
  const char* password = "YOURPASSWORD"; //Enter your wifi Password here

  int MICROSTEPS = 16; //Change to suit your drives Microstepping setting. Higher microstepping values will allow slower movements and make motors smoother.
  int gearRatio = 4; //Gear ratio of rotation/tilt axis. Example 80t to 20t pulley is a ratio of 4:1 so value should be 4.
  int xAxisPulleyT = 20; //Number of teeth on X axis pulley
  int toothPitch = 2; //Belt Tooth Spacing. GT2 = 2mm

  //Change these values to match your stepper motors steps/rev. 1.8 degree stepper motors are 200 Steps per Rev.
  int R_AXIS_STEPSREV = 200;
  int T_AXIS_STEPSREV = 200;
  int X_AXIS_STEPSMM = 200;

  int maxX = 650; //This value should be the length of your slider extrusion, minus the width of the carriage. If you built my design, the width is 110mm.

  //IO Assignments. Be careful if you reassign these as there are lots of pins you can't use on the ESP32.
  const int R_STEP_PIN = 13;
  const int R_DIRECTION_PIN = 14;
  const int X_STEP_PIN = 27;
  const int X_DIRECTION_PIN = 26;
  const int T_STEP_PIN = 25;
  const int T_DIRECTION_PIN = 33;
  const int X_AXIS_LIMIT = 16;

  bool debug = false; //Set to true for debug serial output

// ----- END OF USER CONFIGURABLE VALUES ----- //

#define XStepper 0
#define RStepper 1
#define TStepper 2

//IO Debounce
int limitState;
int lastLimitState = LOW;
//int triggerState;
//int lastTriggerState = LOW;
unsigned long lastLimitDebounce = 0;
//unsigned long lastTriggerDebounce = 0;
unsigned long debounceDelay = 80;

//Target and actual positions of motors. X axis is in MM, R and T axis are in Degrees
long targetX = 0;
long currentX = 0;
float currentR = 0;
float targetR = 0;
float currentT = 0;
float targetT = 0;

//Acceleration Variables
int accelSteps = 30; //Number of steps in Accel/Decel calculation.
//These values are used to calculate the acceleration for jog moves.
int jogRTAccelMultiplier = 6;
int jogXAccelMultiplier = 20;

//Keyframe Value Storage Arrays
long key_X[5];
float key_R[5];
float key_T[5];
bool key_Set[5] = {false};
int key_Time[5];
int key_Delay[5];



String lastCommand; //Stores last movement command as a String
bool inStartLocation = false; //Programmed move won't begin until this variable is true.

WiFiClient sseClient;

MultiStepperLite steppers(3);

void setup() {
  Serial.begin(115200);

  //X Axis
  pinMode(X_AXIS_LIMIT, INPUT_PULLUP);
  pinMode(X_DIRECTION_PIN, OUTPUT);
  digitalWrite(X_DIRECTION_PIN, LOW);

  //R Axis
  pinMode(R_DIRECTION_PIN, OUTPUT);
  digitalWrite(R_DIRECTION_PIN, LOW);

  //T Axis
  pinMode(T_DIRECTION_PIN, OUTPUT);
  digitalWrite(T_DIRECTION_PIN, LOW);

  steppers.init_stepper(XStepper, X_STEP_PIN);
  steppers.init_stepper(RStepper, R_STEP_PIN);
  steppers.init_stepper(TStepper, T_STEP_PIN);

  //Axis Step Calculations
  R_AXIS_STEPSREV = R_AXIS_STEPSREV * gearRatio * MICROSTEPS; //4:1 Reduction, 16x Multistep, 200 Step/Rev = 12800
  T_AXIS_STEPSREV = T_AXIS_STEPSREV * gearRatio * MICROSTEPS;
  X_AXIS_STEPSMM = ((X_AXIS_STEPSMM * MICROSTEPS) / xAxisPulleyT) / toothPitch; //20t Pulley, 16x Multistep, 200 Step/Rev, 2mm pitch = 80

  if (!connectWiFi())
  {
    startAP();
  }

  /*WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");

  if (!MDNS.begin("slider")) {
    Serial.print("Error starting mDNS, use IP to connect: ");
    Serial.println(WiFi.localIP());
    return;
  } else {
    Serial.println("mDNS Started. Visit http://slider.local for controls");
  }*/

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", index_html);
  });

  server.on("/cmd", HTTP_POST, handleCommand);
  server.on("/events", HTTP_GET, handleEvents);
  server.on("/text", HTTP_POST, handleText);

  server.begin();

  for(int i = 0; i < 100; i++){
    checkIO();
  }
  delay(100);
  homeXAxis();
}

bool connectWiFi(){
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("slider");

  WiFi.begin(ssid, password);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000){
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED){
    Serial.println("\nConnected!");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("slider")){
      Serial.println("mDNS started: http://slider.local");
    }

    return true;
  }

  Serial.println("\nWiFi failed");
  return false;
}

void startAP(){
  Serial.println("Starting Access Point...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName, apPass);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(IP);

  apMode = true;
}

void loop() {
  server.handleClient();

  static unsigned long last = 0;
  if (millis() - last > 5000) {
    last = millis();
  }
}

long convertMMtoSteps(int input){
  return (input * MICROSTEPS) * 5;
}

long convertStepstoMM(int input){
  return (input / MICROSTEPS) / 5;
}

long convertRevstoSteps(float input){
  return R_AXIS_STEPSREV * input;
}

long convertStepstoRevs(float input){
  return input / R_AXIS_STEPSREV;
}

long convertDegtoSteps(float input){
  return (R_AXIS_STEPSREV * input) / 360;
}

void checkIO(){
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

  /*int triggerReading = digitalRead(X_AXIS_LIMIT);

  if (triggerReading != lastTriggerState) {
    lastTriggerDebounce = millis();
  }
  if ((millis() - lastTriggerDebounce) > debounceDelay) {
    if (triggerReading != triggerState) {
      triggerState = triggerReading;
    }
  }
  lastTriggerState = triggerReading;*/
}

void handleEvents() {
  sseClient = server.client();
  sseClient.println("HTTP/1.1 200 OK");
  sseClient.println("Content-Type: text/event-stream");
  sseClient.println("Cache-Control: no-cache");
  sseClient.println("Connection: keep-alive");
  sseClient.println();
}

void handleCommand() {
  String cmd = server.arg("plain");
  Serial.println("Received command: " + cmd);

  //X Handler
  if (cmd == "+100") {
    jogX(100);
  } else if (cmd == "-100") {
    jogX(-100);
  } else if(cmd == "+10"){
    jogX(10);
  } else if(cmd == "-10"){
    jogX(-10);
  } else if(cmd == "+1"){
    jogX(1);
  } else if(cmd == "-1"){
    jogX(-1);
  }
  //R Handler
  if (cmd == "+30degR") {
    jogR(30);
  } else if (cmd == "-30degR") {
    jogR(-30);
  } else if(cmd == "+15degR"){
    jogR(15);
  } else if(cmd == "-15degR"){
    jogR(-15);
  } else if(cmd == "+1degR"){
    jogR(1);
  } else if(cmd == "-1degR"){
    jogR(-1);
  }
  //T Handler
  if (cmd == "+30degT") {
    jogT(30);
  } else if (cmd == "-30degT") {
    jogT(-30);
  } else if(cmd == "+15degT"){
    jogT(15);
  } else if(cmd == "-15degT"){
    jogT(-15);
  } else if(cmd == "+1degT"){
    jogT(1);
  } else if(cmd == "-1degT"){
    jogT(-1);
  }

  if(cmd == "GotoStart"){
    sendEvent("LOG:Returning to Key 1");
    returnToKey1();
  } else if(cmd == "Begin"){
    if(inStartLocation){
      sendEvent("LOG:Begin");
      programmedMove();
    } else {
      sendEvent("LOG:Goto Start first!");
    }
  } else if(cmd == "PrevKey"){
    gotoKey(0);
    
  } else if(cmd == "NextKey"){
    gotoKey(1);
  }

  server.send(200, "text/plain", "OK");
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
  String delay = payload.substring(sep2 + 1);
  String output = "SET";
  
  if(debug){
    Serial.println("Raw payload: " + payload);
    Serial.println("Button: " + btn);
    Serial.println("Time: " + time);
  }

  //Set Keyframes
  if (btn == "Set1") {
    if(time == "-1"){
      time = "60";
    }
    output = output + "1:Slide=" + currentX + ", Pan=" + currentR + ", Tilt=" + currentT; //Convert Revolutions back to degrees for R and T Axis
    key_X[0] = currentX;
    key_R[0] = currentR;
    key_T[0] = currentT;
    key_Set[0] = true;
    key_Time[0] = time.toInt();
    key_Delay[0] = delay.toInt();
    sendEvent("LOG:Set Key 1");
  } else if(btn == "Set2") {
    if(time == "-1"){
      time = "60";
    }
    output = output + "2:Slide=" + currentX + ", Pan=" + currentR + ", Tilt=" + currentT + ", Time=" + time + ", Delay=" + delay; //Convert Revolutions back to degrees for R and T Axis
    key_X[1] = currentX;
    key_R[1] = currentR;
    key_T[1] = currentT;
    key_Set[1] = true;
    key_Time[1] = time.toInt();
    key_Delay[1] = delay.toInt();
    sendEvent("LOG:Set Key 2");
  } else if(btn == "Set3") {
    if(time == "-1"){
      time = "60";
    }
    output = output + "3:Slide=" + currentX + ", Pan=" + currentR + ", Tilt=" + currentT + ", Time=" + time + ", Delay=" + delay; //Convert Revolutions back to degrees for R and T Axis
    key_X[2] = currentX;
    key_R[2] = currentR;
    key_T[2] = currentT;
    key_Set[2] = true;
    key_Time[2] = time.toInt();
    key_Delay[2] = delay.toInt();
    sendEvent("LOG:Set Key 3");
  } else if(btn == "Set4") {
    if(time == "-1"){
      time = "60";
    }
    output = output + "4:Slide=" + currentX + ", Pan=" + currentR + ", Tilt=" + currentT + ", Time=" + time + ", Delay=" + delay; //Convert Revolutions back to degrees for R and T Axis
    key_X[3] = currentX;
    key_R[3] = currentR;
    key_T[3] = currentT;
    key_Set[3] = true;
    key_Time[3] = time.toInt();
    key_Delay[3] = delay.toInt();
    sendEvent("LOG:Set Key 4");
  } else if(btn == "Set5") {
    if(time == "-1"){
      time = "60";
    }
    output = output + "5:Slide=" + currentX + ", Pan=" + currentR + ", Tilt=" + currentT + ", Time=" + time + ", Delay=" + delay; //Convert Revolutions back to degrees for R and T Axis
    key_X[4] = currentX;
    key_R[4] = currentR;
    key_T[4] = currentT;
    key_Set[4] = true;
    key_Time[4] = time.toInt();
    key_Delay[4] = delay.toInt();
    sendEvent("LOG:Set Key 5");
  }

  //Clear Keyframes
  if(btn == "Clear1"){
    key_Set[0] = false;
    sendEvent("LOG:Cleared Key 1");
  } else if(btn == "Clear2"){
    key_Set[1] = false;
    sendEvent("LOG:Cleared Key 2");
  } else if(btn == "Clear3"){
    key_Set[2] = false;
    sendEvent("LOG:Cleared Key 3");
  } else if(btn == "Clear4"){
    key_Set[3] = false;
    sendEvent("LOG:Cleared Key 4");
  } else if(btn == "Clear5"){
    key_Set[4] = false;
    sendEvent("LOG:Cleared Key 5");
  }

  sendEvent(output);
  server.send(200, "text/plain", "OK");
}
