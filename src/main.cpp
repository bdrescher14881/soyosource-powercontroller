/***************************************************************************
  soyosource-powercontroller @matlen67

  Version: 1.241013

  16.03.2024 -> Speichern der Checkboxzustände: aktiv Timer1 / Timer2
  03.04.2024 -> Statusübersicht bei geschlossenen details/summary boxen
  14.04.2024 -> Falls Batterieschutz aktiviert, deaktiviere Regelung der Nulleinspeisung
  25.04.2024 -> Leistungspunkt bei Nulleinspeisung festlegen
                (Bei mir funktioniert gut Intervall Shelly 1000ms & Intervall Nulleinspeisung 4000ms)
  26.04.2024 -> Auswahl der aktiven Leiter (L1, L2, L3) beim Shelly
  27.04.2024 -> Fehlerbehebung Shelly 3EM, Shelly Plus 1PM mit zugefügt
  28.04.2024 -> Teiler unter 'SoyoSource Output' hinzugefügt, um die Leistung auf mehere Geräte aufzuteilen
  29.04.2024 -> Telnet entfernt
  05.05.2024 -> update ArduinoJson to 7.0.4
  08.05.2024 -> mqtt topic voltage & soc bearbeitbar 
  21.05.2024 -> Nulleinspeisung update Output auf 5000
  13.10.2024 -> Teiler Output auf 6 erhöht
                Shelly 3EM Pro -> Auswertung Json bei Einphasiger Nutzung


  *************************
  Wiring
  NodeMCU D1 - RS485 RO
  NodeMCU D3 - RS485 DE/RE
  NodeMCU D4 - RS485 DI

****************************************************************************/

#include <Arduino.h> 
#include <LittleFS.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPAsync_WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Uptime.h>
#include <time.h>
#include "html.h"

#define DEBUG_SERIAL Serial

#define DEBUG

#define FW_VERSION "1.241013"

#ifdef DEBUG
  #define DBG_PRINT(x) DEBUG_SERIAL.print(x)
  #define DBG_PRINTLN(x) DEBUG_SERIAL.println(x)
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
#endif

//*****************************************************************************
// da Serial.printf(x,x) mit define nicht funktioniert als workaround sprintf
// sprintf(dbgbuffer,"ESP_%02X%02X%02X", mac[3], mac[4], mac[5]);
// DBG_PRINTLN(dbgbuffer);
//*****************************************************************************
char dbgbuffer[128]; 

#define RXPin        D1  // Serial Receive pin (D1)
#define TXPin        D4  // Serial Transmit pin (D4)
 
//RS485 control
#define SERIAL_COMMUNICATION_CONTROL_PIN D3 // Transmission set pin (D3)
#define RS485_TX_PIN_VALUE HIGH
#define RS485_RX_PIN_VALUE LOW

// time server
#define MY_NTP_SERVER "de.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0/2,M10.5.0/3"   


SoftwareSerial RS485Serial(RXPin, TXPin); // RX, TX
WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer server(80);
AsyncEventSource events("/events");
AsyncDNSServer dns;


// Uptime Global Variables
Uptime uptime;
uint8_t Uptime_Years = 0U, Uptime_Months = 0U, Uptime_Days = 0U, Uptime_Hours = 0U, Uptime_Minutes = 0U, Uptime_Seconds = 0U;
uint16_t Uptime_TotalDays = 0U; // Total Uptime Days
char uptime_str[37];  

// Wifi to percent
const int RSSI_MAX =-50;  // max strength signal in dBm
const int RSSI_MIN =-100; // min strength signal in dBm

//Timer
unsigned long timerSoyoSource = 555;
unsigned long lastTimerSoyoSource = 0;  

unsigned long timerUptime = 1000;
unsigned long lastTimerUptime = 0;  

unsigned long meterinterval = 2000;
unsigned long lastMeterinterval = 0;  

unsigned long nullinterval = 5000;
unsigned long lastNullinterval = 0;  



//mqtt
char mqtt_server[16] = "192.168.178.10";
char mqtt_port[5] = "1889";
char mqtt_user[32] = "";
char mqtt_pass[32] = "";
char msgData[64];
String msg = "";
char mqtt_topic_bat_voltage [48] = "VenusOS/SmartShunt/voltage";
char mqtt_topic_bat_soc [48] = "VenusOS/SmartShunt/soc";

String dataReceived;
int data;
bool isDataReceived = false;
uint8_t byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7; 
int byteSend;
int data_array[8];
int soyo_hello_data[8] = {0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // bit7 org 0x00, CRC 0xFF
int soyo_power_data[8] = {0x24, 0x56, 0x00, 0x21, 0x00, 0x00, 0x80, 0x08}; // 0 Watt
int soyo_text_data[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

char buffer[512]; // mqtt payload buffer, gross genug fuer JSON-Payloads (z.B. Tasmota SENSOR)
int old_soyo_power = 0;
int soyo_power = 0;
int new_soyo_power = 0;
int teiler_output = 1;

unsigned char mac[6];
char mqtt_root[32] = "SoyoSource/";
char clientId[16];
char topic_power[40];
char soyo_text[40];

// Home Assistant MQTT discovery & state/command topics
char topic_meterpower[40];
char topic_uptime[40];
char topic_wifi[40];
char topic_null_state[40];
char topic_timer1_state[40];
char topic_timer2_state[40];
char topic_batt_state[40];
char topic_teiler[40];
char topic_maxwatt[40];
char topic_nulloffset[40];

char topic_cmd_power[40];
char topic_cmd_null[40];
char topic_cmd_timer1[40];
char topic_cmd_timer2[40];
char topic_cmd_batschutz[40];
char topic_cmd_teiler[40];
char topic_cmd_maxwatt[40];
char topic_cmd_nulloffset[40];
char topic_cmd_wildcard[40];

unsigned long timerMqttState = 5000;
unsigned long lastMqttState = 0;

float mqtt_bat_soc = 0.0;
float mqtt_bat_voltage = 0.0;

long rssi;

time_t now;                       
tm timeInfo;


// timer
char currentTime[20];
char timer1_time[6] = "06:00";
char timer2_time[6] = "20:00";
char meteripaddr[16] = "";

int timer1_watt = 0;
int timer2_watt = 0;
int maxwatt = 0;

//state checkboxes
bool checkbox_timer1 = false;
bool checkbox_timer2 = false;
bool checkbox_mqttenabled = false;
bool checkbox_nulleinspeisung = false;
bool checkbox_batschutz = false;
bool checkbox_meter_l1 = true;
bool checkbox_meter_l2 = true;
bool checkbox_meter_l3 = true;

char metername[24] = "Meter";
char mqtt_state[20] = "disabled";

// variablen Shelly 3em
const int shelly_3em_pro = 1;   // ip/rpc/Shelly.GetStatus
const int shelly_plus_1pm = 2;  // ip/rpc/Shelly.GetStatus

const int shelly_3em = 10;      // ip/status
const int shelly_em = 11;       // ip/status
const int shelly_1pm = 12;      // ip/status


String shelly_ip = "";
int shelly_model = 0 ;

// firmware update aus GitHub releases
const char* FW_MANIFEST_URL = "https://github.com/bdrescher14881/soyosource-powercontroller/releases/latest/download/manifest.json";
const char* FW_BIN_URL      = "https://github.com/bdrescher14881/soyosource-powercontroller/releases/latest/download/firmware.bin.gz";

bool do_update_check = false;       // Check vom Webinterface angefordert
bool do_fw_update = false;          // Installation vom Webinterface angefordert
bool fw_update_available = false;
char fw_update_version[16] = "";
char fw_update_state[48] = "";
unsigned long lastUpdateCheck = 0;
const unsigned long updateCheckInterval = 86400000UL; // einmal taeglich

// meter quelle: 0 = Shelly (HTTP), 1 = Tasmota (HTTP), 2 = MQTT-Topic, 3 = HomeWizard (HTTP)
uint8_t meter_source = 0;
char meter_json_path[48] = "";              // JSON-Pfad zum Leistungswert, z.B. "MT175.P" (Tasmota/MQTT)
char topic_meter_in[64] = "";               // MQTT-Topic des Energiemeters (nur meter_source 2)
bool checkbox_meter_invert = false;         // Vorzeichen des Meterwerts umdrehen
unsigned long lastMeterMsg = 0;             // Zeitstempel letzter Meterwert (Staleness-Ueberwachung MQTT)
const unsigned long meterTimeout = 30000;   // nach 30s ohne MQTT-Meterwert wird auf 0 geregelt

//nulleinspeisung
int nulloffset = 0;
int meter_power = 0;
int meterpower = 0;
int meterl1 = 0;
int meterl2 = 0;
int meterl3 = 0;

//batterieüberwachung
int batsocstop = 15;
int batsocstart = 50;
bool output_enabled = true;


bool new_connect = true;

const char* PARAM_MESSAGE = "message";

//flag for saving data
bool shouldSaveConfig = false;


//callback notifying us of the need to save config
void saveConfigCallback () {
  DBG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}


void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}


int dBmtoPercent(int dBm){
  int percent;
  if(dBm <= RSSI_MIN){
    percent = 0;
  } else if(dBm >= RSSI_MAX) {  
    percent = 100;
  } else {
    percent = 2 * (dBm + 100);
  }

  return percent;
} 


void myUptime(){
  uptime.calculateUptime();                                  

  // Get The Uptime Values To Global Variables
  Uptime_Years      = uptime.getYears();
  Uptime_Months     = uptime.getMonths();
  Uptime_Days       = uptime.getDays();
  Uptime_Hours      = uptime.getHours();
  Uptime_Minutes    = uptime.getMinutes();
  Uptime_Seconds    = uptime.getSeconds();
  Uptime_TotalDays  = uptime.getTotalDays();

  if (Uptime_Years == 0U) {                                  // Uptime Is Less Than One Year
    // First 60 Seconds
    if (Uptime_Minutes == 0U && Uptime_Hours == 0U && Uptime_Days == 0U && Uptime_Months == 0U)
      sprintf(uptime_str, "00:00:%02i", Uptime_Seconds);
    // First Minute
    else if (Uptime_Minutes == 1U && Uptime_Hours == 0U && Uptime_Days == 0U && Uptime_Months == 0U)
      sprintf(uptime_str, "00:%02i:%02i", Uptime_Minutes, Uptime_Seconds);
    // Second Minute And More But Less Than Hours, Days, Months
    else if (Uptime_Minutes >= 2U && Uptime_Hours == 0U && Uptime_Days == 0U && Uptime_Months == 0U)
      sprintf(uptime_str, "00:%02i:%02i", Uptime_Minutes, Uptime_Seconds);
    // First Hour And More But Less Than Days, Months
    else if (Uptime_Hours >= 1U && Uptime_Days == 0U && Uptime_Months == 0U)
      sprintf(uptime_str, "%02i:%02i:%02i", Uptime_Hours, Uptime_Minutes, Uptime_Seconds);
    // First Day And Less Than Month
    else if (Uptime_Days == 1U && Uptime_Months == 0U)
      sprintf(uptime_str, "%iday %02i:%02i:%02i", Uptime_Days, Uptime_Hours, Uptime_Minutes, Uptime_Seconds);
    // Second Day And More But Less Than Month
    else if (Uptime_Days >= 2U && Uptime_Months == 0U)
      sprintf(uptime_str, "%idays %02i:%02i:%02i", Uptime_Days, Uptime_Hours, Uptime_Minutes, Uptime_Seconds);
    // First Month And More But Less Than One Year
    else if (Uptime_Months >= 1U)
      sprintf(uptime_str, "%im, %id %02i:%02i", Uptime_Months, Uptime_Days, Uptime_Hours, Uptime_Minutes);
    // If There Is Any Error In This If Loop Then Make Full String.
    else sprintf(uptime_str, "%iy %im %id %02i:%02i", Uptime_Years, Uptime_Months, Uptime_Days, Uptime_Hours, Uptime_Minutes);
  } else                                                     // Uptime Is More Than One Year
    sprintf(uptime_str, "%iy %im %id %02i:%02i", Uptime_Years, Uptime_Months, Uptime_Days, Uptime_Hours, Uptime_Minutes);
}


//callback from mqtt
// loest einen gepunkteten JSON-Pfad wie "MT175.P" oder "SML.Power_curr" auf
float resolveJsonPath(JsonVariant root, const char* path) {
  if (path == NULL || path[0] == '\0') {
    return NAN;
  }

  JsonVariant v = root;
  char pathbuf[48];
  strncpy(pathbuf, path, sizeof(pathbuf) - 1);
  pathbuf[sizeof(pathbuf) - 1] = '\0';

  char* tok = strtok(pathbuf, ".");
  while (tok != NULL) {
    v = v[tok];
    if (v.isNull()) {
      return NAN;
    }
    tok = strtok(NULL, ".");
  }

  if (!v.is<float>()) {
    return NAN;
  }
  return v.as<float>();
}


// uebernimmt einen Meterwert aus Tasmota/MQTT in die Regelung
void applyMeterValue(float value) {
  if (checkbox_meter_invert) {
    value = -value;
  }
  meter_power = (int)value;
  meterpower = (int)value;
  meterl1 = 0;
  meterl2 = 0;
  meterl3 = 0;
  lastMeterMsg = millis();
}


void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  unsigned int i = 0;

  if (length > sizeof(buffer) - 1) { // Payload auf Puffergroesse begrenzen
    length = sizeof(buffer) - 1;
  }

  for (i=0;i<length;i++) {
    buffer[i] = char(payload[i]);
  }
  buffer[i] = '\0';

  // Energiemeter per MQTT (meter_source 2): Payload ist Zahl oder JSON mit Pfad
  if (meter_source == 2 && strlen(topic_meter_in) > 0 && strcmp(topic, topic_meter_in) == 0) {
    float meter_value = NAN;
    if (buffer[0] == '{') {
      JsonDocument doc;
      if (deserializeJson(doc, buffer) == DeserializationError::Ok) {
        meter_value = resolveJsonPath(doc.as<JsonVariant>(), meter_json_path);
        if (isnan(meter_value)) {
          meter_value = resolveJsonPath(doc["StatusSNS"], meter_json_path); // Tasmota tele/SENSOR Format
        }
      }
    } else {
      meter_value = atof(buffer);
    }
    if (!isnan(meter_value)) {
      applyMeterValue(meter_value);
      strcpy(metername, "MQTT Meter");
    }
  }

  if (strcmp(topic, topic_power) == 0){
    int arrived_value_i = atoi(buffer);
    if (arrived_value_i >= 0 && arrived_value_i <= 3000) {
      soyo_power = arrived_value_i;
    }
  }

  if(strcmp(topic, mqtt_topic_bat_soc) == 0){
    float arrived_value_f = atof(buffer);
    mqtt_bat_soc = arrived_value_f;
  }

  if(strcmp(topic, mqtt_topic_bat_voltage) == 0){
    float arrived_value_f = atof(buffer);
    mqtt_bat_voltage = arrived_value_f;
  }

  // Home Assistant command topics: <mqtt_root>/<entity>/set
  size_t root_len = strlen(mqtt_root);
  if (strncmp(topic, mqtt_root, root_len) == 0 && topic[root_len] == '/') {
    const char* sub = topic + root_len + 1;
    bool on = (strcmp(buffer, "ON") == 0 || strcmp(buffer, "1") == 0);

    if (strcmp(sub, "power/set") == 0) {
      int v = atoi(buffer);
      if (v >= 0 && v <= 3000) {
        soyo_power = v;
      }
    } else if (strcmp(sub, "null/set") == 0) {
      checkbox_nulleinspeisung = on;
      if (!on) {
        soyo_power = 0;
      }
    } else if (strcmp(sub, "timer1/set") == 0) {
      checkbox_timer1 = on;
    } else if (strcmp(sub, "timer2/set") == 0) {
      checkbox_timer2 = on;
    } else if (strcmp(sub, "batschutz/set") == 0) {
      checkbox_batschutz = on;
      if (!on) {
        output_enabled = true;
      }
    } else if (strcmp(sub, "teiler/set") == 0) {
      int v = atoi(buffer);
      if (v >= 1 && v <= 6) {
        teiler_output = v;
      }
    } else if (strcmp(sub, "maxwatt/set") == 0) {
      int v = atoi(buffer);
      if (v >= 0 && v <= 5000) {
        maxwatt = v;
      }
    } else if (strcmp(sub, "nulloffset/set") == 0) {
      int v = atoi(buffer);
      if (v >= 0 && v <= 200) {
        nulloffset = v;
      }
    }
  }
}


String processor(const String& var){ 
  return String();
}


// add the shared device info block used by all Home Assistant discovery messages
void addDeviceInfo(JsonObject doc) {
  JsonObject device = doc["dev"].to<JsonObject>();
  JsonArray ids = device["ids"].to<JsonArray>();
  ids.add(clientId);
  device["name"] = String("SoyoSource ") + clientId;
  device["mf"] = "matlen67";
  device["mdl"] = "SoyoSource PowerController";
  device["sw"] = FW_VERSION;
}


void publishDiscoverySensor(const char* obj_id, const char* name, const char* state_topic, const char* unit, const char* icon) {
  char config_topic[80];
  sprintf(config_topic, "homeassistant/sensor/%s/%s/config", clientId, obj_id);

  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = String(clientId) + "_" + obj_id;
  doc["stat_t"] = state_topic;
  if (unit) doc["unit_of_meas"] = unit;
  if (icon) doc["icon"] = icon;
  addDeviceInfo(doc.as<JsonObject>());

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  client.publish(config_topic, payload, true);
}


void publishDiscoverySwitch(const char* obj_id, const char* name, const char* state_topic, const char* command_topic, const char* icon) {
  char config_topic[80];
  sprintf(config_topic, "homeassistant/switch/%s/%s/config", clientId, obj_id);

  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = String(clientId) + "_" + obj_id;
  doc["stat_t"] = state_topic;
  doc["cmd_t"] = command_topic;
  if (icon) doc["icon"] = icon;
  addDeviceInfo(doc.as<JsonObject>());

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  client.publish(config_topic, payload, true);
}


void publishDiscoveryNumber(const char* obj_id, const char* name, const char* state_topic, const char* command_topic, float min_v, float max_v, float step, const char* unit, const char* icon) {
  char config_topic[80];
  sprintf(config_topic, "homeassistant/number/%s/%s/config", clientId, obj_id);

  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = String(clientId) + "_" + obj_id;
  doc["stat_t"] = state_topic;
  doc["cmd_t"] = command_topic;
  doc["min"] = min_v;
  doc["max"] = max_v;
  doc["step"] = step;
  if (unit) doc["unit_of_meas"] = unit;
  if (icon) doc["icon"] = icon;
  addDeviceInfo(doc.as<JsonObject>());

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  client.publish(config_topic, payload, true);
}


// publish Home Assistant MQTT discovery messages (retained) for all entities
void sendMqttDiscovery() {
  DBG_PRINTLN("send MQTT discovery");

  publishDiscoverySensor("power", "SoyoSource Power", topic_power, "W", "mdi:flash");
  publishDiscoverySensor("meter_power", "Meter Power", topic_meterpower, "W", "mdi:gauge");
  publishDiscoverySensor("uptime", "Uptime", topic_uptime, NULL, "mdi:clock-outline");
  publishDiscoverySensor("wifi_signal", "WiFi Signal", topic_wifi, "%", "mdi:wifi");

  publishDiscoverySwitch("nulleinspeisung", "Nulleinspeisung", topic_null_state, topic_cmd_null, "mdi:transmission-tower");
  publishDiscoverySwitch("timer1", "Timer 1", topic_timer1_state, topic_cmd_timer1, "mdi:timer-outline");
  publishDiscoverySwitch("timer2", "Timer 2", topic_timer2_state, topic_cmd_timer2, "mdi:timer-outline");
  publishDiscoverySwitch("batterieschutz", "Batterieschutz", topic_batt_state, topic_cmd_batschutz, "mdi:battery-alert");

  publishDiscoveryNumber("power_setpoint", "Power Setpoint", topic_power, topic_cmd_power, 0, 3000, 1, "W", "mdi:flash");
  publishDiscoveryNumber("teiler", "Teiler Output", topic_teiler, topic_cmd_teiler, 1, 6, 1, NULL, "mdi:call-split");
  publishDiscoveryNumber("maxwatt", "Max Output", topic_maxwatt, topic_cmd_maxwatt, 0, 5000, 1, "W", "mdi:flash-outline");
  publishDiscoveryNumber("nulloffset", "Nullpunkt Offset", topic_nulloffset, topic_cmd_nulloffset, 0, 200, 1, "W", "mdi:target");
}


void reconnect() {
  DBG_PRINTLN("reconnect MQTT connection!");

  //set callback again
  client.setCallback(mqtt_callback);
  
  uint8_t timeout = 15;

  // wait for connection
  while (!client.connected()){

    DBG_PRINTLN("");
        
    if (client.connect(clientId, mqtt_user, mqtt_pass )) {
      DBG_PRINTLN("connection established");

      client.publish(topic_power, "0");
      client.subscribe(topic_power);
      client.subscribe(mqtt_topic_bat_soc);
      client.subscribe(mqtt_topic_bat_voltage);
      client.subscribe(topic_cmd_wildcard);

      if (meter_source == 2 && strlen(topic_meter_in) > 0) {
        client.subscribe(topic_meter_in);
        lastMeterMsg = millis(); // frisches Zeitfenster fuer Staleness-Ueberwachung

        DBG_PRINT("subscrible: ");
        DBG_PRINT(topic_meter_in);
        DBG_PRINTLN("");
      }

      strcpy(mqtt_state, "connect");

      DBG_PRINT("subscrible: ");
      DBG_PRINT(topic_power);
      DBG_PRINTLN("");

      DBG_PRINT("subscrible: ");
      DBG_PRINT(mqtt_topic_bat_soc);
      DBG_PRINTLN("");

      DBG_PRINT("subscrible: ");
      DBG_PRINT(mqtt_topic_bat_voltage);
      DBG_PRINTLN("");

      DBG_PRINT("subscrible: ");
      DBG_PRINT(topic_cmd_wildcard);
      DBG_PRINTLN("");

      sendMqttDiscovery();

    } else {
      DBG_PRINT("reconnect failed! state=");
      DBG_PRINTLN(client.state());
      strcpy(mqtt_state, "connect error");
      
      while (timeout){
        DBG_PRINT(".");
        timeout--;
        delay(1000);
      }
    }
  }

}


int calc_checksumme(int b1, int b2, int b3, int b4, int b5, int b6 ){
  int calc = (0xFF - b1 - b2 - b3 - b4 - b5 - b6) % 256;
  return calc & 0xFF;
}


void sendSoyoPowerData(int power){
  soyo_power_data[0] = 0x24;
  soyo_power_data[1] = 0x56;
  soyo_power_data[2] = 0x00;
  soyo_power_data[3] = 0x21;
  soyo_power_data[4] = power >> 0x08;
  soyo_power_data[5] = power & 0xFF;
  soyo_power_data[6] = 0x80;
  soyo_power_data[7] = calc_checksumme(soyo_power_data[1], soyo_power_data[2], soyo_power_data[3], soyo_power_data[4], soyo_power_data[5], soyo_power_data[6]);

  for(int i=0; i<8; i++) {
      RS485Serial.write(soyo_power_data[i]);  // send data to RS485 
      //DBG_PRINTLN(soyo_power_data[i], HEX);  
  }
}

//read config.json
void readConfig(){
 //read configuration from json
  DBG_PRINTLN("mounting FS...");

  if (LittleFS.begin()) {
    DBG_PRINTLN("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      DBG_PRINTLN("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        DBG_PRINTLN("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        JsonDocument json;
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError) {
          DBG_PRINTLN("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

          if(json.containsKey("mqtt_user")){    
            strcpy(mqtt_user, json["mqtt_user"]);
          }

          if(json.containsKey("mqtt_pass")){ 
            strcpy(mqtt_pass, json["mqtt_pass"]);
          }

          if(json.containsKey("mqtt_bat_vol")){
            strcpy(mqtt_topic_bat_voltage, json["mqtt_bat_vol"]);
          }

          if(json.containsKey("mqtt_bat_soc")){
            strcpy(mqtt_topic_bat_soc, json["mqtt_bat_soc"]);
          }

          char key_value[2];

          if(json.containsKey("mqtt_on")){
            strcpy(key_value, json["mqtt_on"]);
            if(strcmp(key_value, "1") == 0){
              checkbox_mqttenabled = true;
            }else{
              checkbox_mqttenabled = false;
            }
          }

          if(json.containsKey("zft_on")){
            strcpy(key_value, json["zft_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_nulleinspeisung = true;
            }else{
              checkbox_nulleinspeisung = false;
            }
          }

          if(json.containsKey("batp_on")){
            strcpy(key_value, json["batp_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_batschutz = true;
            }else{
              checkbox_batschutz = false;
            }
          }

          if(json.containsKey("t1_on")){
            strcpy(key_value, json["t1_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_timer1 = true;
            }else{
              checkbox_timer1 = false;
            }
          }

          if(json.containsKey("t2_on")){
            strcpy(key_value, json["t2_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_timer2 = true;
            }else{
              checkbox_timer2 = false;
            }
          }

          if(json.containsKey("mtr_l1_on")){
            strcpy(key_value, json["mtr_l1_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_meter_l1 = true;
            }else{
              checkbox_meter_l1 = false;
            }
          }

          if(json.containsKey("mtr_l2_on")){
            strcpy(key_value, json["mtr_l2_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_meter_l2 = true;
            }else{
              checkbox_meter_l2 = false;
            }
          }

          if(json.containsKey("mtr_l3_on")){
            strcpy(key_value, json["mtr_l3_on"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_meter_l3 = true;
            }else{
              checkbox_meter_l3 = false;
            }
          }


          if(json.containsKey("t1_t")){
            strcpy(timer1_time, json["t1_t"]);            
          }

          if(json.containsKey("t2_t")){
            strcpy(timer2_time, json["t2_t"]);
          }

          if(json.containsKey("t1_p")){
            timer1_watt = json["t1_p"];
          }

          if(json.containsKey("t2_p")){
            timer2_watt = json["t2_p"];  
          }

          if(json.containsKey("mp")){
            maxwatt = json["mp"];  
          }

          if(json.containsKey("mtr_ip")){
            strcpy(meteripaddr, json["mtr_ip"]);
            shelly_ip = String(meteripaddr);
          }

          if(json.containsKey("mtr_src")){
            meter_source = json["mtr_src"];
          }

          if(json.containsKey("mtr_json")){
            strcpy(meter_json_path, json["mtr_json"]);
          }

          if(json.containsKey("mtr_topic")){
            strcpy(topic_meter_in, json["mtr_topic"]);
          }

          if(json.containsKey("mtr_inv")){
            strcpy(key_value, json["mtr_inv"]);

            if(strcmp(key_value, "1") == 0){
              checkbox_meter_invert = true;
            }else{
              checkbox_meter_invert = false;
            }
          }

          if(json.containsKey("mtr_iv")){
            meterinterval = json["mtr_iv"]; 
          }

          if(json.containsKey("z_iv")){
            nullinterval = json["z_iv"]; 
          }

          if(json.containsKey("z_ofs")){
            nulloffset = json["z_ofs"]; 
          }

          if(json.containsKey("soc_stop")){
            batsocstop = json["soc_stop"]; 
          }

          if(json.containsKey("soc_start")){
            batsocstart = json["soc_start"]; 
          }

          if(json.containsKey("tout")){
            teiler_output = json["tout"]; 
          }

        } else {
          DBG_PRINTLN("failed to load json config");
        }
      }
    }
  } else {
    DBG_PRINTLN("failed to mount FS");
  }
  //end read config data
}


// write config.json
void saveConfig(){
  DBG_PRINTLN(F("save data to config.json"));
  JsonDocument json;
 
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_pass"] = mqtt_pass;
  json["mqtt_bat_vol"] = mqtt_topic_bat_voltage;
  json["mqtt_bat_soc"] = mqtt_topic_bat_soc;


  if(checkbox_mqttenabled){
    json["mqtt_on"] = "1";
  }else{
    json["mqtt_on"] = "0";
  }

  if(checkbox_nulleinspeisung){
    json["zft_on"] = "1";
  }else{
    json["zft_on"] = "0";
  }

  if(checkbox_batschutz){
    json["batp_on"] = "1";
  }else{
    json["batp_on"] = "0";
  }

  if(checkbox_timer1){
    json["t1_on"] = "1";
  }else{
    json["t1_on"] = "0";
  }

  if(checkbox_timer2){
    json["t2_on"] = "1";
  }else{
    json["t2_on"] = "0";
  }

  if(checkbox_meter_l1){
    json["mtr_l1_on"] = "1";
  }else{
    json["mtr_l1_on"] = "0";
  }

   if(checkbox_meter_l2){
    json["mtr_l2_on"] = "1";
  }else{
    json["mtr_l2_on"] = "0";
  }

   if(checkbox_meter_l3){
    json["mtr_l3_on"] = "1";
  }else{
    json["mtr_l3_on"] = "0";
  }

  json["t1_t"] = timer1_time;
  json["t1_p"] = timer1_watt;
  json["t2_t"] = timer2_time;
  json["t2_p"] = timer2_watt;
  json["mp"] = maxwatt;
  json["mtr_ip"] = meteripaddr;
  json["mtr_src"] = meter_source;
  json["mtr_json"] = meter_json_path;
  json["mtr_topic"] = topic_meter_in;

  if(checkbox_meter_invert){
    json["mtr_inv"] = "1";
  }else{
    json["mtr_inv"] = "0";
  }

  json["mtr_iv"] = meterinterval;
  json["z_iv"] = nullinterval;
  json["z_ofs"] = nulloffset;
  json["soc_stop"] = batsocstop;
  json["soc_start"] = batsocstart;
  json["tout"] = teiler_output;
  
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    DBG_PRINTLN("failed to open config file for writing");
    return;
  }

  serializeJson(json, configFile);
  configFile.close();

  serializeJson(json, Serial);
  DBG_PRINTLN();
}


// get shelly type(3EM PRO, 3EM, EM, 1PM, Plus 1PM)
int getShellyType(){ 
  String shelly_url = "http://" + shelly_ip +  "/shelly";
  int type = 0;

  memset(metername, 0, sizeof(metername)); 
  strcat(metername, "no device");    
   
  JsonDocument doc;
 
  WiFiClient client_shelly;
  HTTPClient http;

  if (http.begin(client_shelly, shelly_url)) { 
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();   
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
          DBG_PRINT(F("deserializeJson() failed: "));
          DBG_PRINTLN(error.f_str());
        }

        String json_type = doc["type"];
        String json_model = doc["model"];

        if(json_type != NULL){

          //test auf Shelly 1PM
          if(json_type.equals("SHSW-PM")){
            type = shelly_1pm;
            memset(metername, 0, sizeof(metername)); 
            strcat(metername, "Shelly 1PM");   
          }
          //test auf Shelly EM
          if(json_type.equals("SHEM")){
            type = shelly_em;
            memset(metername, 0, sizeof(metername)); 
            strcat(metername, "Shelly EM");
          }

          //test auf Shelly 3EM
          if(json_type.equals("SHEM-3")){
            type = shelly_3em;
            memset(metername, 0, sizeof(metername)); 
            strcat(metername, "Shelly 3EM");   
          }
        }


        if(json_model != NULL){

          //test auf Shelly 3EM Pro
          if(json_model.equals("SPEM-003CEBEU")) {
            type = shelly_3em_pro;
            memset(metername, 0, sizeof(metername)); 
            strcat(metername, "Shelly 3EM Pro");     
          }

          //test auf Shelly Plus 1PM
          if(json_model.equals("SNSW-001P16EU")) {
            type = shelly_plus_1pm;
            memset(metername, 0, sizeof(metername)); 
            strcat(metername, "Shelly Plus 1PM");     
          } 
        }

      }
    }
    http.end();
  }
  DBG_PRINT("getShellyType() = ");
  DBG_PRINTLN(String(metername));

  return type;
}


// read shelly3EM
int getMeterData(int type) {
  String shelly_url;
  int power = 0;
  int power1 = 0;
  int power2 = 0;
  int power3 = 0; 
  
  JsonDocument json;
  WiFiClient client_shelly;
  HTTPClient http;
   
  if (type > 0 && type < 10) { 
    shelly_url = "http://" + shelly_ip +  "/rpc/Shelly.GetStatus"; // Shelly PRO 3EM
  } else if(type >= 10) {
    shelly_url = "http://" + shelly_ip +  "/status";  // Shelly 3EM und Andere
  } else{
    return 0;
  }                      
  
  if (http.begin(client_shelly, shelly_url))  {  
    int httpCode = http.GET();         
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();   
        DeserializationError error = deserializeJson(json, payload);
                
        if (error) {
          DBG_PRINT(F("deserializeJson() failed: "));
          DBG_PRINTLN(error.f_str());
        }

       
        if (type == shelly_3em_pro) {
          if(json.containsKey("em:0")){ 
            power1 = json["em:0"]["a_act_power"];  
            power2 = json["em:0"]["b_act_power"];
            power3 = json["em:0"]["c_act_power"]; 
          }
          else if (json.containsKey("em1:0")){
            power1 = json["em1:0"]["act_power"];  
            power2 = json["em1:1"]["act_power"];
            power3 = json["em1:2"]["act_power"]; 
          }
        } else if (type == shelly_3em) {
          power1 = json["emeters"][0]["power"]; 
          power2 = json["emeters"][1]["power"]; 
          power3 = json["emeters"][2]["power"]; 
        } else if (type == shelly_em) {
          power1 = json["meters"][0]["power"]; 
          power2 = json["meters"][1]["power"]; 
          power3 = 0; 
        } else if (type == shelly_1pm) {
          power1 = json["meters"][0]["power"];
          power2 = 0;
          power3 = 0;   
        } else if (type == shelly_plus_1pm) {
          power1 = json["switch:0"]["apower"]; 
          power2 = 0;
          power3 = 0;
        }  

              
        if(!checkbox_meter_l1){
          power1 = 0;
        }

        if(!checkbox_meter_l2){
          power2 = 0;
        }

        if(!checkbox_meter_l3){
          power3 = 0;
        }

        power = power1 + power2 + power3;

        meterpower = power;
        meterl1 = power1;
        meterl2 = power2;
        meterl3 = power3;
      }

    } else {
      sprintf(dbgbuffer,"[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      DBG_PRINTLN(dbgbuffer);
      shelly_model = 0;
    }

    http.end();
  } else {
    DBG_PRINTLN("[HTTP] Unable to connect\n");
    shelly_model = 0;
  }

  return power;
}


// get meter power from Tasmota (z.B. IR-Lesekopf am Stromzaehler) via Status 8 / StatusSNS
int getTasmotaData() {
  String tasmota_url = "http://" + String(meteripaddr) + "/cm?cmnd=Status%208";
  float value = NAN;

  JsonDocument json;
  WiFiClient client_tasmota;
  HTTPClient http;

  if (http.begin(client_tasmota, tasmota_url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      if (deserializeJson(json, payload) == DeserializationError::Ok) {
        value = resolveJsonPath(json.as<JsonVariant>(), meter_json_path);
        if (isnan(value)) {
          value = resolveJsonPath(json["StatusSNS"], meter_json_path); // Pfad relativ zu StatusSNS erlauben
        }
      }
    } else {
      sprintf(dbgbuffer,"[HTTP] Tasmota GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      DBG_PRINTLN(dbgbuffer);
    }
    http.end();
  }

  if (isnan(value)) {
    memset(metername, 0, sizeof(metername));
    strcat(metername, "Tasmota: keine Daten");
    return 0;
  }

  memset(metername, 0, sizeof(metername));
  strcat(metername, "Tasmota");
  applyMeterValue(value);
  return meter_power;
}


// get meter power from HomeWizard (P1 Meter, Energy Socket, kWh Meter) via /api/v1/data
// Voraussetzung: "Lokale API" in der HomeWizard Energy App aktiviert
int getHomeWizardData() {
  String homewizard_url = "http://" + String(meteripaddr) + "/api/v1/data";
  float value = NAN;
  float power1 = 0;
  float power2 = 0;
  float power3 = 0;
  bool has_phases = false;

  JsonDocument json;
  WiFiClient client_homewizard;
  HTTPClient http;

  if (http.begin(client_homewizard, homewizard_url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      if (deserializeJson(json, payload) == DeserializationError::Ok) {
        if (json.containsKey("active_power_w")) {
          value = json["active_power_w"];
        }
        if (json.containsKey("active_power_l1_w")) { // P1 Meter liefert Phasenwerte
          has_phases = true;
          power1 = json["active_power_l1_w"];
          if (json.containsKey("active_power_l2_w")) {
            power2 = json["active_power_l2_w"];
          }
          if (json.containsKey("active_power_l3_w")) {
            power3 = json["active_power_l3_w"];
          }
        }
      }
    } else {
      sprintf(dbgbuffer,"[HTTP] HomeWizard GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      DBG_PRINTLN(dbgbuffer);
    }
    http.end();
  }

  if (isnan(value)) {
    memset(metername, 0, sizeof(metername));
    strcat(metername, "HomeWizard offline");
    return 0;
  }

  if (has_phases) { // wie beim Shelly 3EM: nur aktivierte Phasen summieren
    if (!checkbox_meter_l1) {
      power1 = 0;
    }
    if (!checkbox_meter_l2) {
      power2 = 0;
    }
    if (!checkbox_meter_l3) {
      power3 = 0;
    }
    value = power1 + power2 + power3;
  }

  memset(metername, 0, sizeof(metername));
  strcat(metername, "HomeWizard");
  applyMeterValue(value);

  if (has_phases) {
    meterl1 = (int)power1;
    meterl2 = (int)power2;
    meterl3 = (int)power3;
  }

  return meter_power;
}


// prueft das neueste GitHub-Release auf eine neue Firmware-Version (manifest.json)
void checkFwUpdate() {
  DBG_PRINTLN("checking for firmware update...");
  strcpy(fw_update_state, "pruefe...");

  // setInsecure: keine Zertifikatspruefung (zu wenig Heap fuer CA-Store);
  // das gzip-Image selbst ist durch seine CRC32 abgesichert
  BearSSL::WiFiClientSecure client_https;
  client_https.setInsecure();
  client_https.setBufferSizes(1024, 256);

  HTTPClient https;
  https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // GitHub leitet Release-Assets um
  https.setTimeout(10000);

  if (!https.begin(client_https, FW_MANIFEST_URL)) {
    strcpy(fw_update_state, "Check fehlgeschlagen");
    return;
  }

  int httpCode = https.GET();
  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    if (deserializeJson(doc, https.getString()) == DeserializationError::Ok && doc.containsKey("version")) {
      memset(fw_update_version, 0, sizeof(fw_update_version));
      strncat(fw_update_version, doc["version"], sizeof(fw_update_version) - 1);

      if (strcmp(fw_update_version, FW_VERSION) != 0) {
        fw_update_available = true;
        sprintf(fw_update_state, "Update verfuegbar: %s", fw_update_version);
      } else {
        fw_update_available = false;
        sprintf(fw_update_state, "aktuell (%s)", FW_VERSION);
      }
    } else {
      strcpy(fw_update_state, "Manifest ungueltig");
    }
  } else {
    sprintf(fw_update_state, "Check fehlgeschlagen (%d)", httpCode);
  }
  https.end();

  DBG_PRINTLN(fw_update_state);
}


// laedt firmware.bin.gz vom neuesten GitHub-Release und flasht es (Neustart bei Erfolg)
void doFwUpdate() {
  DBG_PRINTLN("starting firmware update from GitHub...");
  strcpy(fw_update_state, "installiere Update...");

  soyo_power = 0; // Regelung steht waehrend des Downloads, sicherheitshalber 0 W vorgeben
  sendSoyoPowerData(0);

  BearSSL::WiFiClientSecure client_https;
  client_https.setInsecure();
  client_https.setBufferSizes(1024, 256);

  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.followRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  t_httpUpdate_return ret = ESPhttpUpdate.update(client_https, FW_BIN_URL);

  // hierher kommt der Code nur, wenn das Update fehlgeschlagen ist (bei Erfolg: Neustart)
  if (ret == HTTP_UPDATE_FAILED) {
    sprintf(fw_update_state, "fehlgeschlagen (%d)", ESPhttpUpdate.getLastError());
    DBG_PRINTLN(ESPhttpUpdate.getLastErrorString());
  } else if (ret == HTTP_UPDATE_NO_UPDATES) {
    strcpy(fw_update_state, "kein Update gefunden");
  }
}


void checkTimer(){
  
  time(&now);
  localtime_r(&now, &timeInfo);

  if (checkbox_timer1 == true){      
      int t1_hour = String(timer1_time).substring(0,2).toInt();
      int t1_min = String(timer1_time).substring(3).toInt();

      if((timeInfo.tm_hour == t1_hour && timeInfo.tm_min == t1_min && timeInfo.tm_sec == 0) || (timeInfo.tm_hour == t1_hour && timeInfo.tm_min == t1_min && timeInfo.tm_sec == 1) ){
        soyo_power = timer1_watt;
      }
  }

  if (checkbox_timer2 == true){    
    int t2_hour = String(timer2_time).substring(0,2).toInt();
    int t2_min = String(timer2_time).substring(3).toInt();  
     
    if((timeInfo.tm_hour == t2_hour && timeInfo.tm_min == t2_min && timeInfo.tm_sec == 0) || (timeInfo.tm_hour == t2_hour && timeInfo.tm_min == t2_min && timeInfo.tm_sec == 1)){
      soyo_power = timer2_watt;
    }
  }

}


//#################### SETUP #######################
void setup() {

  DEBUG_SERIAL.begin(115200);
  delay(500);

  DBG_PRINTLN("");
  DBG_PRINT(F("CPU Frequency = "));
  DBG_PRINT(F_CPU / 1000000);
  DBG_PRINTLN(F(" MHz"));
  
  WiFi.macAddress(mac);
  WiFi.persistent(true); // sonst verliert er nach einem Neustart die IP !!!
  
  sprintf(dbgbuffer,"ESP_%02X%02X%02X", mac[3], mac[4], mac[5]);
  DBG_PRINTLN(dbgbuffer);
  
  //configTime(MY_TZ, MY_NTP_SERVER);
  
  sprintf(clientId, "soyo_%02x%02x%02x", mac[3], mac[4], mac[5] );
  
  //mqtt_root = "SoyoSource/soyo_xxxxxx";
  strcat(mqtt_root, clientId);
  
  //topic_power = "SoyoSource/soyo_xxxxxx/power";
  strcat(topic_power, mqtt_root);
  strcat(topic_power, "/power");

  // Home Assistant state & command topics
  sprintf(topic_meterpower, "%s/meter_power", mqtt_root);
  sprintf(topic_uptime, "%s/uptime", mqtt_root);
  sprintf(topic_wifi, "%s/wifi_signal", mqtt_root);
  sprintf(topic_null_state, "%s/null_state", mqtt_root);
  sprintf(topic_timer1_state, "%s/timer1_state", mqtt_root);
  sprintf(topic_timer2_state, "%s/timer2_state", mqtt_root);
  sprintf(topic_batt_state, "%s/batschutz_state", mqtt_root);
  sprintf(topic_teiler, "%s/teiler", mqtt_root);
  sprintf(topic_maxwatt, "%s/maxwatt", mqtt_root);
  sprintf(topic_nulloffset, "%s/nulloffset", mqtt_root);

  sprintf(topic_cmd_power, "%s/power/set", mqtt_root);
  sprintf(topic_cmd_null, "%s/null/set", mqtt_root);
  sprintf(topic_cmd_timer1, "%s/timer1/set", mqtt_root);
  sprintf(topic_cmd_timer2, "%s/timer2/set", mqtt_root);
  sprintf(topic_cmd_batschutz, "%s/batschutz/set", mqtt_root);
  sprintf(topic_cmd_teiler, "%s/teiler/set", mqtt_root);
  sprintf(topic_cmd_maxwatt, "%s/maxwatt/set", mqtt_root);
  sprintf(topic_cmd_nulloffset, "%s/nulloffset/set", mqtt_root);
  sprintf(topic_cmd_wildcard, "%s/+/set", mqtt_root);


  pinMode(SERIAL_COMMUNICATION_CONTROL_PIN, OUTPUT);
  digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_RX_PIN_VALUE);
  RS485Serial.begin(4800);   // set RS485 baud

  readConfig();

  
  ESPAsync_WMParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  ESPAsync_WMParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  ESPAsync_WMParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  ESPAsync_WMParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 32); 

  ESPAsync_WiFiManager wifiManager(&server, &dns);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(60);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);

  configTime(MY_TZ, MY_NTP_SERVER);
  
  bool res = wifiManager.autoConnect(clientId);
  
  if(!res) {
    DBG_PRINTLN("Failed to connect");
    ESP.restart();
  } else {
    //if you get here you have connected to the WiFi    
    DBG_PRINT("WiFi connected to ");
    DBG_PRINTLN(String(WiFi.SSID()));
    DBG_PRINT("RSSI = ");
    DBG_PRINT(String(WiFi.RSSI()));
    DBG_PRINTLN(" dBm");
    DBG_PRINT("IP address  ");
    DBG_PRINTLN(WiFi.localIP());
    DBG_PRINTLN();

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
    
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      saveConfig();
    }

    DBG_PRINTLN(String("mqttenabled: ") + checkbox_mqttenabled);
    if(checkbox_mqttenabled){
      DBG_PRINTLN("set mqtt server!");
      DBG_PRINTLN(String("mqtt_server: ") + mqtt_server);
      DBG_PRINTLN(String("mqtt_port: ") + mqtt_port);
      DBG_PRINTLN(String("mqtt_user: ") + mqtt_user);
      DBG_PRINTLN(String("mqtt_pass: ") + mqtt_pass);

      client.setServer(mqtt_server, atoi(mqtt_port));
      client.setCallback(mqtt_callback);
    }

    // Handle Web Server Events
    events.onConnect([](AsyncEventSourceClient *client){
      if(client->lastId()){
        DBG_PRINTLN("");
        //DEBUG_SERIAL.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        sprintf(dbgbuffer,"Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        DBG_PRINTLN(dbgbuffer);
        //DEBUG_SERIAL.println("");
      }
      client->send("hello!", NULL, millis(), 10000);
    });

    // Handle Web Server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      new_connect = true;
      request->send_P(200, "text/html", index_html, processor);
    });

    // crate json and fetch data
    server.on("/json", HTTP_GET, [] (AsyncWebServerRequest *request){
      JsonDocument myJson;
      String message = "";

      rssi = WiFi.RSSI();

      myJson["WIFIRSSI"]      = rssi;
      myJson["CLIENTID"]      = clientId;
      myJson["METERNAME"]     = metername;
      myJson["MAXWATTINPUT"]  = maxwatt;
      myJson["TOUT"]          = teiler_output;
      myJson["NULLINTERVAL"]  = nullinterval;
      myJson["NULLOFFSET"]    = nulloffset;
      myJson["METERIP"]       = meteripaddr;
      myJson["METERSRC"]      = meter_source;
      myJson["METERJSON"]     = meter_json_path;
      myJson["METERTOPIC"]    = topic_meter_in;
      myJson["CBMETERINV"]    = checkbox_meter_invert;
      myJson["METERINTERVAL"] = meterinterval;
      myJson["TIMER1TIME"]    = timer1_time;
      myJson["TIMER1WATT"]    = timer1_watt;
      myJson["TIMER2TIME"]    = timer2_time;
      myJson["TIMER2WATT"]    = timer2_watt;
      myJson["MQTTROOT"]      = mqtt_root;
      myJson["MQTTSTATECL"]   = mqtt_state;

      myJson["CBNULL"] = checkbox_nulleinspeisung; //checkbox
      if(checkbox_nulleinspeisung){       // Stausanzeige
        myJson["NULLSTATE"] = "EIN";
      }else{
        myJson["NULLSTATE"] = "AUS";
      }

      myJson["CBMQTTSTATE"] = checkbox_mqttenabled; //checkbox
      if(checkbox_mqttenabled){
        myJson["MQTTSTATE"] = "EIN";
      }else{
        myJson["MQTTSTATE"] = "AUS";
      }

      myJson["CBTIMER1"] = checkbox_timer1; //checkbox
      myJson["CBTIMER2"] = checkbox_timer2; //checkbox
      if(checkbox_timer1 || checkbox_timer2){
        myJson["TIMERSTATE"] = "EIN";
      }else{
        myJson["TIMERSTATE"] = "AUS";
      }

      myJson["CBBATSCHUTZ"] = checkbox_batschutz; //checkbox
      if(checkbox_batschutz){
        myJson["BATTSTATE"] = "EIN";
      }else{
        myJson["BATTSTATE"] = "AUS";
      }

      myJson["CBMETERL1"] = checkbox_meter_l1; //checkbox Shelly L1
      myJson["CBMETERL2"] = checkbox_meter_l2; //checkbox Shelly L2
      myJson["CBMETERL3"] = checkbox_meter_l3; //checkbox Shelly L3

      myJson["MQTTSERVER"] = mqtt_server;
      myJson["MQTTPORT"] = mqtt_port;
      myJson["MQTTUSER"] = mqtt_user;
      myJson["MQTTPASS"] = mqtt_pass;
      myJson["MQTTBATVOL"] = mqtt_topic_bat_voltage;
      myJson["MQTTBATSOC"] = mqtt_topic_bat_soc;
      
      myJson["UPTIME"] = uptime_str;
      myJson["SOYOPOWER"] = soyo_power;
      myJson["METERNAME"] = metername;
      myJson["METERPOWER"] = meterpower;
      myJson["METERL1"] = meterl1;
      myJson["METERL2"] = meterl2;
      myJson["METERL3"] = meterl3;
      myJson["MQTT_SUB_1"] = String(soyo_power) + " W";
      myJson["MQTT_BAT_SOC"] = String(mqtt_bat_soc, 1) + " %";
      myJson["MQTT_BAT_V"] = String(mqtt_bat_voltage, 1) + " V";
      myJson["BATSOCSTOP"] = batsocstop;
      myJson["BATSOCSTART"] = batsocstart;
      myJson["WIFIQUALITI"] = dBmtoPercent(rssi);
      myJson["FWVERSION"] = FW_VERSION;
      myJson["FWUPDATESTATE"] = fw_update_state;
      myJson["FWUPDATEAVAIL"] = fw_update_available;


      serializeJson(myJson, message);

      request->send(200, "application/json", message);
    });

    // start AP Mode
    server.on("/apmode", HTTP_GET, [](AsyncWebServerRequest *request) {
      ESPAsync_WiFiManager wifiManager(&server,&dns);
      wifiManager.resetSettings();
      
      ESP.restart();
      request->send_P(200, "text/html", index_html, processor);
    });

    // restart system
    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {   
      DBG_PRINTLN("/restart");  
      ESP.restart();
      request->send_P(200, "text/html", index_html, processor);
    });

    server.on("/acoutput", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String parm1;
  
      if (request->hasParam("value") ) {
        parm1 = request->getParam("value")->value();
        DBG_PRINT("/acoutput?value = ");
        DBG_PRINTLN(parm1);

        if(parm1.equals("/s0") ){
          soyo_power = 0;
          sprintf(msgData, "%d", soyo_power);
          if(checkbox_mqttenabled){
            client.publish(topic_power, msgData);
          }
        }
        else if(parm1.equals("/p1")){
          soyo_power +=1;
          sprintf(msgData, "%d", soyo_power);
          if(checkbox_mqttenabled){
            client.publish(topic_power, msgData);
          }
        }
        else if(parm1.equals("/p10")){
          soyo_power +=10;
          sprintf(msgData, "%d", soyo_power);
          if(checkbox_mqttenabled){
            client.publish(topic_power, msgData);
          }
        }
        else if(parm1.equals("/m1")){
          soyo_power -=1;
          if(soyo_power < 0){
            soyo_power = 0;
           }
          sprintf(msgData, "%d", soyo_power);
          if(checkbox_mqttenabled){
            client.publish(topic_power, msgData);
          }
        }
        else if(parm1.equals("/m10")){
          soyo_power -=10;
          if(soyo_power < 0){
            soyo_power = 0;
           }
          sprintf(msgData, "%d", soyo_power);
          if(checkbox_mqttenabled){
            client.publish(topic_power, msgData);
          }
        }
      }
      request->send_P(200, "text/html", index_html, processor);
    });


    server.on("/checkbox", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String checkbox_id;
      String checkbox_value;
     
      if (request->hasParam("cbid") && request->hasParam("state")) {
        checkbox_id = request->getParam("cbid")->value();
        checkbox_value = request->getParam("state")->value();
       
        if(checkbox_id.equals("CBTIMER1")){
          if(checkbox_value.equals("1")){
            checkbox_timer1 = true;
          } else {
            checkbox_timer1 = false;
          }
        }
        else if(checkbox_id.equals("CBTIMER2")){
          if(checkbox_value.equals("1")){
            checkbox_timer2 = true;
          } else { 
            checkbox_timer2 = false;
          }
        }
        else if(checkbox_id.equals("CBMQTTSTATE")){
          if(checkbox_value.equals("1")){
            checkbox_mqttenabled = true;
          } else {
            checkbox_mqttenabled = false;
          }
        }
        else if(checkbox_id.equals("CBNULL")){
          if(checkbox_value.equals("1")){
            checkbox_nulleinspeisung = true;
          } else {
            checkbox_nulleinspeisung = false;
            soyo_power = 0;
          }
        }
        else if(checkbox_id.equals("CBBATSCHUTZ")){
          if(checkbox_value.equals("1")){
            checkbox_batschutz = true;
          } else {
            checkbox_batschutz = false;
            output_enabled = true; //wenn batschutz aus, dann freigabe fuer soyo output
          }
        }
        else if(checkbox_id.equals("CBMETERL1")){
          if(checkbox_value.equals("1")){
            checkbox_meter_l1 = true;
          } else {
            checkbox_meter_l1 = false;
          }
        }
        else if(checkbox_id.equals("CBMETERL2")){
          if(checkbox_value.equals("1")){
            checkbox_meter_l2 = true;
          } else {
            checkbox_meter_l2 = false;
          }
        }
        else if(checkbox_id.equals("CBMETERL3")){
          if(checkbox_value.equals("1")){
            checkbox_meter_l3 = true;
          } else {
            checkbox_meter_l3 = false;
          }
        }
      }    
      request->send_P(200, "text/html", index_html, processor);
    });


    server.on("/savesettings", HTTP_GET, [] (AsyncWebServerRequest *request) {
      String value;

      value = request->getParam("t1")->value();
      memset(timer1_time, 0, sizeof(timer1_time)); 
      strcat(timer1_time, value.c_str());     

      value = request->getParam("w1")->value();
      timer1_watt = atoi(value.c_str()); 
    
      value = request->getParam("t2")->value();
      memset(timer2_time, 0, sizeof(timer2_time)); 
      strcat(timer2_time, value.c_str());     

      value = request->getParam("w2")->value();  
      timer2_watt = atoi(value.c_str());  
               
      value = request->getParam("maxwatt")->value();
      maxwatt = atoi(value.c_str()); 

      value = request->getParam("meteripaddr")->value();
      memset(meteripaddr, 0, sizeof(meteripaddr));
      strcat(meteripaddr, value.c_str());

      // neue Meter-Quellen-Parameter; hasParam-Schutz falls Browser noch die alte Seite gecached hat
      uint8_t old_meter_source = meter_source;
      if (request->hasParam("metersrc")) {
        value = request->getParam("metersrc")->value();
        meter_source = atoi(value.c_str());
      }
      if (request->hasParam("meterjson")) {
        value = request->getParam("meterjson")->value();
        memset(meter_json_path, 0, sizeof(meter_json_path));
        strncat(meter_json_path, value.c_str(), sizeof(meter_json_path) - 1);
      }
      if (request->hasParam("metertopic")) {
        value = request->getParam("metertopic")->value();
        memset(topic_meter_in, 0, sizeof(topic_meter_in));
        strncat(topic_meter_in, value.c_str(), sizeof(topic_meter_in) - 1);
      }
      if (request->hasParam("meterinv")) {
        checkbox_meter_invert = (request->getParam("meterinv")->value() == "1");
      }

      value =  request->getParam("tout")->value();
      teiler_output = atoi(value.c_str());

      value =  request->getParam("meterinterval")->value();
      meterinterval = atol(value.c_str());

      value =  request->getParam("nullinterval")->value();
      nullinterval = atol(value.c_str());

      value =  request->getParam("nulloffset")->value();
      nulloffset = atoi(value.c_str());

      value =  request->getParam("mqttserver")->value();
      memset(mqtt_server, 0, sizeof(mqtt_server)); 
      strcat(mqtt_server, value.c_str());

      value =  request->getParam("mqttport")->value();
      memset(mqtt_port, 0, sizeof(mqtt_port)); 
      strcat(mqtt_port, value.c_str());

      value =  request->getParam("mqttuser")->value();
      memset(mqtt_user, 0, sizeof(mqtt_user)); 
      strcat(mqtt_user, value.c_str());

      value =  request->getParam("mqttpass")->value();
      memset(mqtt_pass, 0, sizeof(mqtt_pass)); 
      strcat(mqtt_pass, value.c_str());


      value =  request->getParam("mqttbatvol")->value();
      memset(mqtt_topic_bat_voltage, 0, sizeof(mqtt_topic_bat_voltage)); 
      strcat(mqtt_topic_bat_voltage, value.c_str());

      value =  request->getParam("mqttbatsoc")->value();
      memset(mqtt_topic_bat_soc, 0, sizeof(mqtt_topic_bat_soc)); 
      strcat(mqtt_topic_bat_soc, value.c_str());
      
      value =  request->getParam("batsocstop")->value();
      batsocstop = atoi(value.c_str());

      value =  request->getParam("batsocstart")->value();
      batsocstart = atoi(value.c_str());  
      
      saveConfig();

      shelly_ip = String(meteripaddr);

      if (meter_source != old_meter_source) {
        // Quelle gewechselt: Erkennung und Anzeige zuruecksetzen
        shelly_model = 0;
        meter_power = 0;
        meterpower = 0;
        meterl1 = 0;
        meterl2 = 0;
        meterl3 = 0;
        lastMeterMsg = millis();
        memset(metername, 0, sizeof(metername));
        if (meter_source == 1) {
          strcat(metername, "Tasmota");
        } else if (meter_source == 2) {
          strcat(metername, "MQTT Meter");
        } else if (meter_source == 3) {
          strcat(metername, "HomeWizard");
        } else {
          strcat(metername, "no device");
        }
      }

      if(checkbox_mqttenabled){
        // re-apply possibly changed server/port and force reconnect with new settings
        // (der Reconnect abonniert auch ein evtl. geaendertes Meter-Topic)
        client.setServer(mqtt_server, atoi(mqtt_port));
        client.disconnect();
      }

      request->send_P(200, "text/html", index_html, processor);
    });

    // Firmware-Update-Check/-Installation: Flags setzen, die Arbeit macht loop()
    // (TLS-Download darf nicht im AsyncWebServer-Handler-Kontext laufen)
    server.on("/checkupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
      do_update_check = true;
      request->send(200, "text/plain", "OK");
    });

    server.on("/dofwupdate", HTTP_GET, [] (AsyncWebServerRequest *request) {
      do_fw_update = true;
      request->send(200, "text/plain", "OK");
    });

    AsyncElegantOTA.begin(&server);
    server.onNotFound(notFound);
    server.addHandler(&events);
    server.begin();
    
    rssi = WiFi.RSSI();
   
    if (meter_source == 0) {
      shelly_model = getShellyType(); // get shelly typ, 3em / 3empro
    } else if (meter_source == 1) {
      memset(metername, 0, sizeof(metername));
      strcat(metername, "Tasmota");
    } else if (meter_source == 2) {
      memset(metername, 0, sizeof(metername));
      strcat(metername, "MQTT Meter");
      lastMeterMsg = millis();
    } else if (meter_source == 3) {
      memset(metername, 0, sizeof(metername));
      strcat(metername, "HomeWizard");
    }

    // erster automatischer Update-Check 2 Minuten nach dem Boot, danach taeglich
    lastUpdateCheck = millis() - updateCheckInterval + 120000UL;

    digitalWrite(SERIAL_COMMUNICATION_CONTROL_PIN, RS485_TX_PIN_VALUE); // RS485 Modul -> set board to transmit 
  }

  // end setup()  
}


void loop() {

  if(checkbox_mqttenabled){
    if (!client.connected()) {
      DBG_PRINTLN("lost mqtt connection -> start reconncect");
      reconnect();
    }
    client.loop(); 
  }


  // send current power to SoyoSource
  if ((millis() - lastTimerSoyoSource) > timerSoyoSource) {

    if(checkbox_batschutz == true && output_enabled == false){ // wenn batterie soc < limit dann soyo_power = 0 
      soyo_power = 0;
    }

    new_soyo_power = soyo_power / teiler_output; // Last auf mehrere Soyo's aufteilen
    if(new_soyo_power < 0){
      new_soyo_power = 0;
    }
   
    //sendSoyoPowerData(soyo_power);
    sendSoyoPowerData(new_soyo_power);
    
    if(new_soyo_power != old_soyo_power) {  // nur für Debug, damit nur Laständerungen ausgegeben werden
      old_soyo_power = new_soyo_power;
      sprintf(dbgbuffer,"new soyo_power = %i ( %02X %02X %02X %02X %02X %02X %02X %02X )",new_soyo_power, soyo_power_data[0],soyo_power_data[1],soyo_power_data[2],soyo_power_data[3],soyo_power_data[4],soyo_power_data[5],soyo_power_data[6],soyo_power_data[7]);
      DBG_PRINTLN(dbgbuffer);
    }

    if(checkbox_mqttenabled){
      sprintf(msgData, "%d", soyo_power);
      client.publish(topic_power, msgData);
    }

    lastTimerSoyoSource = millis();
  }


  // publish Home Assistant state topics
  if(checkbox_mqttenabled && client.connected() && (millis() - lastMqttState) > timerMqttState){
    client.publish(topic_meterpower, String(meterpower).c_str());
    client.publish(topic_uptime, uptime_str);
    client.publish(topic_wifi, String(dBmtoPercent(WiFi.RSSI())).c_str());
    client.publish(topic_null_state, checkbox_nulleinspeisung ? "ON" : "OFF", true);
    client.publish(topic_timer1_state, checkbox_timer1 ? "ON" : "OFF", true);
    client.publish(topic_timer2_state, checkbox_timer2 ? "ON" : "OFF", true);
    client.publish(topic_batt_state, checkbox_batschutz ? "ON" : "OFF", true);
    client.publish(topic_teiler, String(teiler_output).c_str());
    client.publish(topic_maxwatt, String(maxwatt).c_str());
    client.publish(topic_nulloffset, String(nulloffset).c_str());

    lastMqttState = millis();
  }


  // firmware update: taeglicher Check + manuell vom Webinterface angefordert
  if (WiFi.status() == WL_CONNECTED) {
    if (do_update_check || (millis() - lastUpdateCheck) > updateCheckInterval) {
      do_update_check = false;
      lastUpdateCheck = millis();
      checkFwUpdate();
    }
    if (do_fw_update) {
      do_fw_update = false;
      doFwUpdate();
    }
  }


  // timer to get meter data (Shelly / Tasmota / MQTT-Staleness)
  if ((millis() - lastMeterinterval) > meterinterval) {
    if (meter_source == 0) {            // Shelly (HTTP)
      if (shelly_model > 0){
        meter_power = getMeterData(shelly_model);
      } else{
        shelly_model = getShellyType();
        DBG_PRINTLN("Kein Shelly erkannt! Bitte IP eintragen und speichern.");
      }
    } else if (meter_source == 1) {     // Tasmota (HTTP)
      meter_power = getTasmotaData();
    } else if (meter_source == 3) {     // HomeWizard (HTTP)
      meter_power = getHomeWizardData();
    } else {                            // MQTT: Werte kommen per Callback, hier nur Ausfall-Ueberwachung
      if ((millis() - lastMeterMsg) > meterTimeout) {
        meter_power = 0;
        meterpower = 0;
        memset(metername, 0, sizeof(metername));
        strcat(metername, "MQTT Meter offline");
      }
    }

    lastMeterinterval = millis();
  }

  
  // timer to manage Nulleinspeisung
  if ((millis() - lastNullinterval) > nullinterval) { 
    if(checkbox_nulleinspeisung && output_enabled){        
      if(meter_power > nulloffset + 10){  
        soyo_power += meter_power - nulloffset; 

        if(soyo_power > maxwatt){
          soyo_power = maxwatt;
        } 
      } 
  
      if(meter_power < 0 + nulloffset ){
        soyo_power += meter_power - nulloffset; 
       
        if(soyo_power < 0){
          soyo_power = 0;
        }
      }

    }
    lastNullinterval = millis();
  }


  // timer für uptime, SoyoSource Timer und BatSOCLimit
  if ((millis() - lastTimerUptime) > timerUptime) {
    myUptime();

    if(checkbox_timer1 || checkbox_timer2){
      checkTimer();
    }

    // check ob Batterie SOC < oder > eingestelltem Limit
    float mqttbatsoc_float = mqtt_bat_soc + 0.5;
    int mqttbatsoc_int = (int)mqttbatsoc_float;
       
    if(checkbox_batschutz == true && mqttbatsoc_int > 1){ // falls mqtt noch nicht verbunden oder nicht aktiv
      if(mqttbatsoc_int <= batsocstop){
        output_enabled = false;
      }else if(mqttbatsoc_int >= batsocstart){
        output_enabled = true;
      }
    }
    
    lastTimerUptime = millis();
  }


}



