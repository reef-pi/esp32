# esp32 

This repository provide an ESP32 firmware for [reef-pi](https://github.com/reef-pi/reef-pi) integration.

## Setup

- Use Arduino IDE for compiling and flashing ESP32 firmware. [Guide](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/)
- This firmware few external of opensource libraries. Install them before you compiling the firmware
  - AsyncTCP and ESPAsyncWebServer. [Guide](https://randomnerdtutorials.com/esp32-async-web-server-espasyncwebserver-library/)
  - TokenIterator and UrlTokenBindings. [Guide](https://techtutorialsx.com/2021/08/07/esp32-parsing-url-variables/)
  - OneWire and DallaTemperature. [Guide](https://randomnerdtutorials.com/esp32-ds18b20-temperature-arduino-ide/)


## Customizing

  - Wifi Credentials: Update the Arduino sketch with appropriate wifi SSID and credential. 
  - Pin configuration: By default the firmware ships with a specific set of pins for pwm, digitial input, analog input and one wire. Depending upon how you are wiring the physical
circuit, its likely you'll want different pin number. Update the total number of individual pins in line number 9 to 10, and the exact pin number in each category in line number 21 to 23 and 25. PWM channels (line number 24) should be of same length as the jackcount and generally from starts from 0 till jack count -1 . Adjust it accoridngly (for two jacks it should be [0,1]).

## Testing

Once compiled, uploaded the esp32 can be tested independently using curl. reef-pi esp32 integration operates on a simple http protocol.

- Outlets, used for equipment control is accessed by http post request with `/outlets/<OUTLET_ID>/action` conventions, where action can be `on` or `off
```
curl  -X POST http://<ESP32 IP>/outlets/0/on # will swtch on outlet with id 0
curl  -X POST http://<ESP32 IP>/outlets/0/off # will switch off outlet with id  0
```

- Inlets, used for digitial sensor reading (such as float switch) is accessed by http get request with `/inlets/<INLET_ID>` convention
```
curl http://<ESP32 IP>/inlets/0 # will return the digital value of inlet with id 0
```

- Jacks, generally used for PWM control (such as light) is accessed by http post request with `/jacks/<JACK_ID>/<VALUE>` convention. Where `VALUE` can be from 0 to 255.
```
curl -X POST  http://<ESP32 IP>/jacks/0/127  # will set the pwm  duty cyle of jack 0 to 127 almost  50%,.
```

- Analog inputs, used for reading analog sensors (such as temperature and ph sensors) is accessed by http get request with `/analog_inputs/<ANALOG_INPUT_ID>` convention.

```
curl http://<ESP32 IP>/analog_inputs/0 # will return the analog input id 0's value
```


## Integration with reef-pi

In reef-pi to integrate this esp32 based device, add a new driver of type `esp32` from Configiration -> Driver  tab. Provide the esp32 IP and outlet, inlet, jack, analog input counts in the driver specifics. Once created you can then declare new connectors with the driver and associate those connectors with equipment, ph module, ato module and more.


##  A special note on ds18b20 temperature sensor integration

In main reef-pi temperature tab is an independant module with ds18b20 integration on the pi itself. With esp32, all analog sensors including ds18b20 is modeled as analog input and can only be accessed through pH module. Hence ds18b20 sensors integrated through esp32 can only be read (and used for equipment control) through the ph module. In future version of reef-pi temperature and ph module will be comgined and this annoyance will be addressed.

One last note, the current esp32 firmeware uses a speical trick that the 0th analog input is treated as one wire pin. update the code if you are not using it or using a different pin for one wire integration. You can also leave it as it and just not use those pins/IDs in the reef-pi side.

