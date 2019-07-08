#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
MDNSResponder mdns;
#define ESP8266_LED 5


// Update these with values suitable for your network.
const char* ssid = "CHANGE-ME";
const char* password = "CHANGE-ME";
const char* mqtt_server = "CHANGE-ME";
const char* mqtt_username = "CHANGE-ME";
const char* mqtt_password = "CHANGE-ME";
const char* DNSNAME = "makermondaybuzzer";

/**************************** FOR OTA **************************************************/
#define SENSORNAME "frontdoor" //change this to whatever you want to call your device
//#define ota_password "Master"
int OTAport = 8266;

WiFiClient espClient;
PubSubClient client(espClient);
int DoorPin = 12;
int LightPin = 13;
String action;
String strTopic;
String strPayload;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char*)topic);
  action = String((char*)payload);
  if(strTopic == "ha/door"){
    if(action == "ON")
      {
        Serial.println("ON");
        digitalWrite(DoorPin, HIGH);
      }
    else if (action == "OFF")
      {
        Serial.println("OFF");
        digitalWrite(DoorPin, LOW);
      }
    }
  else if(strTopic == "ha/hall_light"){
    if(action == "ON")
      {
        Serial.println("ON");
        digitalWrite(LightPin, HIGH);
      }
    else if (action == "OFF")
      {
        Serial.println("OFF");
        digitalWrite(LightPin, LOW);
      }
    }
}

 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    //if (client.connect("arduinoClient")) {
    if (client.connect("ESP8266Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.subscribe("ha/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi(); 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

//OTA SETUP
  ArduinoOTA.setPort(OTAport);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(SENSORNAME);

  // No authentication by default
//  ArduinoOTA.setPassword((const char *)OTApassword);
//ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
   if (mdns.begin((DNSNAME), WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }
  
  pinMode(DoorPin, OUTPUT);
  pinMode(LightPin, OUTPUT);
  pinMode(ESP8266_LED, OUTPUT);
  digitalWrite(ESP8266_LED, HIGH);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
}
