/*******************************************************************
 *  Read YouTube Channel statistics from the YouTube API           *
 *  This sketch uses the WiFiManager Library for configuraiton     *
 *  Using DoubleResetDetector to launch config mode                *
 *                                                                 *
 *  By Brian Lough                                                 *
 *  https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA       *
 *******************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>


#include <DoubleResetDetector.h>
// For entering Config mode by pressing reset twice
// Available on the library manager (DoubleResetDetector)
// https://github.com/datacute/DoubleResetDetector

#include <ArduinoJson.h>
// Required for the YouTubeApi and used for the config file
// Available on the library manager (ArduinoJson)
// https://github.com/bblanchon/ArduinoJson

#include <WiFiManager.h>
// For configuring the Wifi credentials without re-programing
// Availalbe on library manager (WiFiManager)
// https://github.com/tzapu/WiFiManager

// For storing configurations
#include "FS.h"

// Additional libraries needed by WiFiManager
#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include "DHT.h"
#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)


char apiKey[42] = "";
char channelId[42] = "";


WiFiClient client;

unsigned long api_mtbs = 60000; //mean time between api requests
unsigned long api_lasttime;   //last time api request has been done

long subs = 0;

// flag for saving data
bool shouldSaveConfig = false;

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
// This sketch uses drd.stop() rather than relying on the timeout
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0
#define CONFIG_FILE_NAME "/apconfig.json"
#define AP_NAME "WEMOS_42eb"
#define AP_PASSWORD "password"


unsigned long myChannelNumber = 953239;
const char * myWriteAPIKey = "sdfsdfsdf";

const char* host = "api.iotguru.cloud";

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
DHT dht(DHTPIN, DHTTYPE);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  // You could indicate on your screen or by an LED you are in config mode here

  // We don't want the next time the boar resets to be considered a double reset
  // so we remove the flag
  drd.stop();
}

void setup() {

  Serial.begin(115200);
  dht.begin();

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount FS");
    return;
  }

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  loadConfig();

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Adding an additional config on the WIFI manager webpage for the API Key and Channel ID
  WiFiManagerParameter customChannelId("channelId", "channelId", channelId, 42);
  WiFiManagerParameter customApiKey("apiKey", "API key", apiKey, 42);

  wifiManager.addParameter(&customChannelId);
  wifiManager.addParameter(&customApiKey);

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    forceConfigMode();
    //wifiManager.resetSettings();
    //wifiManager.startConfigPortal(AP_NAME, AP_PASSWORD);
  } else {
    Serial.println("No Double Reset Detected");
    wifiManager.autoConnect(AP_NAME, AP_PASSWORD);
  }

  strcpy(apiKey, customApiKey.getValue());
  //strcpy(channelId, customChannelId.getValue());
  

  if (shouldSaveConfig) {
    saveConfig();
  }

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  // Force Config mode if there is no API key
  if(strcmp(apiKey, "") > 0) {
    Serial.println("Init weather api");
    //api = new YoutubeApi(apiKey, client);
  } else {
    Serial.println("Forcing Config Mode");
    forceConfigMode();
  }
  Serial.println("");
  Serial.println("Weather API key:");
  Serial.println(apiKey);
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  drd.stop();

}

bool loadConfig() {
  File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  strcpy(apiKey, json["apiKey"]);
  //strcpy(channelId, json["channelId"]);
  Serial.println(apiKey);
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["apiKey"] = apiKey;
  json["channelId"] = channelId;

  File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void forceConfigMode() {
  Serial.println("Reset");
  WiFi.disconnect();
  Serial.println("Dq");
  delay(500);
  ESP.restart();
  delay(5000);
}

void loop() {

  if (millis() - api_lasttime > api_mtbs) {
    float temperature = dht.readTemperature();
    float humidity  = dht.readHumidity();  
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.println(temperature);
    Serial.println("weather updated");
  
  float value = temperature;

// make TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    return;
  }

  String tempUrl = "http://api.iotguru.cloud/measurement/create/sdfsdf/temp/" + String(temperature);
  tempUrl+="\r\n";
  Serial.print(tempUrl);
  // Request to the server
  client.print(String("GET ") + tempUrl + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
delay(200);
// Read all the lines of the reply from server and print them to Serial
Serial.println("receiving from remote server");
// not testing 'client.connected()' since we do not need to send data here

  while (client.available()) {
    char ch = static_cast<char>(client.read());
    Serial.print(ch);
  }

  String humidityUrl = "http://api.iotguru.cloud/measurement/create/sdfsdf/humidity/" + String(humidity);
  humidityUrl+="\r\n";
  Serial.print(humidityUrl);
  // Request to the server
  client.print(String("GET ") + humidityUrl + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
   delay(200);
// Read all the lines of the reply from server and print them to Serial
Serial.println("receiving from remote server");
// not testing 'client.connected()' since we do not need to send data here

  while (client.available()) {
    char ch = static_cast<char>(client.read());
    Serial.print(ch);
  }
    api_lasttime = millis();
  }
}
