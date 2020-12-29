#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// WiFi configuration
const char* ssid = "your_ssid_here"; // WiFi SSID
const char* password = "your_wifi_password_here"; // WiFi password

// Using a static IP address to avoid using DHCP will save battery
const bool useDHCP = false; // use DHCP (true/false)
IPAddress ip(192, 168, 0, 100); // WiFi Client IP when useDHCP == false
IPAddress gateway(192, 168, 0, 1); // Statis client gateway when useDHCP == false
IPAddress subnet(255, 255, 255, 0); // Static client subnet when useDHCP == false

// MQTT configuration
const char* mqttServer = "192.168.0.50"; // MQTT broker IP
const char* mqttUser = "your_mqtt_username"; // MQTT username
const char* mqttPassword = "your_mqtt_password"; // MQTT password
const int mqttPort = 1883; // MQTT port (default: 1883)

// Retry configuration
const int retryWaitTime = 30; // Retry wait time in minutes
const int retryMaxTimes = 5; // Maximum retry times

int wakePort;
RTC_DATA_ATTR int retryCount = 0;
RTC_DATA_ATTR int retryPort = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting up...");
    Serial.println();
    
    int wakeupStatus = esp_sleep_get_ext1_wakeup_status();

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    Serial.println("wakeupStatus: " + String(wakeupStatus));
    Serial.println("retryPort: " + String(retryPort));
    Serial.println("retryCount: " + String(retryCount));

    if (wakeupStatus == 0 && retryPort != 0) {
        wakePort = retryPort;
        Serial.println ("Retry previously triggered wakePort: " + String(wakePort));
    } else {
        if (wakeupStatus != 0) {
          wakePort = log(wakeupStatus)/log(2);
        } else {
          if (digitalRead(25)) {
            wakePort = 25;
          } else if (digitalRead(26)) {
            wakePort = 26;
          }
        }
        if (wakePort != 0) Serial.println ("Wakeup triggered by port: " + String(wakePort));
    }

    if (wakePort == 25 || wakePort == 26) {
        retryPort = wakePort;
        Serial.print("Connecting to " + String(ssid));

        // Setup WiFi
        WiFiClient espClient;
        if (!useDHCP) WiFi.config(ip, gateway, subnet);
        WiFi.begin(ssid, password);
 
        int trycount = 0;
        // try to connect to WiFi for another 15 seconds
        while (WiFi.status() != WL_CONNECTED && trycount <= 15) {
            delay(500);
            Serial.print(".");
            trycount++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());

            Serial.println("Connecting to MQTT...");
            PubSubClient client(espClient);
            client.setServer(mqttServer, mqttPort);

            if (client.connect("Mailbox", mqttUser, mqttPassword)) {
                if (wakePort == 25) {
                    for (int i = 0; i < 3; i++) {
                      client.publish("mailbox/action", "arrived");
                      delay(100);
                    }
                    Serial.println("A letter arrived, arming door");
                    esp_sleep_enable_ext1_wakeup(GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
                    gpio_pulldown_en(GPIO_NUM_26);
                } else {
                    for (int i = 0; i < 3; i++) {
                      client.publish("mailbox/action", "emptied");
                      delay(100);
                    }
                    Serial.println("Mailbox emptied, arming flap");
                    esp_sleep_enable_ext1_wakeup(GPIO_SEL_25, ESP_EXT1_WAKEUP_ANY_HIGH);
                    gpio_pulldown_en(GPIO_NUM_25);
                }
                resetRetry();
            }
            else {
                Serial.println("Unable to connect to MQTT server");
                retry();
            }

            // disconnect from MQTT
            client.disconnect();
        }
        else {
            Serial.println("Unable to connect to WiFi network");
            retry();
        }

        // disconnect WiFi
        WiFi.disconnect();
    } else {
        gpio_pulldown_en(GPIO_NUM_25);
        gpio_pulldown_en(GPIO_NUM_26);
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_25 | GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
    }

    // Go to deep sleep
    Serial.println("Going to sleep now...");

    esp_deep_sleep_start();
    Serial.println("This will never be printed");
}

void retry()
{
    if (retryCount < retryMaxTimes) {
        Serial.println("An error has occurred, let's try again later");
        ++retryCount;
        esp_sleep_enable_timer_wakeup(retryWaitTime * 60000000);
        gpio_pulldown_en(GPIO_NUM_25);
        gpio_pulldown_en(GPIO_NUM_26);
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_25 | GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
    }
    else {
        Serial.println("Retry failed again. I give up, sorry.");
        resetRetry();
    }
}

void resetRetry()
{
    Serial.println("Resetting retry data");
    // reset any retry counts and states
    retryCount = 0;
    retryPort = 0;
}

void loop()
{
    // This is not going to be called
}
