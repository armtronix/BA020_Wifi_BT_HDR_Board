
String FIRMWARE_VERSION="Firmware- V0.2 ";
String HARDWARE_VERSION="Hardware- V0.2 ";
String BOARD_NAME="BA020: Wifi BT Heavy Duty Board -Power Monitoring";
String MQTT_COMMANDS_AVAILABLE="Commands via Mqtt: R01_ON, R01_OFF, Reset, SetPV:P,V ";
String MQTT_TOPICS_AVAILABLE="Subscription sub-topics: /R01, /RO2, /HLW8021";
//#define DEBUG
//#define ALEXAEN
//#define WEBOTA
//debug added for information, change this according your needs

#define ARDUINO_ARCH_ESP32
//#define ESP8266

#ifdef DEBUG
  #define Debug(x)    Serial.print(x)
  #define Debugln(x)  Serial.println(x)
  #define Debugf(...) Serial.printf(__VA_ARGS__)
  #define Debugflush  Serial.flush
#else
  #define Debug(x)    {}
  #define Debugln(x)  {}
  #define Debugf(...) {}
  #define Debugflush  {}
#endif

#ifdef ALEXAEN
#include <Espalexa.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
 #include <WiFi.h>
 #include <WebServer.h>
 #include <ESPmDNS.h>
 #include "SPIFFS.h"
 #include <Update.h>
 #include "WiFiUdp.h"
 #include "HLW8012.h"
 #include <WebSocketsServer.h>
#else
 #include <ESP8266WiFi.h>
 #include <ESP8266WebServer.h>
 #include <ESP8266mDNS.h>
 #include "HLW8012.h"
extern "C" {
  #include "user_interface.h" //Needed for the reset command
}
#endif


#include <WiFiClient.h>
//#include <EEPROM.h>
//#include <Ticker.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"

#ifdef ALEXAEN
void firstLightChanged(uint8_t brightness);
#endif

//***** Settings declare ********************************************************************************************************* 
String hostName ="Armtronix"; //The MQTT ID -> MAC adress will be added to make it kind of unique
int iotMode=0; //IOT mode: 0 = Web control, 1 = MQTT (No const since it can change during runtime)
//select GPIO's
#ifdef ARDUINO_ARCH_ESP32
 #define OUTPIN_12        12 //output pin for relay 2
 #define OUTLED            2  // output pin for led
 #define INPIN             0  // input pin (push button)
 #define INPIN_12         32 //Virtual Switch for relay 
 int ZCD =                14;
 /*HLW8012.h     GPIOS                      */
 #define SEL_PIN          25
 #define CF1_PIN          13
 #define CF_PIN            4
#else
 #define OUTPIN_12        12 //output pin
 #define OUTLED            5
#endif
#define UPDATE_TIME      2000 //120 seconds //added on 14/03/19 by naren
#define RESTARTDELAY     3 //minimal time in sec for button press to reset
#define HUMANPRESSDELAY  50 // the delay in ms untill the press should be handled as a normal push by human. Button debounce. !!! Needs to be less than RESTARTDELAY & RESETDELAY!!!
#define RESETDELAY       20 //Minimal time in sec for button press to reset all settings and boot to config mode

// Check values every 2 seconds
#define STATUS_UPDATE_TIME              200
// Set SEL_PIN to HIGH to sample current
// This is the case for Itead's Sonoff POW, where a
// the SEL_PIN drives a transistor that pulls down
// the SEL pin in the HLW8012 when closed
#define CURRENT_MODE                    HIGH

// These are the nominal values for the resistors in the circuit
#define CURRENT_RESISTOR                0.001
#define VOLTAGE_RESISTOR_UPSTREAM       ( 5 * 470000 ) // Real: 2280k
#define VOLTAGE_RESISTOR_DOWNSTREAM     ( 1000 ) // Real 1.009k

//##### Object instances ##### 
MDNSResponder mdns;

#ifdef ARDUINO_ARCH_ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif

#ifdef ALEXAEN
Espalexa espalexa; 
#endif

WiFiClient wifiClient;
PubSubClient mqttClient;
WebSocketsServer webSocket = WebSocketsServer(81);
//Ticker btn_timer;
//Ticker otaTickLoop;
HLW8012 hlw8012;

//##### Flags ##### They are needed because the loop needs to continue and cant wait for long tasks!
int rstNeed=0;   // Restart needed to apply new settings
int toPub_1=0; // determine if state should be published.
int toPub_2=0; // determine if state should be published.
int configToClear=0; // determine if config should be cleared.
int otaFlag=0;
boolean inApMode=0;

//##### Global vars ##### 
int webtypeGlob;
int otaCount=300; //imeout in sec for OTA mode
int current; //Current state of the button
int switch_status_1; //Physical state of the switch 1
int switch_status_2; //Physical state of the switch 2
int state_out_1;   ////webpage mqtt state of the switch 1
int state_out_2;   ////webpage mqtt state of the switch 
unsigned long count = 0; //Button press time counter
String st; //WiFi Stations HTML list

String mqtt_user ="";
String mqtt_passwd ="";
String mqtt_will_msg ="Disconnected";

char buf[40]; //For MQTT data recieve
char* host; //The DNS hostname
String javaScript,XML;
//To be read from Config file
String esid="";
String epass = "";

String pubTopic;
String pubTopic_HLW8021;
String subTopic;
String mqttServer = "";
String o1_state_pub; //State of relay one for mqtt
String o2_state_pub; //State of relay two for mqtt
const char* otaServerIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

/*Alexa event names */
boolean wifiConnected = false;
String firstName;
String hostsaved;

float P,V;
String Values_HLW8021;
String Values_HLW8021_ActivePower;
String Values_HLW8021_Voltage;
String Values_HLW8021_Current;
String Values_HLW8021_ApparentPower;
String Values_HLW8021_Powerfactor;
String Values_HLW8021_Energy;
double setmulPower=0;
double setmulCurrent=0;
double setmulVoltage=0;
int calflag =0;

#ifdef ARDUINO_ARCH_ESP32
#include "esp_system.h"
#else 
#endif


volatile int interruptCounter = 0;
int numberOfInterrupts = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/*###############################################################################################*/
// When using interrupts we have to call the library entry point
// whenever an interrupt is triggered
void IRAM_ATTR hlw8012_cf1_interrupt() 
{   
    portENTER_CRITICAL_ISR(&mux);
    interruptCounter++;
    hlw8012.cf1_interrupt();
    portEXIT_CRITICAL_ISR(&mux);
    
}
void IRAM_ATTR hlw8012_cf_interrupt()
{
    portENTER_CRITICAL_ISR(&mux);
    hlw8012.cf_interrupt();
    portEXIT_CRITICAL_ISR(&mux);
}
/*###############################################################################################*/
// Library expects an interrupt on both edges
void setInterrupts() {
    attachInterrupt(digitalPinToInterrupt(CF1_PIN), hlw8012_cf1_interrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(CF_PIN), hlw8012_cf_interrupt, CHANGE);
}

/*###############################################################################################*/
void calibrate_sensor(float power_value,float voltage_value) 
{
    // Let's first read power, current and voltage
    // with an interval in between to allow the signal to stabilise:
    digitalWrite(OUTPIN_12,HIGH);
    
    Serial.println(power_value);
    Serial.println(voltage_value);
    
     delay(100);
     unsigned long timeout = millis();
    while ((millis() - timeout) < 10000) {
        delay(1);
    }
    // Calibrate using a 60W bulb (pure resistive) on a 230V line
    setmulPower=hlw8012.expectedActivePower(power_value);
    setmulVoltage=hlw8012.expectedVoltage(voltage_value);
    setmulCurrent=hlw8012.expectedCurrent(power_value / voltage_value);
    
    hlw8012.setCurrentMultiplier(setmulCurrent);
    hlw8012.setVoltageMultiplier(setmulVoltage);
    hlw8012.setPowerMultiplier(setmulPower);
    
    // Show corrected factors
    Serial.print("[HLW] New current multiplier : "); Serial.println(hlw8012.getCurrentMultiplier());
    Serial.print("[HLW] New voltage multiplier : "); Serial.println(hlw8012.getVoltageMultiplier());
    Serial.print("[HLW] New power multiplier   : "); Serial.println(hlw8012.getPowerMultiplier());
    Serial.println();
    digitalWrite(OUTPIN_12,LOW);
}

//-------------- void's -------------------------------------------------------------------------------------
void setup() 
{
  Serial.begin(115200);
  delay(100); 
  Serial.println(BOARD_NAME);
  Serial.println(FIRMWARE_VERSION);
  Serial.println(HARDWARE_VERSION);
  Serial.println(MQTT_COMMANDS_AVAILABLE);
  Serial.println(MQTT_TOPICS_AVAILABLE);
  delay(100); 
  pinMode(INPIN_12,INPUT_PULLUP);  
  pinMode(CF1_PIN, INPUT);
  pinMode(CF_PIN, INPUT);
  pinMode(SEL_PIN, OUTPUT);
  pinMode(OUTPIN_12, OUTPUT);
  pinMode(OUTLED, OUTPUT);
  digitalWrite(OUTPIN_12, LOW);
  digitalWrite(OUTLED, LOW);
  
//  btn_timer.attach(0.05, btn_handle);
  Debugln("DEBUG: Entering loadConfig()");
  if (!SPIFFS.begin()) 
  {
    Serial.println("Failed to mount file system");
  }
  
  uint8_t mac[6];
  #ifdef ARDUINO_ARCH_ESP32
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  #else
  WiFi.macAddress(mac);
  #endif
  hostName += "-";
  hostName += macToStr(mac);
  String hostTemp=hostName;
  hostTemp.replace(":","-");
  host = (char*) hostTemp.c_str();
  hostsaved=hostTemp;
  loadConfig();
  //loadConfigOld();
  Debugln("DEBUG: loadConfig() passed");
  
  // Connect to WiFi network
  Debugln("DEBUG: Entering initWiFi()");
  initWiFi();
  Debugln("DEBUG: initWiFi() passed");
  Debug("iotMode:");
  Debugln(iotMode);
  Debug("webtypeGlob:");
  Debugln(webtypeGlob);
  Debug("otaFlag:");
  Debugln(otaFlag);
  Debugln("DEBUG: Starting the main loop");
#ifdef ALEXAEN
  //Alexa Part
   if(wifiConnected)
   {
    server.on("/", HTTP_GET, [](){
    server.send(200, "text/plain", "This is an example index page your server may send.");
    });
    server.on("/test", HTTP_GET, [](){
    server.send(200, "text/plain", "This is a second subpage you may have.");
    });
    server.onNotFound([](){
      if (!espalexa.handleAlexaApiCall(server.uri(),server.arg(0))) //if you don't know the URI, ask espalexa whether it is an Alexa control request
      {
        //whatever you want to do with 404s
        server.send(404, "text/plain", "Not found");
      }
    });
  }
  // Define your devices here.
 espalexa.addDevice((char*)firstName.c_str(), firstLightChanged); //simplest definition, default state off
 espalexa.begin(&server); //give espalexa a pointer to your server object so it can use your server instead of creating its own
#else
#endif 
  
  hlw8012.begin(CF_PIN, CF1_PIN, SEL_PIN, CURRENT_MODE, true);
  
  hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM); 
  hlw8012.setCurrentMultiplier(setmulCurrent);
  hlw8012.setVoltageMultiplier(setmulVoltage);
  hlw8012.setPowerMultiplier(setmulPower);
    
  Serial.print("[HLW] Default current multiplier : "); Serial.println(hlw8012.getCurrentMultiplier());
  Serial.print("[HLW] Default voltage multiplier : "); Serial.println(hlw8012.getVoltageMultiplier());
  Serial.print("[HLW] Default power multiplier   : "); Serial.println(hlw8012.getPowerMultiplier());
  Serial.println();
  setInterrupts();
}


/*###############################################################################################*/
void switch_status_handle()
{
  if(!digitalRead(INPIN_12))
 {
 switch_status_1=1;
 switch_status_2=1;
 }
 else
 {
 switch_status_1=0; 
 switch_status_2=0;
 }
}

/*###############################################################################################*/
void output_status_handle()
{
int A=state_out_1;
int B=switch_status_1;
int C=state_out_2;
int D=switch_status_2;

if((((!A)&&(B))||((A)&&(!B))))
 {
  digitalWrite(OUTPIN_12,HIGH);
  if(o1_state_pub !="R01ISON")
  {
   o1_state_pub="R01ISON";
   toPub_1 = 1;
  }
 }
else
 {
  digitalWrite(OUTPIN_12,LOW);
  if(o1_state_pub !="R01ISOFF")
  {
   o1_state_pub="R01ISOFF";
   toPub_1 = 1;
  }
 }

if(((!C)&&(D))||((C)&&(!D)))
 {
  digitalWrite(OUTLED,HIGH);
  if(o2_state_pub !="R02ISON")
  {
   o2_state_pub="R02ISON";
   toPub_2 = 1;
  }
 }
else
 {
  digitalWrite(OUTLED,LOW);
 if(o2_state_pub !="R02ISOFF")
  {
   o2_state_pub="R02ISOFF";
   toPub_2 = 1;
  }
 }
}

/*###############################################################################################*/

//void btn_handle()
//{
//  if(!digitalRead(INPIN)){
//    ++count; // one count is 50ms
//  } else {
//    if (count > 1 && count < HUMANPRESSDELAY/5) { //push between 50 ms and 1 sec      
//      Serial.print("button pressed "); 
//      Serial.print(count*0.05); 
//      Serial.println(" Sec."); 
//    
//      Serial.print("Light is ");
//      Serial.println(digitalRead(OUTPIN_04));
//      
//      Serial.print("Switching light to "); 
//      Serial.println(!digitalRead(OUTPIN_04));
//      digitalWrite(OUTPIN_04, !digitalRead(OUTPIN_04)); 
//      state = digitalRead(OUTPIN_04);
//      if(iotMode==1 && mqttClient.connected()){
//        toPub=1;        
//        Debugln("DEBUG: toPub set to 1");
//      }
//    } else if (count > (RESTARTDELAY/0.05) && count <= (RESETDELAY/0.05)){ //pressed 3 secs (60*0.05s)
//      Serial.print("button pressed "); 
//      Serial.print(count*0.05); 
//      Serial.println(" Sec. Restarting!"); 
//      setOtaFlag(!otaFlag);      
//     #ifdef ARDUINO_ARCH_ESP32
//           esp_restart();
//           #else
//           ESP.reset();
//           #endif 
//    } else if (count > (RESETDELAY/0.05)){ //pressed 20 secs
//      Serial.print("button pressed "); 
//      Serial.print(count*0.05); 
//      Serial.println(" Sec."); 
//      Serial.println(" Clear settings and resetting!");       
//      configToClear=1;
//      }
//    count=0; //reset since we are at high
//  }
//}



//-------------------------------- Main loop ---------------------------
void loop() 
{
  if(interruptCounter>0)
  { 
    portENTER_CRITICAL(&mux);
    interruptCounter--;
    portEXIT_CRITICAL(&mux);
  }
 static unsigned long last = millis();//added on 14/03/19 by naren

    
 // This UPDATE_TIME should be at least twice the interrupt timeout (2 second by default)
    if ((millis() - last) > UPDATE_TIME) 
    {

        last = millis();
        
        Values_HLW8021_ActivePower = String(hlw8012.getActivePower());
        Values_HLW8021_Voltage = String(hlw8012.getVoltage());
        Values_HLW8021_Current = String(hlw8012.getCurrent());
        Values_HLW8021_ApparentPower = String(hlw8012.getApparentPower());
        Values_HLW8021_Powerfactor = String((int) (100 * hlw8012.getPowerFactor()));
        Values_HLW8021_Energy = String(hlw8012.getEnergy());
        
        Serial.print("[HLW] Active Power (W)    : "); Serial.println(Values_HLW8021_ActivePower);
        Serial.print("[HLW] Voltage (V)         : "); Serial.println(Values_HLW8021_Voltage);
        Serial.print("[HLW] Current (A)         : "); Serial.println(Values_HLW8021_Current);
        Serial.print("[HLW] Apparent Power (VA) : "); Serial.println(Values_HLW8021_ApparentPower);
        Serial.print("[HLW] Power Factor (%)    : "); Serial.println(Values_HLW8021_Powerfactor);
        Serial.print("[HLW] Agg. energy (Ws)    : "); Serial.println(Values_HLW8021_Energy);
        Serial.println();
        
        Values_HLW8021= "P:"+Values_HLW8021_ActivePower+"W"+", V:"+Values_HLW8021_Voltage+"V"+", C:"+Values_HLW8021_Current+"A"+", E:"+Values_HLW8021_Energy+"Ws";
        pubTopic_HLW8021=pubTopic+"/HLW8021";
         if (mqttClient.connected()) 
         {
         mqttClient.publish((char*)pubTopic_HLW8021.c_str(), (char*)Values_HLW8021.c_str());
         mqttClient.loop();
         }
    }

   if(calflag ==1)
   { 
    calibrate_sensor(P,V);
    calflag =0; 
    saveConfig()? Serial.println("sucessfully.") : Serial.println("not succesfully!");;
    delay(100);
    esp_restart();
   }


  switch_status_handle();
  output_status_handle(); 
  webSocket.loop();     
     


  
 //Debugln("DEBUG: loop() begin");
  if(configToClear==1)
  {
    //Debugln("DEBUG: loop() clear config flag set!");
    clearConfig()? Serial.println("Config cleared!") : Serial.println("Config could not be cleared");
    delay(1000);
   #ifdef ARDUINO_ARCH_ESP32
           esp_restart();
           #else
           ESP.reset();
           #endif 
  }
  //Debugln("DEBUG: config reset check passed");  
  if (WiFi.status() == WL_CONNECTED && otaFlag)
  {
    if(otaCount<=1) 
    {
     Serial.println("OTA mode time out. Reset!"); 
     setOtaFlag(0);
     #ifdef ARDUINO_ARCH_ESP32
     esp_restart();
     #else
     ESP.reset();
     #endif 
     delay(10);
    }
    server.handleClient();
    delay(1);   
  } 
  else if (WiFi.status() == WL_CONNECTED || webtypeGlob == 1)
  {
     //Debugln("DEBUG: loop - webtype =1"); 
    //Debugln("DEBUG: loop() wifi connected & webServer ");
    if (iotMode==0 || webtypeGlob == 1)
    {
      //Debugln("DEBUG: loop() Web mode requesthandling ");
      server.handleClient();
      delay(1);
      if(esid != "" && WiFi.status() != WL_CONNECTED) //wifi reconnect part
      {
       // Scan_Wifi_Networks_reconnect();
        if ((millis() - last) > UPDATE_TIME) //added on 14/03/19 by naren
           {
            Scan_Wifi_Networks();
            last = millis(); //added on 14/03/19 by naren
           }
        
      }
    } 
    else if (iotMode==1 && webtypeGlob != 1 && otaFlag !=1)
    {
          //Debugln("DEBUG: loop() MQTT mode requesthandling ");
          if (!connectMQTT()){
              delay(20);          
          }                    
          if (mqttClient.connected()){            
              //Debugln("mqtt handler");
              mqtt_handler();
          } else {
              Debugln("mqtt Not connected!");
          }
    }
  } 
  else
  {
    Debugln("DEBUG: loop - WiFi not connected function initWifi()");  
    delay(10);//eariler it was 1000 changedon 14/03/19
    initWiFi(); //Try to connect again
  }
#ifdef ALEXAEN
  espalexa.loop();
#endif

   delay(1);
 //Debugln("DEBUG: loop() end");


}

//our callback functions

//void firstLightChanged(uint8_t brightness) 


/*##################################Callback functions for Relay #########################*/
#ifdef ALEXAEN
void firstLightChanged(uint8_t brightness) 
{
 Serial.println("Device 2 changed to ");
    if (brightness == 255) 
     {
      Serial.println("High");
      if(switch_status_12==1)
       { 
        wifi_input_status_12=0;
       }
      else
       { 
        wifi_input_status_12=1;
       }
      send_status_12=1;
     }
    else if(brightness == 0) 
     {
      Serial.println("LOW");
      if(switch_status_12==0)
       { 
        wifi_input_status_12=0;
       }
      else
       { 
        wifi_input_status_12=1;
       }
     send_status_12=1;
    }

}
#endif


