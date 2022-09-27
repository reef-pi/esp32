#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TokenIterator.h>
#include <UrlTokenBindings.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define OUTLET_COUNT 6
#define INLET_COUNT 4
#define JACK_COUNT 4
#define ANALOG_INPUT_COUNT 2
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8



const char *ssid = "SET_SSID";
const char *password = "SET_PASSWORD";

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
  server.on("/outlets/*", HTTP_POST, switchOutlet);
  server.on("/inlets/*", HTTP_GET, readInlet);
  server.on("/jacks/*", HTTP_POST, setJackValue);
  server.on("/analog_inputs/*", HTTP_GET, readAnalogInput);

  server.begin();
}

void loop() {
}

UrlTokenBindings parseURL(AsyncWebServerRequest *request, char templatePath[]) {
  char urlBuffer[30];
  request->url().toCharArray(urlBuffer, 30);
  int urlLength = request->url().length();
  TokenIterator templateIterator(templatePath, strlen(templatePath), '/');
  TokenIterator pathIterator(urlBuffer, urlLength, '/');
  UrlTokenBindings bindings(templateIterator, pathIterator);
  return bindings;
}

void switchOutlet(AsyncWebServerRequest *request) {
  char path[] = "/outlets/:id/:action";
  UrlTokenBindings bindings = parseURL(request, path);

  int id = String(bindings.get("id")).toInt();
  if (id < 0 || id >= OUTLET_COUNT) {
    request->send(409, "text/plain", "invalid outlet pin id");
  }
  String action = String(bindings.get("action"));
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

void readInlet(AsyncWebServerRequest *request) {
  char path[] = "/inlets/:id";
  UrlTokenBindings bindings = parseURL(request, path);
  int id = String(bindings.get("id")).toInt();
  if (id < 0 || id >= INLET_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  }
  int v = digitalRead(inletPins[id]);
  Serial.println("Inlet pin:" + String(inletPins[id]) + " Value:" + String(v));
  request->send(200, "text/plain", String(v));
}


void readAnalogInput(AsyncWebServerRequest *request) {
  char path[] = "/analog_inputs/:id";
  UrlTokenBindings bindings = parseURL(request, path);
  int id = String(bindings.get("id")).toInt();

  if (id < 0 || id > ANALOG_INPUT_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  }
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



void setJackValue(AsyncWebServerRequest *request) {
  char path[] = "/jacks/:id/:value";
  UrlTokenBindings bindings = parseURL(request, path);
  int id = String(bindings.get("id")).toInt();
  int dc = String(bindings.get("value")).toInt();
  if (id < 0 || id >= JACK_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  }
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
