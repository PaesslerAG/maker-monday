#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#define NUM_LEDS 14
#define LED_TYPE WS2812B
#define DATA_PIN 13
#define COLOR_ORDER GRB

const char* ssid = "CHANGE_SSID"; 
const char* password = "CHANGE_WIFI_PASSWORD";

const char* mqtt_server = "CHANGE_MQTT_SERVER";
const char* mqtt_user = "CHANGE_MQTT_USER";
const char* mqtt_password = "CHANGE_MQTT_PASSWORD";
const char* clientID = "carreraampel";
const char* inTopic = "carrera/lights/set";
const char* outTopic = "carrera/lights/data";

char msg[200];
int brightness = 255;

// Lap counter struct
struct LapCount {
  const uint8_t PIN;
  uint32_t numberRounds;
  bool detected;
};

// Define 2 Lanes for 2 Cars
LapCount LapA {18, 0, false};
LapCount LapB {19, 0, false};

//define some clients
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Define ArduinoJson Objects
StaticJsonDocument<256> json;
DynamicJsonDocument doc(256);
JsonObject prtg = doc.createNestedObject("prtg");
JsonArray prtg_result = prtg.createNestedArray("result");
JsonObject prtg_result_0 = prtg_result.createNestedObject();
JsonObject prtg_result_1 = prtg_result.createNestedObject();

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup_wifi() {
  delay(500);
 
  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("success!");
  Serial.print("IP Address is: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(clientID, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqtt.publish(outTopic, clientID);
      // ... and resubscribe
      mqtt.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  // Setup MQTT
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);
  
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  red();

  pinMode (LapA.PIN, INPUT) ;
//  attachInterrupt(LapA.PIN, isr_a, FALLING);

  pinMode (LapB.PIN, INPUT) ;
//  attachInterrupt(LapB.PIN, isr_b, FALLING);
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null terminator used to terminate the char array

  Serial.print("Message arrived on topic: [");
  Serial.print(topic);
  Serial.print("], ");
  Serial.print((char*)payload);

  // Manage callbacks
  if(strcmp ((char*)payload, "red") == 0)   { red();    }
  if(strcmp ((char*)payload, "green") == 0) { green();  }
  if(strcmp ((char*)payload, "race") == 0)  { race();   }
  if(strcmp ((char*)payload, "reset") == 0) { reset();  }
  
  if(sscanf((char*)payload, "brightness %d", &brightness) == 1 ) { FastLED.setBrightness(brightness); }
}

void red() {
    for(int dot = 0; dot < NUM_LEDS; dot++) { 
        leds[dot] = CRGB::Red;
    }
    FastLED.show();
}

void green() {
    for(int dot = 0; dot < NUM_LEDS; dot++) { 
        leds[dot] = CRGB::Green;
    }
    FastLED.show();
}

void race() {
    int timer = 300;
    
    // all LED off
    fill_solid(leds,NUM_LEDS,0);
    FastLED.show();

    // Start sequence. Fill LED from left to right. 
    for(int dot = 0; dot < (NUM_LEDS / 2); dot++) {
      int dotreverse = (NUM_LEDS -1 ) - dot;
      leds[dot] = CRGB::Red;
      leds[dotreverse] = CRGB::Red;
      FastLED.show();
      delay(timer);
    }
    
    // Fill all LED Green
    for(int dot = 0; dot < NUM_LEDS; dot++) { 
        leds[dot] = CRGB::Green;
    }
    
    FastLED.show();
    delay(1000);
    
    //pinMode (LapA.PIN, INPUT) ;
    attachInterrupt(LapA.PIN, isr_a, FALLING);

    //pinMode (LapB.PIN, INPUT) ;
    attachInterrupt(LapB.PIN, isr_b, FALLING);
}

void reset() {
  // Reset red lights and lap counter to 0.
  red();
  LapA.numberRounds = 0;
  LapB.numberRounds = 0;
  // publish reset
  publish_mqtt();
  //publish_http();
  detachInterrupt(LapA.PIN);
  detachInterrupt(LapB.PIN);
}

void publish_mqtt() {
  json["LapA"] = LapA.numberRounds;
  json["LapB"] = LapB.numberRounds;

  serializeJson(json, msg);

  mqtt.publish(outTopic, msg, false);
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  if(LapA.detected) {
    LapA.numberRounds +=1;
    Serial.println("A:");
    Serial.print(LapA.numberRounds);
    LapA.detected = false;
    publish_mqtt();    
  }
  if(LapB.detected) {
    LapB.numberRounds +=1;
    Serial.println("B:");
    Serial.print(LapB.numberRounds);
    LapB.detected = false;
    publish_mqtt();
  }
  
  if((LapA.numberRounds > 0) || (LapB.numberRounds > 0)) {
    fill_solid(leds,NUM_LEDS,0);

    for(int dot = 0; dot < LapA.numberRounds; dot++) {
      leds[dot] = CRGB::Blue;      
    }

    for(int dot = 7; dot < (LapB.numberRounds + 7); dot++) {
      leds[dot] = CRGB::Yellow;      
    }
  }

  FastLED.show();

  //slow down the loop
  delay(500);
}

void IRAM_ATTR isr_a() {
  LapA.detected = true;
}

void IRAM_ATTR isr_b() {
  LapB.detected = true;
}