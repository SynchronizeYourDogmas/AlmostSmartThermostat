/*
 * Project Home_Thermostat
 * Description:
 * Author: Peter Coyles
 * Date: 2018-01-31
 */
//for OLED display
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

//bosch BME680 sensor
#include "Adafruit_BME680.h"

#define BME_SCK A0
#define BME_MISO A1
#define BME_MOSI A2
#define BME_CS A3

#define SEALEVELPRESSURE_HPA (1027.1)

#define BME680_ADDRESS (0x76)

Adafruit_BME680 bme; // I2C

double temperatureInC = 0;
double relativeHumidity = 0;
double pressureHpa = 0;
double gasResistanceKOhms = 0;
double approxAltitudeInM = 0;

//eeprom storage
uint8_t firstValue; //reserved for first value in EEPROM storage
struct MyStorage{   //temperature table and version info
  uint8_t versionMain;
  uint8_t versionSub;
  uint8_t userdefined;
  float sunnyTemp;
  float ecoTemp;
  float nightTemp;
  time_t updateTime;
  byte tempMap[168];
};
MyStorage tempStorage={};

//bit representation of temperature codes
String nightTemp = "01";
String ecoTemp = "10";
String sunnyTemp = "11";

//L293DNE variables for controlling the latching relay
int startPin=A0;
int onPin=A1;
int offPin=A2;

//delay variables for loop
unsigned long delayIt = 0;
unsigned int time_interval = 15000; //15s

//thermostat control variables
float fixedTemperature = 22;
float targetTemperature;
float threshold=.5;

char currentTemp[16];
char targetTemp[16];
char humidity[16];
char airQuality[16];
char airPressure[16];
String automatic;
String isHeating;
String tempCode;
String sensor_context="Temperature";

int heating=0;
int manual=0;

int currQuarter=0; // 0-15min 1st to 45-60min 4th
int prevQuarter=0;

// OLED display 128*64
// If using software SPI (the default case):
#define OLED_MOSI   D5
#define OLED_CLK    D6
#define OLED_DC     D2
#define OLED_CS     D3
#define OLED_RESET  D4
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

//------------------------------------------------------------------------------
// Files generated by LCD Assistant
// http://en.radzio.dxp.pl/bitmap_converter/
//------------------------------------------------------------------------------

static const unsigned char sunny_small_bmp [] = {
  0x00, 0x40, 0x20, 0x80, 0x10, 0x81, 0x10, 0x86, 0x0B, 0x68, 0x04, 0x30, 0x88, 0x18, 0x48, 0x0E,
  0x30, 0x09, 0x08, 0x08, 0x08, 0x10, 0x04, 0x30, 0x0B, 0xC8, 0x71, 0x04, 0x01, 0x04, 0x02, 0x00
};
const unsigned char eco_bmp [] = {
0x00, 0x00, 0x00, 0x00, 0x01, 0xF0, 0x02, 0x08, 0x04, 0x04, 0x04, 0x00, 0x1F, 0xE0, 0x04, 0x00,
0x04, 0x00, 0x04, 0x00, 0x1F, 0xE0, 0x04, 0x00, 0x04, 0x04, 0x02, 0x08, 0x01, 0xF0, 0x00, 0x00
};
const unsigned char night_bmp [] = {
0x03, 0xE0, 0x07, 0x00, 0x0E, 0x00, 0x1E, 0x00, 0x1C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x7C, 0x00,
0x7C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x1C, 0x00, 0x1E, 0x00, 0x0E, 0x00, 0x05, 0x00, 0x03, 0xE0
};
const unsigned char heating_bmp [] = {
0x00, 0x00, 0x00, 0x00, 0x10, 0x84, 0x10, 0x84, 0x21, 0x08, 0x21, 0x08, 0x29, 0x4A, 0x4A, 0x52,
0x52, 0x94, 0x10, 0x84, 0x10, 0x84, 0x21, 0x08, 0x21, 0x08, 0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE
};
const unsigned char manual_bmp [] = {
0x00, 0x80, 0x04, 0x90, 0x04, 0x90, 0x24, 0x90, 0x24, 0x90, 0x24, 0x92, 0x24, 0x92, 0x20, 0x02,
0x00, 0x02, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x20, 0x04, 0x18, 0x18, 0x0C, 0x30, 0x03, 0xC0
};


//manipulate for testing or loading new defaults
bool force_default(true);
bool force_clear(false);


void setup(){
  //time zone stuff
  Time.zone(+1);
  // We'll send debugging information via the Serial monitor
  Serial.begin(115200);
  //oled splash screen etcetera
  oled_init();

  //checking in with the BME680 sensor
  if (!bme.begin(0x76)) {
    Particle.publish("Log", "Could not find a valid BME680 sensor, check wiring!");
  } else {
    Particle.publish("Log", "bme.begin() success =)");
    // Set up oversampling and filter initialization
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320*C for 150 ms

    Particle.variable("currentTemp", currentTemp);
    Particle.variable("humidity", humidity);
    Particle.variable("airPressure", airPressure);
    Particle.variable("airQuality", airQuality);
    Particle.variable("altitude", &approxAltitudeInM, DOUBLE);
  }

  //clear for testing purposes only
  if (force_clear){
    EEPROM.clear();
  }
  //force loading of defaults in eeprom
  if (force_default){
    factory_defaults();
  }
  // when started up for the first time or after EEPROM was cleared
  // the defaults should be loaded
  firstValue=EEPROM.read(0);
  if (firstValue==0xFF){
    factory_defaults();
  }
  //setup cloud variables and functions
  Particle.variable("targetTemp",targetTemp);
  Particle.variable("automatic",automatic);
  Particle.variable("Heating",isHeating);
  Particle.function("autoSwitch",switchToggle);
  Particle.function("overrideTemp",overrideTemp);
  //initialize control pins
  pinMode(startPin, OUTPUT);
  pinMode(onPin, OUTPUT);
  pinMode(offPin, OUTPUT);
  //all OFF before start
  digitalWrite(startPin, LOW);
  digitalWrite(onPin, LOW);
  digitalWrite(offPin, LOW);

  //starts in automatic mode
  automatic = "yes";
  toggle_relay(0);

}

void loop() {
  //check every delayIt(=15s), better than using delay()
  if (millis() - delayIt >= time_interval) {
    delayIt = millis();

    //temperature is set via cloud function overrideTemp (manual=1)
    //or is read from EEPROM
    currQuarter = getCurrentQuarter();
    if (currQuarter!=prevQuarter){
      prevQuarter=currQuarter;
      if (manual!=1){
        //get temperature from EEPROM
        targetTemperature = getTempEeprom();
      }
    }

    //the BME680 sensor reading
    get_BME680_data();

    //temperature control
    if (temperatureInC > targetTemperature + threshold){
      //switch off
      if (heating==1) {
        toggle_relay(0);
      }
    }
    else if (temperatureInC < targetTemperature - threshold){
      if (heating==1){
        //already heating
      }
      else {
        toggle_relay(1);
      }
    }

    //update cloud
    snprintf(currentTemp,sizeof(currentTemp),"%.1f",temperatureInC);
    snprintf(targetTemp,sizeof(targetTemp),"%.1f",targetTemperature);
    snprintf(humidity,sizeof(humidity),"%.0f",relativeHumidity);
    snprintf(airQuality,sizeof(airQuality),"%.1f",gasResistanceKOhms);
    snprintf(airPressure,sizeof(airPressure),"%.1f",pressureHpa);

    if (heating==1){
      isHeating="yes";
    } else {
      isHeating="no";
    }
    //update display
    display.clearDisplay();
    //value switches between temp, humidity and air quality
    if (sensor_context=="Temperature"){
      oled_value(sensor_context,currentTemp);
      sensor_context="Humidity";
    }
    else if(sensor_context=="Humidity") {
      oled_value(sensor_context,humidity);
      sensor_context="AirQuality";
    }
    else if(sensor_context=="AirQuality") {
      oled_value(sensor_context,airQuality);
      sensor_context="Temperature";
    }
    else {
      sensor_context="Temperature";
    }
    oled_targetTemp();  //indicator for target temperature
    oled_time();
    oled_pressure();
    display.display();
  }
}

// converts minutes to quarters of an hour: 0-15min 1st to 45-60min 4th
// 1 quarter is the resolution at which temperature is programmed in the EEPROM
int getCurrentQuarter(){
  int q=(Time.minute()/15)+1;
  return q;
}
// converts a bit representation of an hour in the temeperature table to a byte  for storage
// e.g. '10101111', 1st & 2nd quarter '10' ecotemp, 3th & 4th quarter '11' sunnytemp
// translates to a value of 175
int bytestring_to_int(const char bitStr[8]) {
	unsigned char val = 0;
	int toShift = 0;
	for (int i = 7; i >= 0; i--) {
		if (bitStr[i] == '1') {
			val = (1 << toShift) | val;
		}
		toShift++;
	}
	return val;
}

int overrideTemp(String sTemp) {
  float proposedTemperature=sTemp.toFloat();
  if (proposedTemperature >=28 or proposedTemperature <=5){
    //do not allow extreme temp settings
    return -1;
  }
  else {
    //accept the input
    targetTemperature=proposedTemperature;
    automatic="no";
    manual=1;
    return 1;
  }
}

void get_BME680_data(){
  if (! bme.performReading()) {
    Particle.publish("Log", "Failed to perform reading :(");
  }
  else {
    temperatureInC = bme.temperature - 2.5; //dirty hack awaiting proper calibration
    relativeHumidity = bme.humidity;
    pressureHpa = bme.pressure / 100.0;
    gasResistanceKOhms = bme.gas_resistance / 1000.0;
    approxAltitudeInM = bme.readAltitude(SEALEVELPRESSURE_HPA);
  }
}

float getTempEeprom(){
  float temperature=0;
  uint8_t eepromTemp=0;
  tempCode="";
  String binaryTemp="";
  String zerobyte = "00000000";
  int i=0;
  //read from EEPROM
  EEPROM.get(1,tempStorage);
  //determine index in array
  int x=(Time.weekday()-1)*24+Time.hour(); //weekday 1=Sunday
  eepromTemp=tempStorage.tempMap[x];
  binaryTemp= String(eepromTemp,BIN); //leading zeroes dropped
  binaryTemp= zerobyte.substring(binaryTemp.length()) + binaryTemp;
  //take  corresponding code
  i=(getCurrentQuarter()-1)*2;
  tempCode = binaryTemp.substring(i,i+2);
  if (tempCode==nightTemp){
    //night
    temperature=tempStorage.nightTemp;
  } else if (tempCode==ecoTemp) {
    //economy
    temperature=tempStorage.ecoTemp;
  } else if (tempCode==sunnyTemp) {
    //sunny
    temperature=tempStorage.sunnyTemp;
  } else {
    //something is wrong
    temperature=-1;
  }
  return temperature;
}

void factory_defaults(){
  //initialisation
  MyStorage defaultStorage={};
  defaultStorage.versionMain=0; // version 0.01
  defaultStorage.versionSub=1;
  defaultStorage.userdefined=0; // 0 is default ; > 0 is userdefined
  defaultStorage.sunnyTemp=22;
  defaultStorage.ecoTemp=20;
  defaultStorage.nightTemp=16;
  //Time is an illusion, lunchtime double so.
  time_t writeTime=Time.now();
  defaultStorage.updateTime = writeTime;
  /*7 days times 24 hours divided in periods of 15 minutes
	for each period there are 4 options represented by bit values
			'00' undefined
			'01' nightTemp
			'10' ecoTemp
			'11' sunnyTemp
      for a total of 168 bytes (small enough to be 1 cloud variable) */
  String uur="";
  for (int i = 0; i < 7; i++) {
    int u = 0;
    while (u < 24) {
      /*tot 05u nacht*/
     	if (u < 5) {
     	  uur = "01010101";
     	}
     	/*tot 06u eco*/
     	else if (u < 6) {
     	  uur = "10101010";
     	}
     	else if (u < 22) {
     	  uur = "11111111";
     	}
     	else if (u < 23) {
     		uur = "10101010";
     	}
     	else if (u < 24) {
     		uur = "01010101";
     	}
     	defaultStorage.tempMap[i * 24 + u] = bytestring_to_int(uur);
 			u++;
 		}
 	}
  //fill EEPROM and set first value<>0xFF(=is value when cleared)
  EEPROM.put(1,defaultStorage);
  EEPROM.write(0,0);
}

int switchToggle(String command) {
    if (command=="on") {
        //switch to automatic mode
        automatic="yes";
        manual=0;
        //get temperature from EEPROM
        targetTemperature = getTempEeprom();
        if (targetTemperature==-1){
          automatic="no";
          manual=1;

        } else {
          return 1;
        }
    }
    else if (command=="off") {
      automatic="no";
      manual=1;
      return 0;
    }
    else {
        //command not recognized
        return -1;
    }
}

void toggle_relay (int relay_on) {
  if (relay_on==1) {
    digitalWrite(startPin,HIGH);
    digitalWrite(onPin,HIGH);
    digitalWrite(offPin,LOW);
    delay(150);
    digitalWrite(startPin, LOW);
    heating=1;
  }
  else {
    digitalWrite(startPin,HIGH);
    digitalWrite(onPin,LOW);
    digitalWrite(offPin,HIGH);
    delay(150);
    digitalWrite(startPin, LOW);
    heating=0;
  }
}

void oled_init() {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  // init done

  display.display(); // show splashscreen
  delay(3000);
  display.clearDisplay();   // clears the screen and buffer
}

void oled_value(String oledContext,String oledValue){
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(oledContext);
  display.setTextSize(3);
  display.setCursor(2,14);
  display.println(oledValue);
  if (oledContext=="Temperature"){
    if (heating==1){
      display.drawBitmap(104, 24,  heating_bmp, 16, 16, WHITE);
    }
  else if (oledContext=="Humidity"){
    // imagine something :)
    }
  }
}

void oled_targetTemp(){
  if (manual!=1){
    if (tempCode==nightTemp){
      display.drawBitmap(104, 2,  night_bmp, 16, 16, WHITE);
    }
    else if (tempCode==ecoTemp) {
      display.drawBitmap(104, 2,  eco_bmp, 16, 16, WHITE);
    }
    else if (tempCode==sunnyTemp) {
      display.drawBitmap(104, 2,  sunny_small_bmp, 16, 16, WHITE);
    }
    else {
      //something is wrong
    }
  }
  else {
    display.drawBitmap(104, 2,  manual_bmp, 16, 16, WHITE);
  }
}

void oled_time(){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,51);
  display.println(Time.format("%H:%M"));
}

void oled_pressure(){
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(65,57);
  display.println(airPressure);
  display.setCursor(104,57);
  display.println("hPa");
}
