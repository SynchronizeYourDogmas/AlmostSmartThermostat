/*
 * Project Home_Thermostat
 * Description:
 * Author: Peter Coyles
 * Date: 2018-01-31
 */

// TMP36 temp sensor output connected to analogue input A5

const aref_voltage 3.28         // we tie 3.3V to ARef and measure it with a multimeter!
const temp_correction -2.2     // correction of temperature compared to calibrated temperature measurement


//delay variables for loop
unsigned long delayIt = 0;
int time_interval = 15000; //15s

//TMP36 Pin Variables
int tempPin = A5;        //the analog pin the TMP36's Vout (sense) pin is connected to
                        //the resolution is 10 mV / degree centigrade with a
                        //500 mV offset to allow for negative temperatures
int tempReading;        // the analog reading from the sensor

//L293DNE variables
int startPin=A0;
int onPin=A1;
int offPin=A2;

//thermostat control variables
float fixedTemperature = 22;
float targetTemperature;
float threshold = .5; // a treshold of .5°C around target temperature

char currentTemp[16];
char targetTemp[16];
String automatic;
float tempC;

boolean manual=false;
boolean heating=false;
boolean debug=false;

//define an object for EEPROM storage
struct program {
  uin8_t version;
  char userdefined[1];
  float sunnyTemp;
  float ecoTemp;
  float nightTemp;
  byte tempMap[168];
}

void setup()
{
  // We'll send debugging information via the Serial monitor
  Serial.begin(115200);

  //setup cloud variables and functions
  Particle.variable("currentTemp",currentTemp);
  Particle.variable("targetTemp",targetTemp);
  Particle.variable("automatic",automatic);
  Particle.function("manualSwitch",switchToggle);
  Particle.function("setupTemp",setTemp);

  //initialize control pins
  pinMode(startPin, OUTPUT);
  pinMode(onPin, OUTPUT);
  pinMode(offPin, OUTPUT);

  //all OFF before start
  digitalWrite(startPin, LOW);
  digitalWrite(onPin, LOW);
  digitalWrite(offPin, LOW);

  //target temperature at startup
  targetTemperature=fixedTemperature;

  //starts in automatic mode
  automatic = "yes";

}

void loop()
{
  if (millis() - delayIt >= time_interval) {
    delayIt = millis();

    //get temperature from sensor
    tempC = getTemp();
    //convert float to string for cloud variables
    snprintf(currentTemp,sizeof(currentTemp),"%.2f",tempC);
    snprintf(targetTemp,sizeof(targetTemp),"%.2f",targetTemperature);

    if (manual) {
      //manual on-off via the web/app/override switch
    }
    else {
      //automatic temperature control
      tempC = getTemp();

      if (tempC > targetTemperature + threshold){
        //switch off
        if (heating) {
          toggle_relay(false);
        }
      }
      else if (tempC < targetTemperature - threshold){
        if (heating){
          //already heating
        }
        else {
          toggle_relay(true);
        }
      }
    }

    //better to only update this when asked?
    if (manual){
      automatic = "no";
    }
    else {
      automatic = "yes";
    }

    if (debug) {
      debug_info();
    }

  }
}

int setTemp(String temperature) {
  float proposedTemperature=temperature.toFloat();

  if (proposedTemperature >=25 or proposedTemperature <=5){
    return -1;
  }
  else {
    targetTemperature=proposedTemperature;
    manual=false;
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

int switchToggle(String command) {
  manual=true;
  if (command=="on") {
        toggle_relay(true);
        return 1;
    }
    else if (command=="off") {
        toggle_relay(false);
      return 0;
    }
    else {
        return -1;
    }
}


void toggle_relay (boolean relay_on) {
  if (relay_on) {
    digitalWrite(startPin,HIGH);
    digitalWrite(onPin,HIGH);
    digitalWrite(offPin,LOW);
    delay(100);
    digitalWrite(startPin, LOW);
    heating=true;
  }
  else {
    digitalWrite(startPin,HIGH);
    digitalWrite(onPin,LOW);
    digitalWrite(offPin,HIGH);
    delay(100);
    digitalWrite(startPin, LOW);
    heating=false;
  }

  if (debug){
    Serial.println("-------------------");
    Serial.println("Start Toggling relay...");
    if (relay_on) {
        Serial.printf("relay_on: True ");
    }
    else {
        Serial.printf("relay_on: False ");
    }
    Serial.println();
    debug_info();
    Serial.println("End Toggling relay...");
  }

}

void debug_info(){
  Serial.println("-------------------");
  Serial.printf("Debugging at %s...", Time.timeStr().c_str());
  Serial.printf("currentTemp: %s ", currentTemp);
  Serial.printf("targetTemp: %s ", targetTemp);
  if (heating) {
      Serial.printf("heating: True ");
  }
  else {
      Serial.printf("heating: False ");
  }
  if (manual) {
      Serial.printf("manual: True ");
  }
  else {
      Serial.printf("manual: False ");
  }
  Serial.println();
}
