#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <StringTokenizer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPmDNS.h>

#define OUTLET_COUNT 6
#define INLET_COUNT 4
#define JACK_COUNT 4
#define ANALOG_INPUT_COUNT 2
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

const char *ssid = "SET_SSID";
const char *password = "SET_PASSWORD";
const char *host = "reef-pi-node";

const int outletPins[OUTLET_COUNT] = { 5, 16, 17, 18, 19, 23 };
const int inletPins[INLET_COUNT] = { 1, 3, 14, 36 };
const int jackPins[JACK_COUNT] = { 12, 13, 14, 27 };
const int pwmChannels[JACK_COUNT] = { 0, 1, 2, 3 };
const int analogInputPins[ANALOG_INPUT_COUNT] = { 32, 33 };
const int oneWirePin = 4;

OneWire oneWire(oneWirePin);
DallasTemperature ds18b20(&oneWire);
AsyncWebServer server(80);


void setup() {
  Serial.begin(115200);
  for (int i = 0; i < OUTLET_COUNT; i++) {
    pinMode(outletPins[i], OUTPUT);
  }

  for (int i = 0; i < INLET_COUNT; i++) {
    pinMode(inletPins[i], INPUT);
  }
  for (int i = 0; i < JACK_COUNT; i++) {
    ledcSetup(pwmChannels[i], PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(jackPins[i], pwmChannels[i]);
  }
  for (int i = 0; i < ANALOG_INPUT_COUNT; i++) {
    pinMode(analogInputPins[i], INPUT);
  }
  ds18b20.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());
  if (!MDNS.begin(host)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  MDNS.addService("http", "tcp", 80);
  server.on("/outlets/*", HTTP_POST, switchOutlet);
  server.on("/inlets/*", HTTP_GET, readInlet);
  server.on("/jacks/*", HTTP_POST, setJackValue);
  server.on("/analog_inputs/*", HTTP_GET, readAnalogInput);
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "not found");
  });
  server.begin();
}

void loop() {
}

void switchOutlet(AsyncWebServerRequest *request) {
  StringTokenizer pathParams(request->url().substring(1), "/");
  pathParams.nextToken();
  int id = pathParams.nextToken().toInt();
  if (id < 0 || id >= OUTLET_COUNT) {
    request->send(409, "text/plain", "invalid outlet pin id");
  } else {
    String action = pathParams.nextToken();
    Serial.println("Outlet Pin:" + String(outletPins[id]) + ", Action:" + action);
    if (action.equals("on")) {
      digitalWrite(outletPins[id], HIGH);
      request->send(200, "text/plain", "high");
    } else if (action.equals("off")) {
      digitalWrite(outletPins[id], LOW);
      request->send(200, "text/plain", "low");
    } else {
      request->send(409, "text/plain", "unrecognized action");
    }
  }
}

void readInlet(AsyncWebServerRequest *request) {
  StringTokenizer pathParams(request->url().substring(1), "/");
  pathParams.nextToken();
  int id = pathParams.nextToken().toInt();
  if (id < 0 || id >= INLET_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  } else {
    int v = digitalRead(inletPins[id]);
    Serial.println("Inlet pin:" + String(inletPins[id]) + " Value:" + String(v));
    request->send(200, "text/plain", String(v));
  }
}


void readAnalogInput(AsyncWebServerRequest *request) {
  StringTokenizer pathParams(request->url().substring(1), "/");
  pathParams.nextToken();
  int id = pathParams.nextToken().toInt();
  if (id < 0 || id > ANALOG_INPUT_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  } else {
    float value;
    if (id == 0) {
      ds18b20.requestTemperatures();
      value = ds18b20.getTempCByIndex(0);
    } else {
      value = analogRead(analogInputPins[id]);
    }
    Serial.println("Analog Input pin:" + String(analogInputPins[id]) + " Value:" + String(value));
    request->send(200, "text/plain", String(value));
  }
}



void setJackValue(AsyncWebServerRequest *request) {
  StringTokenizer pathParams(request->url().substring(1), "/");
  pathParams.nextToken();
  int id = pathParams.nextToken().toInt();
  int dc = pathParams.nextToken().toInt();
  if (id < 0 || id >= JACK_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  } else {
    if (dc > 255) {
      dc = 255;
    }
    if (dc < 0) {
      dc = 0;
    }
    Serial.println("PWM Pin" + String(jackPins[id]) + " DutyCycle:" + String(dc));
    ledcWrite(pwmChannels[id], dc);
    request->send(200, "text/plain", String(dc));
  }
}
