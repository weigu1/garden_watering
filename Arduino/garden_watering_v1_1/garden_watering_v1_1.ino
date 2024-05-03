/*
  garden_watering.ino

  V1.1 2022-05-23
  changes from v1.0: added config file, mqtt secure bug, added scheduled
                     erigation.
                      
  for infos over time:
  https://www.weigu.lu/microcontroller/tips_tricks/esp_NTP_tips_tricks/index.html                   

  ---------------------------------------------------------------------------
  Copyright (C) 2022 Guy WEILER www.weigu.lu
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
  ---------------------------------------------------------------------------
 
  ESP8266: LOLIN/WEMOS D1 mini pro
  ESP32:   MH ET LIVE ESP32-MINI-KIT 
  
  MHET    | MHET    - LOLIN        |---| LOLIN      - MHET    | MHET
  
  GND     | RST     - RST          |---| TxD        - RxD(3)  | GND
   NC     | SVP(36) -  A0          |---| RxD        - TxD(1)  | 27 
  SVN(39) | 26      -  D0(16)      |---|  D1(5,SCL) -  22     | 25
   35     | 18      -  D5(14,SCK)  |---|  D2(4,SDA) -  21     | 32
   33     | 19      -  D6(12,MISO) |---|  D3(0)     -  17     | TDI(12)
   34     | 23      -  D7(13,MOSI) |---|  D4(2,LED) -  16     | 4
  TMS(14) | 5       -  D8(15,SS)   |---| GND        - GND     | 0   
   NC     | 3V3     - 3V3          |---|  5V        -  5V     | 2
  SD2(9)  | TCK(13)                |---|              TD0(15) | SD1(8)
  CMD(11) | SD3(10)                |---|              SD0(7)  | CLK(6)
*/

/*!!!!!!!!!! make your changes in config.h (or secrets.h) !!!!!!!!!*/

/*!!!!!!!!!! to debug you can use the onboard LED, a second serial port on D4
             or best: UDP! Look in setup()                !!!!!!!!!*/
             
/*?????? Comment or uncomment the following lines suiting your needs ??????*/

/* The file "secrets.h" (here secrets_garden.h) has to be placed in the sketchbook libraries folder
   in a folder named "Secrets" and must contain the same things than the file config.h*/
#define USE_SECRETS
#define OTA               // if Over The Air update needed (security risk!)
//#define MQTTPASSWORD    // if you want an MQTT connection with password (recommended!!)
#define STATIC            // if static IP needed (no DHCP)
#define BME280_I2C       

/****** Arduino libraries needed ******/
#ifdef USE_SECRETS
  #include <secrets_garden_watering.h>
#else  
  #include "config.h"      // most of the things you need to change are here
#endif // USE_SECRETS  
#include "ESPToolbox.h"  // ESP helper lib (more on weigu.lu)
#include <PubSubClient.h>
#include <ArduinoJson.h>
#ifdef BME280_I2C
  #include <Wire.h>
  #include <BME280I2C.h>
#endif

/****** WiFi and network settings ******/
const char *WIFI_SSID = MY_WIFI_SSID;           // if no secrets file, use the config.h file
const char *WIFI_PASSWORD = MY_WIFI_PASSWORD;   // if no secrets file, use the config.h file
#ifdef OTA                                // Over The Air update settings
  const char *OTA_NAME = MY_OTA_NAME;
  const char *OTA_PASS_HASH = MY_OTA_PASS_HASH;  // use the config.h file
#endif // ifdef OTA      
#ifdef STATIC
  IPAddress NET_LOCAL_IP (NET_LOCAL_IP_BYTES);    // 3x optional for static IP
  IPAddress NET_GATEWAY (NET_GATEWAY_BYTES);      // look in config.h
  IPAddress NET_MASK (NET_MASK_BYTES);  
#endif // ifdef STATIC
IPAddress UDP_LOG_PC_IP(UDP_LOG_PC_IP_BYTES);     // UDP logging if enabled in setup() (config.h)

/****** MQTT settings ******/
const int MQTT_MAXIMUM_PACKET_SIZE = 2048; // in setup()
const short MQTT_PORT = MY_MQTT_PORT;
WiFiClient espClient;
#ifdef MQTTPASSWORD
  const char *MQTT_USER = MY_MQTT_USER;
  const char *MQTT_PASS = MY_MQTT_PASS;
#endif // MQTTPASSWORD

PubSubClient MQTT_Client(espClient);

/******* BME280 ******/
float temp(NAN), hum(NAN), pres(NAN);
#ifdef BME280_I2C
  BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                    // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
#endif

ESPToolbox Tb;

/********** SETUP ************************************************************/
void setup() {
  Tb.set_led_log(true);                 // use builtin LED for debugging
  //Tb.set_serial_log(true,1);        // 2 parameter = interface (1 = Serial1)  
  Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 7777"    
  Tb.init_ntp_time();  
  init_wifi();       
  init_relays(PIN_RELAYS,sizeof(PIN_RELAYS));
  #ifdef BME280_I2C
    init_bme280();     
  #endif
  delay(2000);                                  // give it some time   
  MQTT_Client.setBufferSize(MQTT_MAXIMUM_PACKET_SIZE);
  MQTT_Client.setServer(MQTT_SERVER,MQTT_PORT); //open connection MQTT server
  MQTT_Client.setCallback(MQTT_callback);
  //mqtt_connect();
  #ifdef OTA
    Tb.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  Tb.blink_led_x_times(3);
  Tb.log_ln("Setup done!");
}

/********** LOOP  ************************************************************/

void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  Tb.get_time();
  handle_relays();
  handle_auto_watering();
  // Publish every PUBLISH_TIME  
  if (Tb.non_blocking_delay(PUBLISH_TIME)) {    
    MQTT_get_temp_and_publish();        
  }    
  if (WiFi.status() != WL_CONNECTED) {   // if WiFi disconnected, reconnect
    init_wifi();
  }    
  if (!MQTT_Client.connected()) {        // reconnect mqtt client, if needed  
    mqtt_connect();
  }
  MQTT_Client.loop();                    // make the MQTT live  
  delay(10); // needed for the watchdog!
}

/********** WiFi functions ***************************************************/

// init WiFi (overloaded function if STATIC)
void init_wifi() {
  #ifdef STATIC
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_HOSTNAME, NET_LOCAL_IP,
                  NET_GATEWAY, NET_MASK);
  #else                  
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);                       
  #endif // ifdef STATIC
}

/********** MQTT functions ***************************************************/

// connect to MQTT server
void mqtt_connect() {
  while (!MQTT_Client.connected()) { // Loop until we're reconnected
    Tb.log("Attempting MQTT connection...");    
    #ifdef MQTTPASSWORD
      if (MQTT_Client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    #else
      if (MQTT_Client.connect(MQTT_CLIENT_ID)) { // Attempt to connect
    #endif // ifdef UNMQTTSECURE
      Tb.log_ln("MQTT connected");      
      MQTT_Client.subscribe(MQTT_TOPIC_IN.c_str());
      // Once connected, publish an announcement...    
      // MQTT_Client.publish(MQTT_TOPIC, "{\"dt\":\"connected\"}");
      // don't because OpenHAB does not like this ...      
    }
    else {
      Tb.log("MQTT connection failed, rc=");
      Tb.log(String(MQTT_Client.state()));
      Tb.log_ln(" try again in 5 seconds");
      delay(5000);  // Wait 5 seconds before retrying
    }
  }
}

// MQTT get the time, relay flags ant temperature an publish the data
void MQTT_get_temp_and_publish() {
  DynamicJsonDocument doc_out(1024);
  String mqtt_msg, we_msg;  
  doc_out["datetime"] = Tb.t.datetime;
  #ifdef BME280_I2C
    get_data_bme280();
    doc_out["temperature_C"] = (int)(temp*10.0 + 0.5)/10.0;
    doc_out["humidity_%"] = (int)(hum*10.0 + 0.5)/10.0;
    doc_out["pressure_hPa"] = (int)((pres + 5)/10)/10.0;
  #endif    
  doc_out["Relay_0"] = (byte)!relay_stop_flags[0];
  doc_out["Relay_1"] = (byte)!relay_stop_flags[1];
  doc_out["Relay_2"] = (byte)!relay_stop_flags[2];
  doc_out["Relay_3"] = (byte)!relay_stop_flags[3];
  doc_out["Relay_4"] = (byte)!relay_stop_flags[4];
  doc_out["Auto_flag"] = (byte)auto_flag;    
  for (byte i=0; i<(sizeof(watering_events)/sizeof(watering_events[0])); i++) {
    we_msg = String(watering_events[i].relay_nr) + ' ' +
             String(watering_events[i].start_time) + ' ' +
             String(watering_events[i].duration);
    doc_out["Watering_event_" + String(i) + "_relay_start_duration"] = we_msg;
  }
  mqtt_msg = "";
  serializeJson(doc_out, mqtt_msg);
  MQTT_Client.publish(MQTT_TOPIC_OUT.c_str(),mqtt_msg.c_str());    
  Tb.log("MQTT published at ");
  Tb.log_ln(Tb.t.time);
}  

// MQTT callback
// Commands in JSON: {"Relay_(0-4)":1,"Time_min":20}
//                   {"Auto_(0-1)":1}
//                   {"Event_(nr_relay_start_duration)":"2 3 1900 15"}

void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  DynamicJsonDocument doc_in(256);
  byte nr;
  unsigned int i, tmp;
  unsigned long time_ms;
  String message;
  Tb.log_ln("Message arrived [" + String(topic) + "] ");
  for (int i=0;i<length;i++) {    
    message += String((char)payload[i]);
  }
  Tb.log_ln(message);
  deserializeJson(doc_in, message);
  if (doc_in.containsKey("Auto_(0-1)")) {
    auto_flag = bool(doc_in["Auto_(0-1)"]); 
    Tb.log_ln("Auto_flag arrived [" + String(auto_flag) + "] ");
  }
  else if (doc_in.containsKey("Event_(nr_relay_start_duration)")) {
      String event_str = doc_in["Event_(nr_relay_start_duration)"]; 
      Tb.log_ln(event_str);
      i = event_str.substring(0,1).toInt();      
      tmp = event_str.substring(2,3).toInt();
      watering_events[i].relay_nr = tmp;
      tmp = event_str.substring(4,8).toInt();
      watering_events[i].start_time = tmp;
      tmp = event_str.substring(9).toInt();
      watering_events[i].duration = tmp;  
  }
  else {
    nr = doc_in["Relay_(0-4)"];  
    time_ms = doc_in["Time_min"];
    time_ms = time_ms * 60UL *1000UL;  
    Tb.log_ln("Nr: " + String(nr) + " time: " + String(time_ms));
    if ((nr >= 0) && (nr < NR_OF_RELAYS)) {    
      if (time_ms != 0) {
        relay_times_ms[nr] = time_ms;
        relay_start_flags[nr] = true;
      }
      else {
        relay_times_ms[nr] = 0;
      }    
    }
  }  
}

/********** BME280 functions *************************************************/

// initialise the BME280 sensor
#ifdef BME280_I2C
  void init_bme280() {
    Wire.begin();
    while(!bme.begin()) {
      Tb.log_ln("Could not find BME280 sensor!");
      delay(1000);
    }
    switch(bme.chipModel())  {
       case BME280::ChipModel_BME280:
         Tb.log_ln("Found BME280 sensor! Success.");
         break;
       case BME280::ChipModel_BMP280:
         Tb.log_ln("Found BMP280 sensor! No Humidity available.");
         break;
       default:
         Tb.log_ln("Found UNKNOWN sensor! Error!");
    }
  }

// get BME280 data and log it
void get_data_bme280() {
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pres, temp, hum, tempUnit, presUnit);
  Tb.log_ln("Temp: " + (String(temp)) + " Hum: " + (String(hum)) + 
           " Pres: " + (String(pres)));
}
#endif  // BME280_I2C

/********** Relay functions **************************************************/

// init relay pins as output and close all relays
void init_relays(const byte PIN_RELAYS[], byte number_of_relays) {
  for (byte i=0; i<number_of_relays; i++) {
    pinMode(PIN_RELAYS[i],OUTPUT);
  }
  for (byte i=0; i<number_of_relays; i++) {  // all off (neg. logic!)
    digitalWrite(PIN_RELAYS[i],HIGH);
  }
}

// handle auto watering (array in config.h)
void handle_auto_watering() {
  byte nr;
  unsigned long time_ms;
  static unsigned int one_minute_flag = 0;
  if (auto_flag) {
    if (one_minute_flag == (Tb.t.hour*100+Tb.t.minute)) { // wait one minute    
      Tb.log(".");      
    }
    else {          
      for (byte i=0; i<(sizeof(watering_events)/sizeof(watering_events[0])); i++) {        
        if ((Tb.t.hour*100+Tb.t.minute)==watering_events[i].start_time) {          
          one_minute_flag = watering_events[i].start_time;
          Tb.log_ln("auto_watering_started at " + String(Tb.t.time));
          nr = watering_events[i].relay_nr;
          time_ms = watering_events[i].duration;
          time_ms = time_ms * 60UL *1000UL;  
          if ((nr >= 0) && (nr < NR_OF_RELAYS)) {    
            if (time_ms != 0) {
              relay_times_ms[nr] = time_ms;
              relay_start_flags[nr] = true;
            }
            else {
              relay_times_ms[nr] = 0;
            }              
          }
        }
      }      
    }
  }  
}

// start watering if start flag = true and stop it if time is over

void handle_relays() {
  for (byte i=0; i<5; i++) {
    if (relay_start_flags[i] == true) {
      relay_start_flags[i] = false;
      relay_stop_flags[i] = false;
      digitalWrite(PIN_RELAYS[i],LOW);
      relay_prev_millis[i] = millis(); 
      Tb.log_ln("Relay Nr: " + String(i) + " started (" + String(millis()) + ')');
    }  
    if (relay_stop_flags[i] == false) {      
      if ((millis() - relay_prev_millis[i]) > relay_times_ms[i]) {
        relay_stop_flags[i] = true;
        digitalWrite(PIN_RELAYS[i],HIGH);      
        Tb.log_ln("Relay Nr: " + String(i) + " stopped (" + String(millis()) + ')');
      }
    }
  }
}
