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

char* state;
RTC_DATA_ATTR int retryCount;
RTC_DATA_ATTR char* retryState;

void setup()
{
    Serial.begin(115200);
    delay(100); // Take some time to open up the Serial Monitor
    Serial.println("Starting up...");

    // get wakeup bit
    uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();

    // if it didnt woke because of flap or door or retry timer, we only need to configure internal pullups/downs.
    if (esp_sleep_get_wakeup_cause() == 2 || esp_sleep_get_wakeup_cause() == 3) { // 2 is GPIO, 3 is retry timer
        if (esp_sleep_get_wakeup_cause() == 3) { // retry timer
            Serial.print("Retry processing previous state: ");
            Serial.println(retryState);
            Serial.print("Retry count: ");
            Serial.println(retryCount);
            state = retryState;
        }
        else { // mail arrived or emptied
            if (wakeupBit & GPIO_SEL_25) {
                state = "arrived";
            }
            else {
                state = "emptied";
            }
        }

        Serial.print("Connecting to ");
        Serial.print(ssid);

        // Setup WiFi
        WiFiClient espClient;
        if (!useDHCP)
            WiFi.config(ip, gateway, subnet);
        WiFi.begin(ssid, password);

        int trycount = 0;
        // try to connect to WiFi for another 10 seconds
        while (WiFi.status() != WL_CONNECTED && trycount <= 10) {
            delay(1000);
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

            trycount = 0; // reset trycount
            // try to connect to MQTT server, retry two times if connection fails
            while (!client.connected() && trycount <= 2) {
                if (client.connect("Mailbox", mqttUser, mqttPassword)) {
                    Serial.println("MQTT connected");
                }
                else {
                    Serial.print("Failed with state ");
                    Serial.println(client.state());
                    delay(1000);
                }
                trycount++;
            }

            if (client.connected()) {
                resetRetry();
                if (state == "arrived") {
                    client.publish("mailbox/action", "arrived");
                    Serial.println("A letter arrived, arming door");
                    esp_sleep_enable_ext1_wakeup(GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
                }
                else if (state == "emptied") {
                    client.publish("mailbox/action", "emptied");
                    Serial.println("Mailbox emptied, arming flap");
                    esp_sleep_enable_ext1_wakeup(GPIO_SEL_25, ESP_EXT1_WAKEUP_ANY_HIGH);
                }
            }
            else {
                Serial.println("Unable to connect to MQTT server");
                retry();
            }
        }
        else {
            Serial.println("Unable to connect to WiFi network");
            retry();
        }

        // disconnect WiFi
        WiFi.disconnect();
    }
    else {
        Serial.println("Arming flap and door");
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_25 | GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
    }

    /*
    we need the internal pullups/downs to work so we need an option to keep the power for them on during sleep.
    note: when you use external pulldowns (10K) instead of the internal ones you can save around 10µA in deepsleep  (then comment out the next 5 lines)
    note2: don't use gpio34-39 when using internal pullup/downs - they don't have them.
    */

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    gpio_pullup_dis(GPIO_NUM_25);
    gpio_pulldown_en(GPIO_NUM_25);
    gpio_pullup_dis(GPIO_NUM_26);
    gpio_pulldown_en(GPIO_NUM_26);

    // Go to deep sleep
    Serial.println("Going to sleep now...");
    esp_deep_sleep_start();
}

void retry()
{
    // with or without retry mode enabled, listen to GPIO ports
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_25 | GPIO_SEL_26, ESP_EXT1_WAKEUP_ANY_HIGH);
    if (retryCount < retryMaxTimes) {
        Serial.println("An error has occurred, let's try again later");
        ++retryCount;
        retryState = state;
        esp_sleep_enable_timer_wakeup(retryWaitTime * 60000000);
    }
    else {
        Serial.println("Retry failed again. I give up, sorry.");
        resetRetry();
    }
}

void resetRetry()
{
    // reset any retry counts and states
    retryCount = 0;
    retryState = "cleared";
}

void loop()
{
    // This is not going to be called
}
