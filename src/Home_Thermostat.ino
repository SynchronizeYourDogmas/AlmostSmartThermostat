/*
 * Project Home_Thermostat
 * Description:
 * Author: Peter Coyles
 * Date: 2018-01-31
 */
//eeprom storage definitions
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

// TMP36 temp sensor output connected to analogue input A5

const float aref_voltage=3.28;         // we tie 3.3V to ARef and measure it with a multimeter!
const float temp_correction=-2.2;     // correction of temperature compared to calibrated temperature measurement
int tempPin = A5;        //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures
int tempReading;        // the analog reading from the sensor

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
float threshold = .5; // a treshold of .5°C around target temperature

char currentTemp[16];
char targetTemp[16];
String automatic;
String isHeating;
float tempC;

int heating=0;
int manual=0;

int currQuarter=0; // 0-15min 1st to 45-60min 4th
int prevQuarter=0;


//manipulate for testing
bool force_default(false);
bool force_clear(false);


void setup(){
  //time zone stuff
  Time.zone(+1);
  // We'll send debugging information via the Serial monitor
  //Serial.begin(115200);
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
  Particle.variable("currentTemp",currentTemp);
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

    //temperature control
    //get temperature from sensor
    tempC = getTemp();
    if (tempC > targetTemperature + threshold){
      //switch off
      if (heating==1) {
        toggle_relay(0);
      }
    }
    else if (tempC < targetTemperature - threshold){
      if (heating==1){
        //already heating
      }
      else {
        toggle_relay(1);
      }
    }
    //update cloud
    snprintf(currentTemp,sizeof(currentTemp),"%.2f",tempC);
    snprintf(targetTemp,sizeof(targetTemp),"%.2f",targetTemperature);
    if (heating==1){
      isHeating="yes";
    } else {
      isHeating="no";
    }
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

float getTemp() {
  tempReading = analogRead(tempPin);
  // converting that reading to voltage, which is based off the reference voltage
  float voltage = tempReading * aref_voltage;
  voltage /= 4096.0;
  //converting from 10 mv per degree wit 500 mV offset
  //to degrees ((voltage - 500mV) times 100)
  tempC = temp_correction + (voltage - 0.5) * 100 ;
  return tempC;
}

float getTempEeprom(){
  float temperature=0;
  uint8_t eepromTemp=0;
  String tempcode="";
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
  tempcode = binaryTemp.substring(i,i+2);
  if (tempcode==nightTemp){
    //night
    temperature=tempStorage.nightTemp;
  } else if (tempcode==ecoTemp) {
    //economy
    temperature=tempStorage.ecoTemp;
  } else if (tempcode==sunnyTemp) {
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
  defaultStorage.sunnyTemp=21.5;
  defaultStorage.ecoTemp=18;
  defaultStorage.nightTemp=7;
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
