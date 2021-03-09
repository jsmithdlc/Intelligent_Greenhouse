#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <CircularBuffer.h>
SoftwareSerial espSerial =  SoftwareSerial(2,3);      // arduino RX pin=2  arduino TX pin=3    connect the arduino RX pin to esp8266 module TX pin   -  connect the arduino TX pin to esp8266 module RX pin

// ---LAMP---
const int LAMP = 11;
const int night_thr = 600;
const int day_thr = 800;
int low_light_counter = 0;
int high_light_counter = 0;
boolean its_day = true;
// ---VENTILATOR---
const int VENT = 10;

// ---WATER_PUMP---
const int WATER_PUMP = 12;
int recent_watering = 20;
const int max_tank_capacity = 4400; // [ml] máximum volume of water in tank
int water_level = 0;
const int watering_vol = 50; // [ml] ammount of water spent on watering plant each time
const int watering_time = 4406; // [ms] ammount of time spent watering plant each time
int water_level_addr = 0; // EEPROM address for first byte of water_tank_level (consumes 2 bytes in total)

//--- SETTINGS WIFI ---
const char* ssid = "WIFI ID";                   // WIFI network SSID
const char* password = "WIFI PASS";          // WIFI network PASSWORD
int port=8000;                              // Virtuino default Server port
const char* serverIP = "VIRTUINO APP IP";    // The three first numbers have to be the same with the router IP

// ---TEMPERATURE SENSOR---
const int R2_TEMP = 10000; // Resistencia conectada en serie a termistor
const float C_TEMP = 0.6507394466*pow(10,-7);
const float B_TEMP = 2.384641754*pow(10,-4);
const float A_TEMP = 1.106836861*pow(10,-3);
float temp;
float temp_volt;
float R1_TEMP;
const int TEMP_SENSOR = 0;
float temp_read_array[10];
// Temperature Thresholds
const int temp_low_thr = 21;
const int temp_high_thr = 30;
//const int temp_high_thr = 25;
///////////////////////////
// ---HUMIDITY SENSOR---
const int MOIST_SENSOR = 1;
float moist;
int moist_read_array[10];
// Humidity Thresholds
const int moist_thr = 350;
///////////////////////////
// ---LIGHT SENSOR---
const int LIGHT_SENSOR = A2;
int light_read_array[10];
// Light Thresholds
int light_low_thr = 990;
int light_high_thr = 1000;
//int light_low_thr = 800;
//int light_high_thr = 900;
float light;

// Buffer for sensor values definition
const int cbuffer_capacity = 10;
CircularBuffer<float,cbuffer_capacity> tempBuffer;
CircularBuffer<int,cbuffer_capacity> lightBuffer;
CircularBuffer<int,cbuffer_capacity> moistBuffer;

//-------------------------------------------------------------

//-------------VirtuinoCM  Library and settings --------------
#include "VirtuinoCM.h"
VirtuinoCM virtuino;               
#define V_memory_count 32  // the size of V memory.
float V[V_memory_count];   // This array is synchronized with Virtuino V memory. 

boolean debug = true;              // set this variable to false on the finale code to decrease the request time.

float LAMP_V1_lastValue=0;
float VENT_V2_lastValue=0;
float PUMP_V3_lastValue=0;
void setup() {
  // Sets the operation mode of actuators as outputs
  pinMode(LAMP,OUTPUT);
  pinMode(VENT,OUTPUT);
  pinMode(WATER_PUMP,OUTPUT);
  // Sets the operation mode of sensors as inputs
  pinMode(TEMP_SENSOR,INPUT);
  pinMode(MOIST_SENSOR,INPUT);
  pinMode(LIGHT_SENSOR,INPUT);
  // Initializes lamp and water pump in high state, which means 
  // they are turned off
  digitalWrite(LAMP,HIGH);
  digitalWrite(WATER_PUMP,HIGH);
  // Read water level from EEPROM
  water_level = readIntFromEEPROM(water_level_addr);

  if (debug) {
    Serial.begin(9600);
    while (!Serial) continue;
  }


  // Starts serial communication with ESP-01 Wifi component
  espSerial.begin(9600);  
  espSerial.setTimeout(50);
  
  // Starts Virtuino. Sets the buffer to 256, with which 28 pins can be controlled
  virtuino.begin(onReceived,onRequested,256);  
  // Clave de acceso para Virtuino
  //virtuino.key="1234";

  connectToWiFiNetwork();
   
}
void loop() {
  // Necessary for communicating with Virtuino
  virtuinoRun();
  if(tempBuffer.isFull()){
    tempBuffer.shift();
    moistBuffer.shift();
    lightBuffer.shift();
    tempBuffer.push(tempRead());
    moistBuffer.push(analogRead(MOIST_SENSOR));
    lightBuffer.push(analogRead(LIGHT_SENSOR));
  }
  else{
    tempBuffer.push(tempRead());
    moistBuffer.push(analogRead(MOIST_SENSOR));
    lightBuffer.push(analogRead(LIGHT_SENSOR));
  }
  temp = getTempBufferMean();
  light = getLightBufferMean();
  check_daylight(light);
  moist = getMoistBufferMean();
  
  Serial.println("#############");
  Serial.println("La temperatura es: ");
  Serial.print(temp);
  Serial.println(" grados Celcius");
  Serial.println("La humedad relativa es: ");
  Serial.print(moist);
  Serial.println("");
  Serial.println("El nivel de iluminación es: ");
  Serial.print(light);
  Serial.println("");
  Serial.println("#############");
  if(V[31]){
    lightControl(light);
    tempControl(temp);
    if(recent_watering < 20){
      recent_watering += 1;
    }
    else{
      waterControl(moist);
    }
  }
  // Changes the lamp current state if V[1] is activated from 
  // the application
  if (V[1]!=LAMP_V1_lastValue) {            
    digitalWrite(LAMP,!V[1]);
    LAMP_V1_lastValue=V[1];   
  }

  // Changes the ventilator current state if V[2] is activated from 
  // the application
  if (V[2]!=VENT_V2_lastValue) {          
    digitalWrite(VENT,V[2]);
    VENT_V2_lastValue=V[2];                   
  }

  // Activates the water pump for approx 4 seconds if V[3] is activated 
  // from the application
  if (V[3]==1) {          
    digitalWrite(WATER_PUMP,LOW);
    vDelay(watering_time);
    digitalWrite(WATER_PUMP,HIGH);
    water_level = water_level - watering_vol;
    writeIntIntoEEPROM(water_level_addr,water_level);                  
  }

  // Restores water_level in EEPROM to max value since user 
  // replenished it
  if(V[7]==1){
    writeIntIntoEEPROM(water_level_addr, max_tank_capacity);
    water_level = max_tank_capacity;
  }

  Serial.print("NIVEL DE AGUA: ");
  Serial.println(water_level);
  V[4] = temp;
  V[5] = map(moist,0,1023,100,0);
  V[6] = map(light,0,1023,0,100);
  V[8] = water_level;

  // Delay of 1 second between iterations 
  vDelay(1000);

}

//================================================ connectToWiFiNetwork
void connectToWiFiNetwork(){
    Serial.println("Connecting to "+String(ssid));
    while (espSerial.available()) espSerial.read();
    espSerial.println("AT+GMR");       // print firmware info
    waitForResponse("OK",1000);
    espSerial.println("AT+CWMODE=1");  // configure as client
    waitForResponse("OK",1000);
    espSerial.print("AT+CWJAP=\"");    // connect to your WiFi network
    espSerial.print(ssid);
    espSerial.print("\",\"");
    espSerial.print(password);
    espSerial.println("\"");
    waitForResponse("OK",10000);
    espSerial.print("AT+CIPSTA=\"");   // set IP
    espSerial.print(serverIP);
    espSerial.println("\"");   
    waitForResponse("OK",5000);
    espSerial.println("AT+CIPSTA?");
    waitForResponse("OK",3000); 
    espSerial.println("AT+CIFSR");           // get ip address
    waitForResponse("OK",1000);
    espSerial.println("AT+CIPMUX=1");         // configure for multiple connections   
    waitForResponse("OK",1000);
    espSerial.print("AT+CIPSERVER=1,");
    espSerial.println(port);
    waitForResponse("OK",1000);
}

//============================================================== onCommandReceived
//==============================================================
/* This function is called every time Virtuino app sends a request to server to change a Pin value
 * The 'variableType' can be a character like V, T, O  V=Virtual pin  T=Text Pin    O=PWM Pin 
 * The 'variableIndex' is the pin number index of Virtuino app
 * The 'valueAsText' is the value that has sent from the app   */
 void onReceived(char variableType, uint8_t variableIndex, String valueAsText){     
    if (variableType=='V'){
        float value = valueAsText.toFloat();        // convert the value to float. The valueAsText have to be numerical
        if (variableIndex<V_memory_count) V[variableIndex]=value;              // copy the received value to arduino V memory array
    }
}

//==============================================================
/* This function is called every time Virtuino app requests to read a pin value*/
String onRequested(char variableType, uint8_t variableIndex){     
    if (variableType=='V') {
    if (variableIndex<V_memory_count) return  String(V[variableIndex]);   // return the value of the arduino V memory array
  }
  return "";
}


 //==============================================================
void virtuinoRun(){
if(espSerial.available()){
      virtuino.readBuffer = espSerial.readStringUntil('\n');
      if (debug) Serial.print('\n'+virtuino.readBuffer);
      int pos=virtuino.readBuffer.indexOf("+IPD,");
      if (pos!=-1){
            int connectionId = virtuino.readBuffer.charAt(pos+5)-48;  // get connection ID
            int startVirtuinoCommandPos = 1+virtuino.readBuffer.indexOf(":");
            virtuino.readBuffer.remove(0,startVirtuinoCommandPos);
            String* response= virtuino.getResponse();    // get the text that has to be sent to Virtuino as reply. The library will check the inptuBuffer and it will create the response text
            if (debug) Serial.println("\nResponse : "+*response);
            if (response->length()>0) {
              String cipSend = "AT+CIPSEND=";
              cipSend += connectionId;
              cipSend += ",";
              cipSend += response->length();
              cipSend += "\r\n";
              while(espSerial.available()) espSerial.read();    // clear espSerial buffer 
              for (int i=0;i<cipSend.length();i++) espSerial.write(cipSend.charAt(i));
              if (waitForResponse(">",1000)) espSerial.print(*response);
              waitForResponse("OK",1000);
            }
            espSerial.print("AT+CIPCLOSE=");espSerial.println(connectionId);
       }// (pos!=-1)
         
  } // if espSerial.available
      
}

//=================================================== waitForResponse
boolean waitForResponse(String target1,  int timeout){
    String data="";
    char a;
    unsigned long startTime = millis();
    boolean rValue=false;
    while (millis() - startTime < timeout) {
        while(espSerial.available() > 0) {
            a = espSerial.read();
            if (debug) Serial.print(a);
            if(a == '\0') continue;
            data += a;
        }
        if (data.indexOf(target1) != -1) {
            rValue=true;
            break;
        } 
    }
    return rValue;
}

//============================================================== vDelay
void vDelay(int delayInMillis){long t=millis()+delayInMillis;while (millis()<t) virtuinoRun();}

// ================ CONTROL SYSTEM ========================================
// Reads the input from the temperature sensor and converts it to °C
float tempRead(){
  temp_volt = analogRead(TEMP_SENSOR);
  R1_TEMP = R2_TEMP*((1024.0/temp_volt) - 1);
  float temp_read = (1/(A_TEMP+B_TEMP*log(R1_TEMP)+C_TEMP*pow(log(R1_TEMP),3))) - 270;
  return temp_read;
}

// Keeps track of light readings for checking whether its daytime or not
void check_daylight(float light_read){
  if(light_read <= night_thr){
    low_light_counter += 1;
    if(low_light_counter >= 1000){
      its_day = false;
      low_light_counter = 0;
    }
  }
  else if(light_read >= day_thr){
    high_light_counter += 1;
    if(high_light_counter >= 1000){
      its_day=true;
      high_light_counter = 0;
    }
  }
}
// Controls the lamp according to light sensor thresholds and the light reading 
void lightControl(float light_read){
  if(!LAMP_V1_lastValue && its_day){
    if(light_read < light_low_thr){
      digitalWrite(LAMP,LOW);
      V[1] = 1;
      Serial.println("### SISTEMA DE ILUMINACION ACTIVADO ##");
    }
    else if(light_read >= light_high_thr){
      digitalWrite(LAMP,HIGH);
      V[1] = 0;
    }
  }
}


// Controls the ventilator according to temperature thresholds and the temperature reading
void tempControl(float temp_read){
  if(!VENT_V2_lastValue){
    if(temp_read >= temp_high_thr){
      digitalWrite(VENT,HIGH);
      V[2] = 1;
      Serial.println("### SISTEMA DE VENTILACION ACTIVADO ##");
    }
    else if(temp_read < temp_low_thr){
      digitalWrite(VENT,LOW);
      V[2] = 0;
    }
  }
}

// Controls the waterpump according to soil humidity thresholds and the soil humidity reading
void waterControl(float moist_read){
  if(moist_read > moist_thr){
    digitalWrite(WATER_PUMP,LOW);
    V[3] = 1;
    recent_watering = 0;
    Serial.println("### SISTEMA DE RIEGO ACTIVADO ##");
    vDelay(watering_time);
    digitalWrite(WATER_PUMP,HIGH);
    water_level = water_level - watering_vol;
    writeIntIntoEEPROM(water_level_addr,water_level);     
    V[3] = 0;
  }
}

// Calculate temperature buffer mean
float getTempBufferMean(){
  float avg = 0.0;
  using index_t = decltype(tempBuffer)::index_t;
    for (index_t i = 0; i < tempBuffer.size(); i++) {
      avg += tempBuffer[i] / (float)tempBuffer.size();
    }
  return avg;
}

// Calculate ilumination buffer mean
float getLightBufferMean(){
  float avg = 0.0;
  using index_t = decltype(lightBuffer)::index_t;
    for (index_t i = 0; i < lightBuffer.size(); i++) {
      avg += lightBuffer[i] / (float)lightBuffer.size();
    }
  return avg;
}

// Calculate soil moisture buffer mean
float getMoistBufferMean(){
  float avg = 0.0;
  using index_t = decltype(moistBuffer)::index_t;
    for (index_t i = 0; i < moistBuffer.size(); i++) {
      avg += moistBuffer[i] / (float)moistBuffer.size();
    }
  return avg;
}

// ================ WRITING VALUES TO EEPROM =========================
void writeIntIntoEEPROM(int address, int number)
{ 
  byte byte1 = number >> 8;
  byte byte2 = number & 0xFF;
  EEPROM.write(address, byte1);
  EEPROM.write(address + 1, byte2);
}

int readIntFromEEPROM(int address)
{
  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}
