#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>


// Replace the next variables with your SSID/Password combination
const char* ssid = "your_wlan_name_goes_here";
const char* password = "wlan_password_goes_here";
const char* mqttServer = "mqtt_server_ip_goes_here";
const int mqttPort = 1883;
const char* mqttUser = "homeassistants_mqtt_username_goes_here";
const char* mqttPassword = "homeassistans_mqtt_password_goes_here";
int var;
WiFiClient espClient;
PubSubClient client(espClient);

// use static ip to avoid DHCP wait time
IPAddress ip(192, 168, 254, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet (255, 255, 255, 0);

void setup(){
  Serial.begin(115200);
  delay(100); //Take some time to open up the Serial Monitor

  //get wakeup bit
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  //if it didnt woke because of flap or door, we dont need to do anything.
  if(esp_sleep_get_wakeup_cause() == 2){// 2 is RTC_CTRL - the IOs controlled by rtc
  				//Setup WiFi
  				Serial.print("Connecting to ");
  				Serial.print(ssid);

          WiFi.config(ip, gateway, subnet);
					WiFi.begin(ssid, password);
        
          var = 0;
					while (WiFi.status() != WL_CONNECTED && var <= 25) {
              delay(1000);
              Serial.print(".");
						  var++;
          }
          Serial.println();

					if (WiFi.status() != WL_CONNECTED) {
    		  		Serial.println("Unable to connect to WiFi, going back to sleep");
      				esp_deep_sleep_start();
    			}
		
					Serial.println("WiFi connected");
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
        
          Serial.println("Connecting to MQTT...");
					client.setServer(mqttServer, mqttPort);
         
		 			var = 0;
          while (!client.connected() && var < 3) {
						if (client.connect("paesslersMailbox", mqttUser, mqttPassword )) {  
							Serial.println("MQTT connected");
         		} else {
         			Serial.print("Failed with state ");
              Serial.println(client.state());
              delay(2000);
            }
						var++;
					}
        
          if (wakeupBit & GPIO_SEL_25) {
            // GPIO 25 woke up
            Serial.println("a letter arrived");
            client.publish("mailbox/action", "arrived");
          } 
          else{
            // must be GPIO 26 - we can't as explicitly because with unarmed flap this doesnt work.
            Serial.println("emptied mailbox");
            client.publish("mailbox/action", "emptied");
          }
					// disconnect WiFi
					WiFi.disconnect();
  }

  //we need the internal pullups/downs to work so wee need an option to keep the power for them on during sleep.
  //note: when you use external pulldowns (10K) instead of the internal ones you can save around 10µA in deepsleep  (then comment out the next 5 lines)
  //note2: don't use gpio34-39 when using internal pullup/downs - they don't have them.
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  gpio_pullup_dis(GPIO_NUM_25);
  gpio_pulldown_en(GPIO_NUM_25);
  gpio_pullup_dis(GPIO_NUM_26);
  gpio_pulldown_en(GPIO_NUM_26);

  /*
   * Important: Assume someone threw a magazine into our Mailbox, blocking the flap. This would cause it to
   * permanently wake up and drain our battery. So we'll only arm the mailbox door switch after a letter
   * arrived.
   */
  
  if(wakeupBit & GPIO_SEL_25){
  //enable gpio 25 and 26 for deep sleep (using bit or) - wakeup if any of those go high.
    Serial.println("only arming door");
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_26,ESP_EXT1_WAKEUP_ANY_HIGH);
  }
  else{
    //arm both flap and door.
    Serial.println("arming flap");
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_25,ESP_EXT1_WAKEUP_ANY_HIGH);
  }
  //Go to sleep now
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}


void loop(){
  //This is not going to be called
}
