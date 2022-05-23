/*!!!!!! Things you must change: !!!!!!*/

/****** WiFi SSID and PASSWORD ******/
const char *MY_WIFI_SSID = "your_ssid";
const char *MY_WIFI_PASSWORD = "your_password";

/****** MQTT settings ******/
const char *MQTT_SERVER = "192.168.178.222";

/*+++++++ Things you can change: +++++++*/

/******* Automated watering array *******/
bool auto_flag = 1;
struct we {
  const byte relay_nr;
  const unsigned int start_time;
  const unsigned int duration;
};

// A triple of data per watering event:
// relay number, time (hr*100+min), minutes to water
// never use 2 relays at the same time

we watering_events[] = {
  {0,1800,10}, // relay number, time (hr*100+min), minutes to water
  {1,1811,20},
  {0,2000,20},
  {0,2030,20},
};  

/****** Publishes every in milliseconds ******/
const long PUBLISH_TIME = 60000;

/******* Relays *******/
const byte NR_OF_RELAYS = 5;
const byte PIN_RELAYS[NR_OF_RELAYS] = {16, 14, 12, 13, 0};
bool relay_start_flags[NR_OF_RELAYS] = {false,false,false,false,false};
bool relay_stop_flags[NR_OF_RELAYS] = {true,true,true,true,true};
unsigned long relay_times_ms[NR_OF_RELAYS] = {10000,20000,30000,40000,50000};
unsigned long relay_prev_millis[NR_OF_RELAYS] = {0,0,0,0,0};

/****** MQTT settings ******/
const char *MQTT_CLIENT_ID = "garden_watering"; // this must be unique!!!
String MQTT_TOPIC_OUT = "weigu/garden/watering/data";
String MQTT_TOPIC_IN = "weigu/garden/watering/command";
const short MY_MQTT_PORT = 1883; // or 8883
// only if you use MQTTPASSWORD (uncomment //#define MQTTPASSWORD in ino file)
const char *MY_MQTT_USER = "me";
const char *MY_MQTT_PASS = "meagain";

/****** WiFi and network settings ******/
const char *NET_MDNSNAME = "watering";      // optional (access with SmartyReaderLAM.local)
const char *NET_HOSTNAME = "watering";      // optional
const word UDP_LOG_PORT = 6666;             // UDP logging settings if enabled in setup()
const byte UDP_LOG_PC_IP_BYTES[4] = {192, 168, 1, 50};
const char *NTP_SERVER = "lu.pool.ntp.org"; // NTP settings
// your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";

// only if you use OTA (uncomment //#define OTA in ino file)
const char *MY_OTA_NAME = "garden_watering"; // optional (access with garden_watering.local)
const char *MY_OTA_PASS_HASH = "myHash";     // Hash for password

// only if you use a static address (uncomment //#define STATIC in ino file)
const byte NET_LOCAL_IP_BYTES[4] = {192, 168, 178, 155};
const byte NET_GATEWAY_BYTES[4] = {192, 168, 178, 1};
const byte NET_MASK_BYTES[4] = {255,255,255,0};  
