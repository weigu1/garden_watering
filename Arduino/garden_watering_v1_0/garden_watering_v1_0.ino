/*
  garden_watering.ino

  V1.0 2021-07-06

  ---------------------------------------------------------------------------
  Copyright (C) 2021 Guy WEILER www.weigu.lu
  
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

// Publishes every in milliseconds
const long PUBLISH_TIME = 60000;

// Comment or uncomment the following lines suiting your needs
//#define MQTTSECURE    // if you want a secure connection over MQTT (recommended!!)
#define STATIC        // if static IP needed (no DHCP)
#define OTA           // if Over The Air update needed (security risk!)
// The file "secrets.h" has to be placed in the sketchbook libraries folder
// in a folder named "Secrets" and must contain your secrets e.g.:
// const char *MY_WIFI_SSID = "mySSID"; const char *MY_WIFI_PASSWORD = "myPASS";
#define USE_SECRETS
#define BME280_I2C

#include "ESPToolbox.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#ifdef BME280_I2C
  #include <Wire.h>
  #include <BME280I2C.h>
#endif

/****** WiFi and network settings ******/
#ifdef USE_SECRETS
  #include <secrets.h>
  const char *WIFI_SSID = MY_WIFI_SSID;           // ssid     
  const char *WIFI_PASSWORD = MY_WIFI_PASSWORD;   // password
  #ifdef OTA                                      // Over The Air update settings
    const char *OTA_NAME = "garden_watering";
    const char *OTA_PASS_HASH = MY_OTA_PASS_HASH; // md5 hash for OTA
  #endif // ifdef OTA  
#else
  const char *WIFI_SSID = mySSID;           // if no secrets file, add your SSID here
  const char *WIFI_PASSWORD = myPASSWORD;   // if no secrets file, add your PASS here
  #ifdef OTA                                // Over The Air update settings
    const char *OTA_NAME = "garden_watering";
    const char *OTA_PASS_HASH = myOTAHASH;  // if no secrets file, add your OTA HASH here
  #endif // ifdef OTA      
#endif  // ifdef USE_SECRETS
const char *NET_MDNSNAME = "watering";      // optional (access with myESP.local)
const char *NET_HOSTNAME = "watering";      // optional
#ifdef STATIC
  IPAddress NET_LOCAL_IP (192,168,1,89);    // 3x optional for static IP
  IPAddress NET_GATEWAY (192,168,1,20);
  IPAddress NET_MASK (255,255,255,0);  
#endif // ifdef STATIC*/
const word UDP_LOG_PORT = 6667;             // UDP logging settings
IPAddress UDP_LOG_PC_IP(192,168,1,50);
const char *NTP_SERVER = "lu.pool.ntp.org"; // NTP settings
// your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

/****** MQTT settings ******/
#define MQTT_MAX_PACKET_SIZE 512
const char *MQTT_SERVER = "192.168.1.60";
const char *MQTT_CLIENT_ID = "garden_watering";
String MQTT_TOPIC_OUT = "weigu/garden/watering/data";
String MQTT_TOPIC_IN = "weigu/garden/watering/command";
#ifdef MQTTSECURE // http://weigu.lu/tutorials/sensors2bus/06_mqtt/index.html
  const short MQTT_PORT = 8883;                      // port for secure communication
  const char *MQTT_USER = "me";
  const char *MQTT_PASS = "myMqttPass12!";
  const char *MQTT_PSK_IDENTITY = "btsiot1";
  const char *MQTT_PSK_KEY = "0123456789abcdef0123"; // hex string without 0x
  WiFiClientMQTTSECURE espClient;
#else
  const short MQTT_PORT = 1883;                      // clear text = 1883
  WiFiClient espClient;
#endif
PubSubClient MQTT_Client(espClient);
String mqtt_msg;
DynamicJsonDocument doc_out(512);
DynamicJsonDocument doc_in(256);

/******* Relays *******/
const byte PIN_RELAYS[] = {16, 14, 12, 13, 0};
bool relay_start_flags[5] = {false,false,false,false,false};
bool relay_stop_flags[5] = {true,true,true,true,true};
unsigned long relay_times_ms[5] = {10000,20000,30000,40000,50000};
unsigned long relay_prev_millis[5] = {0,0,0,0,0};

/******* BME280 ******/
float temp(NAN), hum(NAN), pres(NAN);
#ifdef BME280_I2C
  BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                    // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,
#endif

ESPToolbox Tb;

/********** SETUP *************************************************************/
void setup() {
  Tb.set_led_log(true);                 // use builtin LED for debugging
  Tb.set_serial_log(true,1);            // 2 parameter = interface (1 = Serial1)
  Tb.set_udp_log(true, UDP_LOG_PC_IP, UDP_LOG_PORT); // use "nc -kulw 0 6667"  
  init_relays(PIN_RELAYS,sizeof(PIN_RELAYS));
  init_wifi(); 
  delay(1000);
  Tb.log_ln("Helu");
  #ifdef BME280_I2C
    init_bme280();     
  #endif  
  delay(2000);                                  // give it some time 
  MQTT_Client.setServer(MQTT_SERVER,MQTT_PORT); //open connection MQTT server
  MQTT_Client.setCallback(MQTT_callback);
  //mqtt_connect();
  #ifdef OTA
    Tb.init_ota(OTA_NAME, OTA_PASS_HASH);
  #endif // ifdef OTA
  Tb.blink_led_x_times(3);
}

/********** LOOP  **************************************************************/
void loop() {
  #ifdef OTA
    ArduinoOTA.handle();
  #endif // ifdef OTA
  handle_relays();
  // Publish every PUBLISH_TIME  
  if (Tb.non_blocking_delay(PUBLISH_TIME)) {
    #ifdef BME280_I2C
      get_data_bme280();
    #endif    
    doc_out["Temperature [C]"] = (int)(temp*10.0 + 0.5)/10.0;
    doc_out["Humidity [%]"] = (int)(hum*10.0 + 0.5)/10.0;
    doc_out["Pressure [hp]"] = (int)((pres + 5)/10)/10.0;
    doc_out["Relay_0 [0/1]"] = (byte)!relay_stop_flags[0];
    doc_out["Relay_1 [0/1]"] = (byte)!relay_stop_flags[1];
    doc_out["Relay_2 [0/1]"] = (byte)!relay_stop_flags[2];
    doc_out["Relay_3 [0/1]"] = (byte)!relay_stop_flags[3];
    doc_out["Relay_4 [0/1]"] = (byte)!relay_stop_flags[4];
    mqtt_msg = "";
    serializeJson(doc_out, mqtt_msg);
    MQTT_Client.publish(MQTT_TOPIC_OUT.c_str(),mqtt_msg.c_str());    
  }    
  if (WiFi.status() != WL_CONNECTED) {   // if WiFi disconnected, reconnect
    init_wifi();
  }    
  if (!MQTT_Client.connected()) {        // reconnect mqtt client, if needed  
    mqtt_connect();
  }
  MQTT_Client.loop();                    // make the MQTT live
  yield();  
}

/********** WiFi functions ****************************************************/
void init_wifi() {
  #ifdef STATIC
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_HOSTNAME, NET_LOCAL_IP,
                  NET_GATEWAY, NET_MASK);
  #else                  
    Tb.init_wifi_sta(WIFI_SSID, WIFI_PASSWORD, NET_MDNSNAME, NET_HOSTNAME);                       
  #endif // ifdef STATIC
}

/********** MQTT functions ****************************************************/
// connect to MQTT server
void mqtt_connect() {
  while (!MQTT_Client.connected()) { // Loop until we're reconnected
    Tb.log("Attempting MQTT connection...");    
    #ifdef MQTTSECURE  
      if (MQTT_Client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    #else
      if (MQTT_Client.connect(MQTT_CLIENT_ID)) { // Attempt to connect
    #endif // ifdef UNMQTTSECURE
      Tb.log_ln("MQTT connected");      
      MQTT_Client.subscribe(MQTT_TOPIC_IN.c_str());
      // Once connected, publish an announcement...    
      //MQTT_Client.publish(MQTT_TOPIC, "{\"dt\":\"connected\"}");
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

// Command struct: {"Relay [0-4]":1,"Time [min]":20};
void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  String message;
  Tb.log_ln("Message arrived [" + String(topic) + "] ");
  for (int i=0;i<length;i++) {    
    message += String((char)payload[i]);
  }
  Tb.log_ln(message);
  deserializeJson(doc_in, message);
  byte nr = doc_in["Relay [0-4]"];  
  unsigned long time_ms = doc_in["Time [min]"];
  time_ms = time_ms * 60UL *1000UL;  
  Tb.log_ln("Nr: " + String(nr) + " time: " + String(time_ms));
  if ((nr >= 0) && (nr <= 4)) {    
    if (time_ms != 0) {
      relay_times_ms[nr] = time_ms;
      relay_start_flags[nr] = true;
    }
    else {
      relay_times_ms[nr] = 0;
    }    
  }
}

/********** BME280 functions **************************************************/
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

void get_data_bme280() {
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pres, temp, hum, tempUnit, presUnit);
  Tb.log_ln("Temp: " + (String(temp)) + " Hum: " + (String(hum)) + 
           " Pres: " + (String(pres)));
}
#endif  // BME280_I2C

/********** Relay functions **************************************************/
void init_relays(const byte PIN_RELAYS[], byte number_of_relays) {
  for (byte i=0; i<number_of_relays; i++) {
    pinMode(PIN_RELAYS[i],OUTPUT);
  }
  for (byte i=0; i<number_of_relays; i++) {  // all off (neg. logic!)
    digitalWrite(PIN_RELAYS[i],HIGH);
  }
}

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
