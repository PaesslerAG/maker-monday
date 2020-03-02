#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Wire.h>
#include <MPU9250.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <secrets.h> 
#include <SPI.h>
#include <PubSubClient.h>
/*
HEADS UP!
Provide a secrets.h in this same folder with the following content:

#define WIFI1_SSID "ssid"
#define WIFI1_PASS "password"
#define MQTT_SERVER "mqtt.local"
#define MQTT_USER "mqttuser"
#define MQTT_PASS "mqttpassword"
#define CARNAME "camaro"

*/

//mqtt
const char* mqtt_server = MQTT_SERVER;

//websocket stuff
WiFiMulti wifiMulti;
WebSocketsServer webSocket = WebSocketsServer(8000);

WiFiClient espClient;
PubSubClient mqttclient(espClient);

// How many leds in your strip?
#define NUM_LEDS 6
#define LED_FRONT_RIGHT 0
#define LED_FRONT_LEFT 1
#define LED_REAR_LEFT 2
#define LED_REAR_RIGHT 3
#define LED_SIREN_RIGHT 4
#define LED_SIREN_LEFT 5
#define LED_DATA_PIN 13


// Define the array of leds
CRGB leds[NUM_LEDS];
CRGB cur_right = CRGB::Black;
CRGB cur_left = CRGB::Black;
      
uint8_t ledcount = 0 ;
bool flashlight = false;

//json document
const size_t capacity = JSON_OBJECT_SIZE(2); //https://arduinojson.org/v6/assistant/
DynamicJsonDocument doc(capacity);
char jsonbuffer[100];

// an MPU9250 object with the MPU-9250 sensor on I2C bus 0 with address 0x68
MPU9250 IMU(Wire,0x68);
float cor_x;
float cor_y;

int status;

//let the siren blink :)
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
  if( cur == target) return;
  
  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
}

CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
{
  nblendU8TowardU8( cur.red,   target.red,   amount);
  nblendU8TowardU8( cur.green, target.green, amount);
  nblendU8TowardU8( cur.blue,  target.blue,  amount);
  return cur;
}

//websocket stuff

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

				// send message to client
				webSocket.sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:


            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
    }

}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Callback! Topic:  ");
  Serial.print(String(topic));
  Serial.print("\n");

  if ( String(topic) == String(CARNAME) + "/flashlights" ) {
    char* p = (char*)malloc(length + 1);
    p[length] = 0;

    // Copy the payload to the new buffer
    memcpy(p, payload, length);

    String flashlightstr(p);

    Serial.print("Payload: ");
    Serial.print(flashlightstr);
    Serial.print("\n");
    if ( flashlightstr == "on" ) {
      flashlight = true;
    } else if ( flashlightstr == "off" ) {
      flashlight = false;
      leds[LED_SIREN_LEFT] = CRGB::Black;
      leds[LED_SIREN_RIGHT] = CRGB::Black;
      FastLED.show();
    }
    free(p);
  } else if ( String(topic) == String(CARNAME)+"/lights" ) {
    char* p = (char*)malloc(length + 1);
    p[length] = 0;

    // Copy the payload to the new buffer
    memcpy(p, payload, length);

    String lightsstr(p);

    Serial.print("Color Payload: ");
    Serial.print(lightsstr);
    Serial.print("\n");
    if ( lightsstr == "on" ) {
      leds[LED_FRONT_RIGHT] = CRGB::White;
      leds[LED_FRONT_LEFT] = CRGB::White;
      leds[LED_REAR_LEFT] = CRGB::Red;
      leds[LED_REAR_RIGHT] = CRGB::Red;
      FastLED.show();
    } else if ( lightsstr == "off" ) {
      leds[LED_FRONT_RIGHT] = CRGB::Black;
      leds[LED_FRONT_LEFT] = CRGB::Black;
      leds[LED_REAR_LEFT] = CRGB::Black;
      leds[LED_REAR_RIGHT] = CRGB::Black;
      FastLED.show();
    }
    else if ( lightsstr == "red" ) {
      leds[LED_FRONT_RIGHT] = CRGB::DarkRed;
      leds[LED_FRONT_LEFT] = CRGB::DarkRed;
      leds[LED_REAR_LEFT] = CRGB::Black;
      leds[LED_REAR_RIGHT] = CRGB::Black;
      FastLED.show();
    }

    free(p);
  } 
  mqttclient.publish((String(CARNAME)+"/status").c_str(), "idling");
}


void reconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID

    // Attempt to connect
    if (mqttclient.connect(CARNAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttclient.publish((String(CARNAME) + "/connected").c_str(), "1", true);
      // ... and resubscribe
      mqttclient.subscribe( ( String(CARNAME) + "/flashlights").c_str() );
      mqttclient.subscribe( (String(CARNAME) + "/lights" ).c_str() );
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() { 

  	  FastLED.addLeds<WS2812, LED_DATA_PIN, RGB>(leds, NUM_LEDS);
      Wire.begin();
      leds[LED_SIREN_LEFT] = CRGB::Green;
      leds[LED_SIREN_RIGHT] = CRGB::Green;
      FastLED.show();
      Serial.begin(9600);
      while (!Serial);             // Leonardo: wait for serial monitor
      //Connect to WIFI
          
      wifiMulti.addAP(WIFI1_SSID, WIFI1_PASS);
      // wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
      // wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

      Serial.println("Connecting ...");
      int i = 0;
      while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
        delay(1000);
        Serial.print('.');
      }
      leds[LED_SIREN_LEFT] = CRGB::Yellow;
      leds[LED_SIREN_RIGHT] = CRGB::Yellow;
      FastLED.show();
      Serial.println('\n');
      Serial.print("Connected to ");
      Serial.println(WiFi.SSID());              // Tell us what network we're connected to
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
      // add mDns Responder
      if (!MDNS.begin("camaro")) {
        Serial.println("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
      }
      Serial.println("mDNS responder started");
      leds[LED_SIREN_LEFT] = CRGB::Purple;
      leds[LED_SIREN_RIGHT] = CRGB::Purple;
      FastLED.show();
      // start communication with IMU 
      status = IMU.begin();
      if (status < 0) {
        Serial.println("IMU initialization unsuccessful");
        Serial.println("Check IMU wiring or try cycling power");
        Serial.print("Status: ");
        Serial.println(status);
        while(1) {}
      }

      //initialize LEDS
      leds[LED_FRONT_RIGHT] = CRGB::White;
      leds[LED_FRONT_LEFT] = CRGB::White;
      leds[LED_REAR_LEFT] = CRGB::Red;
      leds[LED_REAR_RIGHT] = CRGB::Red;



      //start WebSocket Server
      webSocket.begin();
      webSocket.onEvent(webSocketEvent);
      leds[LED_SIREN_LEFT] = CRGB::Orange;
      leds[LED_SIREN_RIGHT] = CRGB::Orange;
      FastLED.show();      
      // mqtt
      mqttclient.setServer(mqtt_server, 1883);
      mqttclient.setCallback(callback);
      leds[LED_SIREN_LEFT] = CRGB::Black;
      leds[LED_SIREN_RIGHT] = CRGB::Black;
      FastLED.show();
      // setting the accelerometer full scale range to +/-8G 
      IMU.setAccelRange(MPU9250::ACCEL_RANGE_2G);
      // setting the gyroscope full scale range to +/-500 deg/s
      IMU.setGyroRange(MPU9250::GYRO_RANGE_500DPS);
      // setting DLPF bandwidth to 20 Hz
      IMU.setDlpfBandwidth(MPU9250::DLPF_BANDWIDTH_20HZ);
      // setting SRD to 19 for a 50 Hz update rate
      IMU.setSrd(19);   
      //status = IMU.calibrateAccel();  
      cor_x = IMU.getAccelY_mss();
      cor_y = IMU.getAccelX_mss();
      // Do IMU Calibration. This can be commented out if done one time
      // (Will remain in IMUs flash after setAccel...)
      // Serial.print("IMU Calibration Status: ");
      // Serial.println(status);
      // float axb;
      // axb = IMU.getAccelBiasX_mss();
      // Serial.print(axb);
      // Serial.println(" - axb\t");
 
      // float axs;
      // Serial.println(status);
      // axs = IMU.getAccelScaleFactorX();
      // float ayb;
      // Serial.println(status);
      // ayb = IMU.getAccelBiasY_mss();
      // float ays;
      // Serial.println(status);
      // ays = IMU.getAccelScaleFactorY();
      // float azb;
      // Serial.println(status);
      // azb = IMU.getAccelBiasZ_mss();
      // float azs;
      // Serial.println(status);
      // azs = IMU.getAccelScaleFactorZ();
      // IMU.setAccelCalX(axb, axs);
      // IMU.setAccelCalY(ayb,ays);
      // IMU.setAccelCalZ(azb,azs);
}

void loop() {
  EVERY_N_SECONDS(10){
    mqttclient.publish((String(CARNAME) + "/temperature").c_str(), String(IMU.getTemperature_C()).c_str(), true);
  }
  EVERY_N_MILLISECONDS(100)
  {
    if(flashlight){
      if(ledcount < 8){
        if ( (ledcount & 0x01) == 0) {
          leds[LED_SIREN_LEFT] = CRGB::Red;
        }
        else{
          leds[LED_SIREN_LEFT] = CRGB::Black;
        }
        ledcount++;
        FastLED.show();
      }
      else{
        if ( (ledcount & 0x01) == 0) {
          leds[LED_SIREN_RIGHT] = CRGB::Blue;
        }
        else{
          leds[LED_SIREN_RIGHT] = CRGB::Black;
        }
        ledcount++;
        FastLED.show();
      }
      if(ledcount == 16){
        ledcount =  0;
      }
    }


    // read the sensor
    IMU.readSensor();
    // YES, the following lines mix X and Y,
    // this is because the sensor is flipped in both directions.
    doc["x"] = ( IMU.getAccelY_mss() - cor_y );
    doc["y"] = ( IMU.getAccelX_mss() - cor_x ) * -1;
    // display the data
    // Serial.print(IMU.getAccelX_mss(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getAccelY_mss(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getAccelZ_mss(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getGyroX_rads(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getGyroY_rads(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getGyroZ_rads(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getMagX_uT(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getMagY_uT(),6);
    // Serial.print("\t");
    // Serial.print(IMU.getMagZ_uT(),6);
    // Serial.print("\t");
    // Serial.println(IMU.getTemperature_C(),6);
    //serializeJson(doc, Serial);
    //Serial.print("\t");
    serializeJson(doc, jsonbuffer);
    webSocket.broadcastTXT(jsonbuffer);
  }
  // EVERY_N_MILLISECONDS(1500)
  // {
  //   webSocket.broadcastTXT("Salve!!");

  //   // Serial.println("Connected Clients: ");
  //   // Serial.print(webSocket.connectedClients());
  //   // Serial.print("\t");
  // }
  webSocket.loop();

  //mqtt
  if (!mqttclient.connected()) {
    reconnect();
  }
  mqttclient.loop();
}