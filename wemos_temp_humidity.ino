/*******************************************************************
 *  Read Temperature and Humidity values from DHT22 sensor         *
 *  This sketch uses the WiFiManager Library for configuraiton     *
 *  Using DoubleResetDetector to launch config mode                *
 *  Using Adafruit DHT library for reading the sensor data         *
 *  Using ThingSpeak to store data to ThingSpeak channel           *
 *                                                                 *
 *  based on sketch made by Brian Lough                            *
 *  https://www.youtube.com/watch?v=l9Gl1yKvMNg                    *
 *******************************************************************/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <DoubleResetDetector.h>
// For entering Config mode by pressing reset twice
// Available on the library manager (DoubleResetDetector)
// used v 1.0.2
// https://github.com/datacute/DoubleResetDetector

#include <ArduinoJson.h>
// Required for the YouTubeApi and used for the config file
// Available on the library manager (ArduinoJson)
// used v 6.13.0
// https://github.com/bblanchon/ArduinoJson

#include <WiFiManager.h>
// For configuring the Wifi credentials without re-programing
// Availalbe on library manager (WiFiManager)
// used v 0.14.0
// https://github.com/tzapu/WiFiManager

// For storing configurations
#include "FS.h"

// Additional libraries needed by WiFiManager
#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include "DHT.h"

#include "ThingSpeak.h"
// for uploading sensor data to ThingSpeak
// used v 1.5.0

#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)


char apiKey[42] = "";
char channelId[42] = "";
unsigned long channelNumber = 333553;

unsigned long api_mtbs = 60000; //mean time between api requests
unsigned long api_lasttime;   //last time api request has been done

// flag for saving data
bool shouldSaveConfig = false;

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
// This sketch uses drd.stop() rather than relying on the timeout
#define DRD_TIMEOUT 5

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0
#define CONFIG_FILE_NAME "/apconfig.json"
#define AP_NAME_PREFIX "WEMOS_"
#define AP_PASSWORD "password"


WiFiClient client;
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
  wifiManager.setDebugOutput(true);


  // Adding an additional config on the WIFI manager webpage for the API Key and Channel ID
  WiFiManagerParameter customChannelId("channelId", "channelId", channelId, 42);
  WiFiManagerParameter customApiKey("apiKey", "API key", apiKey, 42);

  wifiManager.addParameter(&customChannelId);
  wifiManager.addParameter(&customApiKey);

  String ssid = AP_NAME_PREFIX + String(ESP.getChipId());
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    /*
    WifiManager library has an issue  - startConfigPortal function hangs from time to time.
    to avoid such behaviour the following workaround is used:
    1) call wifiManager.setBreakAfterConfig(true);
    2) perform the ESP.restart(); if wifiManager.startConfigPortal returns false
    drawback of the workaround is that in case password is incorrectly set for SSID, 
    it will be saved and the device will enter into the config mode.
    */
    wifiManager.setBreakAfterConfig(true); //workaround for hanging startConfigPortal 
    if (!wifiManager.startConfigPortal(ssid.c_str(), AP_PASSWORD)) {
      Serial.println("Failed to connect"); 
      ESP.restart(); ////workaround for hanging startConfigPortal
    }
  } else {
    Serial.println("No Double Reset Detected");
    if (wifiManager.autoConnect(ssid.c_str(), AP_PASSWORD)) {
      Serial.println("connected to wifi");
    }
  }

  strcpy(apiKey, customApiKey.getValue());
  strcpy(channelId, customChannelId.getValue());
  
  if (shouldSaveConfig) {
    saveConfig();
  }

  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  // Force Config mode if there is no API key
  if(strcmp(apiKey, "") > 0) {
    Serial.println("Init ThingSpeak");
    ThingSpeak.begin(client);  // Initialize ThingSpeak
  } else {
    Serial.println("Forcing Config Mode");
    forceConfigMode();
  }
  Serial.println("");
  Serial.print("Weather API key:");
  Serial.println(apiKey);

  Serial.print("SSID:");
  Serial.println(WiFi.SSID());
  
  Serial.print("WiFi connected. IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  drd.stop();
  
  dht.begin();
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

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, buf.get());
 
  if (error) {
    Serial.print(F("Failed to parse config file with code "));
    Serial.println(error.c_str());
    return false;
  }

  if (doc.containsKey("apiKey")) {
    strcpy(apiKey, doc["apiKey"]);
  }
  if (doc.containsKey("channelId")) {
    strcpy(channelId, doc["channelId"]);
  }
  Serial.print("api key = ");
  Serial.print(apiKey);
  Serial.print(", channel Id = ");
  Serial.println(channelId);
  return true;
}

bool saveConfig() {
  StaticJsonDocument<200> doc;
  doc["apiKey"] = apiKey;
  doc["channelId"] = channelId;

  File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  serializeJson(doc, configFile);
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

    ThingSpeak.setField(1, temperature);
    ThingSpeak.setField(2, humidity);

    Serial.print("api key = ");
    Serial.print(apiKey);
    Serial.print(", channel Id = ");
    Serial.println(channelId);
  
    int x = ThingSpeak.writeFields(atol(channelId), apiKey);
    if(x == 200) {
      Serial.println("Channel update successful.");
    } else{
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    api_lasttime = millis();
  }
}
