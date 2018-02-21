# AlmostSmartThermostat

A home thermostat project based on the Particle Photon (https://www.particle.io/)

The sensor is a Bosch BME680 from Pimoroni (https://shop.pimoroni.com/products/bme680), communicating via the I2C-interface of the Photon.

Sensor data available via the Particle cloud:
  - Temperature (°C)
  - Relative humidity
  - Air Pressure (hPa)
  - Air Quality (gas resistance expressed in kOhm, lower is worse)
  - Altitude

A small 128x64 Oled display (SSD1306) is connect via software SPI and shows
  - Temperature (°C), Air Pressure (hPa), Air Quality alternating every 15s
  - Local time
  - Indicators for heating and desired temperature

All credits to Adafruit (https://www.adafruit.com/) for the work on the drivers and the Particle community for porting them :)

A default temperature table is stored in the simulated EEPROM of the Particle, this allows basic functionality without internet connection.
The table design:
  - 7 days times 24 hours divided in periods of 15 minutes
  - for each period there are 4 options represented by bit values
    '00' undefined
    '01' night temperature
    '10' economy temperature
    '11' sunny temperature
    for a total of 168 bytes (small enough to fit 1 cloud variable)

It is the intention to create a management webinterface in the near future.
At this moment there is only a temperature override function in the Particle console.

The logic sends signals to a L293DNE half-H driver, controlling a simple 3v/220v latching relay which switches the central heating on/off.
Adapt this for your personal situation.

Future work
- calibration
- design PCB to evolve from the breadboard design
- web interface
- making the air quality 'human readable'
- make a nice box to put everything in
