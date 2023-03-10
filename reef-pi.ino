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

const uint8_t OUTLET_PINS[OUTLET_COUNT] = { 5, 16, 17, 18, 19, 23 };
const uint8_t INLET_PINS[INLET_COUNT] = { 25, 26, 36, 39 };
const uint8_t JACK_PINS[JACK_COUNT] = { 12, 13, 14, 27 };
const uint8_t PWM_CHANNELS[JACK_COUNT] = { 0, 1, 2, 3 };
const uint8_t PH_ADDR[PH_COUNT] = {98};
const uint8_t FLOW_METER_PINS[FLOW_METER_COUNT] = {34};
const uint8_t ANALOG_INPUT_PINS[ANALOG_INPUT_COUNT] = { 32, 33 };
const uint8_t ONE_WIRE_PINS[1] = {4};

//////////
// INIT //
//////////
volatile unsigned int flowCounts[FLOW_METER_COUNT];
unsigned int lastFlowCounts[FLOW_METER_COUNT];

OneWire oneWire(ONE_WIRE_PINS[0]);
DallasTemperature ds18b20(&oneWire);
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  //while(!Serial);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  startPins(0,OUTLET_PINS,OUTLET_COUNT);
  startPins(1,INLET_PINS,INLET_COUNT);
  startPins(2,JACK_PINS,JACK_COUNT);
  if (DS18B20_COUNT){
    startPins(3,ONE_WIRE_PINS,DS18B20_COUNT);
  }
  if (PH_COUNT){
    startPins(4,PH_ADDR,PH_COUNT);
  }
  if (FLOW_METER_COUNT){
    startPins(5,FLOW_METER_PINS,FLOW_METER_COUNT);
  }
  if (ANALOG_INPUT_COUNT){
    startPins(6,ANALOG_INPUT_PINS,ANALOG_INPUT_COUNT);
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
  for (size_t i = 0; i < FLOW_METER_COUNT; i++) {
    lastFlowCounts[i] = flowCounts[i];
    flowCounts[i] = 0;
  }
}

///////////////
// FUNCTIONS //
///////////////
// start pins to their respective functions defined in settings
void startPins(uint8_t type, const uint8_t PINS[], uint8_t count){
  const char PIN_NAMES[][14] = {"      Outlets",
                                "       Inlets",
                                "        Jacks",
                                "      DS18B20",
                                "          I2C",
                                "   Flowmeters",
                                "Analog Inputs"};
  size_t subIDs[] = { 0,
                      DS18B20_COUNT,
                      PH_COUNT,
                      FLOW_METER_COUNT,
                      ANALOG_INPUT_COUNT};
  for (size_t i=1; i<5; i++){
    subIDs[i] += subIDs[i-1];
  }
  Serial.print("Starting ");
  Serial.print(PIN_NAMES[type]);
  Serial.println("..");
  
  for (size_t i = 0; i < count; i++) {
    Serial.print("\tPin");
    Serial.println(PINS[i]);

    //Init OUTPUT
    if (type == 0){
      pinMode(PINS[i], OUTPUT);
    } 
    //Init INPUT
    if (type == 1){
      pinMode(PINS[i], INPUT);
    } 
    //Init JACKS
    if (type == 2){
      ledcSetup(PWM_CHANNELS[i], PWM_FREQ, PWM_RESOLUTION);
      ledcAttachPin(PINS[i], PWM_CHANNELS[i]);
    } 
    //Init DS18B20
    if (type == 3){
      ds18b20.begin();
    } 
    // start I2C
    if (type == 4){
      Wire.begin();
      Serial.print("\tbus-clock:");
      Serial.println(Wire.getClock());
      Serial.print("\tbus-timeout:");
      Serial.println(Wire.getTimeOut());
    } 
    //Init FLOWMETER
    if (type == 5){
      pinMode(PINS[i], INPUT);
      flowCounts[i] = 0;
      lastFlowCounts[i] = 0;
      attachInterruptArg(PINS[i], flowMeterCounter, &(flowCounts[i]), FALLING);
    } 
    //Init ANALOGINPUT
    if (type == 6){
      pinMode(PINS[i], INPUT);
    }
    //Message for Pin IDs
    if (type > 2){
      Serial.print("\tPin IDs:");
      Serial.print(subIDs[type-2]);
      Serial.print("-");
      Serial.println(subIDs[type-1]-1);
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
  const char* binding_id = bindings.get("id");
  if (binding_id == NULL){
    Serial.println("NULL pointer error");
    request->send(409, "text/plain", "NULL pointer error"); 
    return;    
  }
  int id = String(binding_id).toInt();
  
  if (id < 0 || id >= OUTLET_COUNT) {
    request->send(409, "text/plain", "invalid outlet pin id");
  }
  String action = String(bindings.get("action"));
  Serial.println("Outlet Pin:" + String(OUTLET_PINS[id]) + ", Action:" + action);
  if (action.equals("on")) {
    digitalWrite(OUTLET_PINS[id], HIGH);
    request->send(200, "text/plain", "high");
  } else if (action.equals("off")) {
    digitalWrite(OUTLET_PINS[id], LOW);
    request->send(200, "text/plain", "low");
  } else {
    request->send(409, "text/plain", "unrecognized action");
  }
}

void readInlet(AsyncWebServerRequest *request) {
  char path[] = "/inlets/:id";
  UrlTokenBindings bindings = parseURL(request, path);
  const char* binding_id = bindings.get("id");
  if (binding_id == NULL){
    Serial.println("NULL pointer error");
    request->send(409, "text/plain", "NULL pointer error"); 
    return;    
  }
  int id = String(binding_id).toInt();
  if (id < 0 || id >= INLET_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
  }

  int v = digitalRead(INLET_PINS[id]);
  Serial.println("Inlet pin:" + String(INLET_PINS[id]) + " Value:" + String(v));
  request->send(200, "text/plain", String(v));
}

void setJackValue(AsyncWebServerRequest *request) {
  char path[] = "/jacks/:id/:value";
  UrlTokenBindings bindings = parseURL(request, path);

  const char* binding_id = bindings.get("id");
  if (binding_id == NULL){
    Serial.println("NULL pointer error");
    request->send(409, "text/plain", "NULL pointer error"); 
    return;    
  }
  int id = String(binding_id).toInt();

  const char* binding_dc = bindings.get("value");
  if (binding_dc == NULL){
    Serial.println("NULL pointer error");
    request->send(409, "text/plain", "NULL pointer error"); 
    return;    
  }
  int dc = String(binding_dc).toInt();
  
  if (id < 0 || id >= JACK_COUNT) {
    request->send(409, "text/plain", "invalid inlet pin id");
    return;
  }
  if (dc > 255) {
    dc = 255;
  }
  if (dc < 0) {
    dc = 0;
  }
  Serial.println("PWM Pin" + String(JACK_PINS[id]) + " DutyCycle:" + String(dc));
  ledcWrite(PWM_CHANNELS[id], dc);
  request->send(200, "text/plain", String(dc));
}

void readAnalogInput(AsyncWebServerRequest *request) {
  char path[] = "/analog_inputs/:id";
  UrlTokenBindings bindings = parseURL(request, path);
  const char* binding_id = bindings.get("id");
  if (binding_id == NULL){
    Serial.println("NULL pointer error");
    request->send(409, "text/plain", "NULL pointer error"); 
    return;    
  }
  int id = String(binding_id).toInt();

  if (id < 0 || id > DS18B20_COUNT + PH_COUNT + FLOW_METER_COUNT + ANALOG_INPUT_COUNT - 1) {
    request->send(409, "text/plain", "invalid inlet pin id");
    return;
  }
  float value;
  int pin, subId;  
  if (id < DS18B20_COUNT) {
    ds18b20.requestTemperatures();
    pin = ONE_WIRE_PINS[0];
    value = ds18b20.getTempCByIndex(id);
  } else if (id < DS18B20_COUNT + PH_COUNT) {
    subId = id - DS18B20_COUNT;
    pin = PH_ADDR[subId];
    value = readPh(subId);
  } else if (id < DS18B20_COUNT + PH_COUNT + FLOW_METER_COUNT) {
    subId = id - DS18B20_COUNT - PH_COUNT;
    pin = FLOW_METER_PINS[subId];
    value = readFlowMeter(subId);
  } else {
    subId = id - DS18B20_COUNT - PH_COUNT - FLOW_METER_COUNT;
    pin = ANALOG_INPUT_PINS[subId];
    value = analogRead(ANALOG_INPUT_PINS[subId]);
  }
  Serial.println("Analog Input pin:" + String(pin) + " Value:" + String(value));
  //value == -1.0, avoiding rounding problems
  if (value < -0.5) request->send(409, "text/plain", "I2C communication error");
  else request->send(200, "text/plain", String(value));
}

float readPh(uint8_t subId){
  uint8_t bytes= 10;
  Wire.beginTransmission(PH_ADDR[subId]);
  Wire.write(byte(0x52));
  Wire.write(byte(0x00));
  uint8_t error = Wire.endTransmission(false);
  
  uint8_t bytesReceived = Wire.requestFrom(PH_ADDR[subId], bytes);
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
  unsigned int* val = static_cast<unsigned int*>(arg);
  (*val)++;
}
