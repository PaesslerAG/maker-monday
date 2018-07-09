#include <Adafruit_BME280.h> //gepatchte version!   https://github.com/Takatsuki0204/BME280-I2C-ESP32
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// for voltage measurement
extern "C" int rom_phy_get_vdd33();

// Wifi settings
const char* ssid = "ssid";                          //replace this with your WiFi network name
const char* password = "wifipassword";      //replace this with your WiFi network password

//define pins and address for BME
#define SDA_PIN 27
#define SCL_PIN 25
#define BME280_ADDRESS 0x76

// deepsleep factor
#define uS_TO_S_FACTOR 1000000        // Conversion factor for micro seconds to seconds //
#define TIME_TO_SLEEP 600             // Time ESP32 will go to sleep (in seconds) //

// the MQTT stuff
const char* mqtt_server = "hassbian";
const char* mqtt_username = "username";
const char* mqtt_password = "password";
const char* clientID = "blclimate";
const char* cSensorName = "office/temperature";
char msg[100];

// define the clients
Adafruit_BME280 bme(SDA_PIN, SCL_PIN);
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// the json stuff
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();

void setup_wifi() {
  //delay(100);

  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting");

  //define loop counter
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    
    if(i >= 20) {
      //Wifi failed. Going to sleep until next power cycle.
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      Serial.println("Wifi failed, going to sleep now");
      esp_deep_sleep_start();
    }
    i++;
  }

  Serial.println("success!");
  Serial.print("IP Address is: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  Serial.begin(115200);
  delay(100);

  //setup Wifi
  setup_wifi();
  //setup mqtt
  client.setServer(mqtt_server, 1883);

  // starte BME280
  if (!bme.begin(BME280_ADDRESS)) {
    Serial.println("sensor failed!");
  }

  if (!client.connected()) {
    reconnect();
  }
    
  root["temperature"] = bme.readTemperature();
  root["humidity"] = bme.readHumidity();
  root["pressure"] = bme.readPressure() / 100;
  root["voltage"] = rom_phy_get_vdd33();
  root.printTo(msg);

  client.publish((String(cSensorName) + "/data").c_str(), msg, false);

  //short delay to transmit the message
  delay(500);

  // go sleeping
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}


void loop() {
  // this loop will  never do anything
}