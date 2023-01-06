#include <WiFi.h>
#include "Wire.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TokenIterator.h>
#include <UrlTokenBindings.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//////////////
// SETTINGS //
//////////////
#define OUTLET_COUNT 6
#define INLET_COUNT 4
#define JACK_COUNT 4
#define DS18B20_COUNT 2
#define PH_COUNT 1
#define FLOW_METER_COUNT 1
#define ANALOG_INPUT_COUNT 2
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8


const char *ssid = "SET_SSID";
const char *password = "SET_PASSWORD";

// Pin-usage:
// Pins 1 and 3 are for Serial port, do not use them if you want to use the Serial Port for debugging
// Pins 21 and 22 are for I2C port, do not use them if you want to connect a pH circuit
// Pins 34-39 are INLET, ANALOG_INPUT and FLOW_METER ONLY
// AnalogInput only available on Pins 32-39

const int outletPins[OUTLET_COUNT] = { 5, 16, 17, 18, 19, 23 };
const int inletPins[INLET_COUNT] = { 25, 26, 36, 39 };
const int jackPins[JACK_COUNT] = { 12, 13, 14, 27 };
const int pwmChannels[JACK_COUNT] = { 0, 1, 2, 3 };
const uint8_t phAddr[PH_COUNT] = {98};
const int flowMeterPins[FLOW_METER_COUNT] = {34};
const int analogInputPins[ANALOG_INPUT_COUNT] = { 32, 33 };
const int oneWirePin = 4;

//////////
// INIT //
//////////
unsigned int flowCounts[FLOW_METER_COUNT];
unsigned int lastFlowCounts[FLOW_METER_COUNT];

OneWire oneWire(oneWirePin);
DallasTemperature ds18b20(&oneWire);
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  //while(!Serial);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  startPins(0,outletPins,OUTLET_COUNT);
  startPins(1,inletPins,INLET_COUNT);
  startPins(2,jackPins,JACK_COUNT);
  startPins(3,analogInputPins,ANALOG_INPUT_COUNT);
  startPins(4,flowMeterPins,FLOW_METER_COUNT);
  
  if (DS18B20_COUNT){
    Serial.println("Starting DS18B20 ...");
    ds18b20.begin();
  }

  if (PH_COUNT){
    Serial.println("Starting I2C ...");
    Wire.begin();
    Serial.print("\tbus-clock:");
    Serial.println(Wire.getClock());
    Serial.print("\tbus-timeout:");
    Serial.println(Wire.getTimeOut());
  }

  Serial.println("Starting WiFi..");
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    Serial.println("\tConnecting to WiFi..");
  }
  Serial.print("\tWiFi-IP:");
  Serial.println(WiFi.localIP());
  server.on("/outlets/*", HTTP_POST, switchOutlet);
  server.on("/inlets/*", HTTP_GET, readInlet);
  server.on("/jacks/*", HTTP_POST, setJackValue);
  server.on("/analog_inputs/*", HTTP_GET, readAnalogInput);

  server.begin();
}

///////////////
// MAIN LOOP //
///////////////
void loop() {
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_BUILTIN, HIGH);
    delay(950);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(950);
  }
  for (int i = 0; i < FLOW_METER_COUNT; i++) {
    lastFlowCounts[i] = flowCounts[i];
    flowCounts[i] = 0;
  }
}

///////////////
// FUNCTIONS //
///////////////
// start pins to their respective functions defined in settings
void startPins(int type, const int pins[], int count){
  const char names[][14] = {"      Outlets",
                            "       Inlets",
                            "        Jacks",
                            "Analog Inputs",
                            "   Flowmeters"};
  Serial.print("Starting ");
  Serial.print(names[type]);
  Serial.println("..");
  
  for (int i = 0; i < count; i++) {
    Serial.print("\tPin");
    Serial.println(pins[i]);

    //Init OUTPUT
    if (type == 0){
      pinMode(pins[i], OUTPUT);
    } //Init INPUT
    if (type == 1){
      pinMode(pins[i], INPUT);
    } //Init ANALOG INPUT
    if (type == 2){
      ledcSetup(pwmChannels[i], PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(jackPins[i], pwmChannels[i]);
    } //Init JACK
    if (type == 3){
      pinMode(pins[i], INPUT);
    } //Init FLOWMETER
    if (type == 4){
      pinMode(pins[i], INPUT);
      flowCounts[i] = 0;
      lastFlowCounts[i] = 0;
      attachInterruptArg(flowMeterPins[i], flowMeterCounter, &(flowCounts[i]), FALLING);
    }
  }
}

// extract parameters from URL (?)
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

void readAnalogInput(AsyncWebServerRequest *request) {
  char path[] = "/analog_inputs/:id";
  UrlTokenBindings bindings = parseURL(request, path);
  int id = String(bindings.get("id")).toInt();

  if (id < 0 || id >= DS18B20_COUNT + PH_COUNT + FLOW_METER_COUNT + ANALOG_INPUT_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  }
  float value;
  int pin, subId;  
  if (id < DS18B20_COUNT) {
    ds18b20.requestTemperatures();
    pin = oneWirePin;
    value = ds18b20.getTempCByIndex(id);
  } else if (id < DS18B20_COUNT + PH_COUNT) {
    subId = id - DS18B20_COUNT;
    pin = phAddr[subId];
    value = readPh(subId);
  } else if (id < DS18B20_COUNT + PH_COUNT + FLOW_METER_COUNT) {
    subId = id - DS18B20_COUNT - PH_COUNT;
    pin = flowMeterPins[subId];
    value = readFlowMeter(subId);
  } else {
    subId = id - DS18B20_COUNT - PH_COUNT - FLOW_METER_COUNT;
    pin = analogInputPins[subId];
    value = analogRead(analogInputPins[subId]);
  }
  Serial.println("Analog Input pin:" + String(pin) + " Value:" + String(value));
  //value == -1.0, avoiding rounding problems
  if (value < -0.5) request->send(409, "text/plain", "I2C communication error");
  else request->send(200, "text/plain", String(value));
}

float readPh(uint8_t subId){
  uint8_t bytes= 10;
  Wire.beginTransmission(phAddr[subId]);
  Wire.write(byte(0x52));
  Wire.write(byte(0x00));
  uint8_t error = Wire.endTransmission(false);
  
  uint8_t bytesReceived = Wire.requestFrom(phAddr[subId], bytes);
  if((bool)bytesReceived){
    Wire.read(); //skip first byte
    uint8_t temp[--bytesReceived]; //read the rest of the bytes
    Wire.readBytes(temp, bytesReceived);
    //log_print_buf(temp, bytesReceived);
    return String((char*)temp).toFloat();
  }
  return -1.0;
}

float readFlowMeter(uint8_t subId){
  unsigned int value = lastFlowCounts[subId];
  return float(value);
}

// counter function to increment flowmeter value
void ARDUINO_ISR_ATTR flowMeterCounter(void* arg){
  unsigned long* val = static_cast<unsigned long*>(arg);
  (*val)++;
}
