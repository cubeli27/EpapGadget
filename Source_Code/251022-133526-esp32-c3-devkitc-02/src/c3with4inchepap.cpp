//board: ESPC3 Dev Module
/* 
--- Pin mapping for ESP32-C3 ---
#define CS_PIN 7 // Chip select - IO7
#define BUSY_PIN 3 // Busy - IO3
#define RES_PIN 2 // Reset - IO2
#define DC_PIN 1 // Data/Command - IO1
#define SCL_PIN 4 // SCK - IO4
#define SDA_PIN 6 // MOSI - IO6
*/
#include <Arduino.h>
void helloWorld();
void goToSleep();
void fetchTelegramMessage();

//DEBUGING STUFF-------------------------------------------
#define DEBUG 1 // 1= enable debug prints, 0=disable debug prints

#if DEBUG == 1
  #define DEBUG_PRINTLN(x)    Serial.println(x)
  #define DEBUG_PRINT(x)      Serial.print(x)
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)
#endif
//---------------------------------------------------------

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <esp_sleep.h>
#include <esp32/rtc.h>
#include <WiFi.h>
#include <private.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


// ESP32-C3 CS(SS)=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RES(RST)=2,DC=1
#define CS_PIN (7)
#define BUSY_PIN (3)
#define RES_PIN (2)
#define DC_PIN (1)

// 2.13'' EPD Module
//GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // DEPG0213BN 122x250, SSD1680
// 4.2'' EPD Module
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // 400x300, SSD1683

// RTC_DATA_ATTR for retaining data during sleep
RTC_DATA_ATTR int counter = 0; //just for testing increments on each wakeup
RTC_DATA_ATTR char message1[100];
RTC_DATA_ATTR char message2[100];
RTC_DATA_ATTR char message3[100];
RTC_DATA_ATTR char telegramMessage1[100] = "No Msg";
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  DEBUG_PRINTLN("initializing/woke up from sleep");
  pinMode(CS_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(RES_PIN, OUTPUT);
  pinMode(BUSY_PIN,OUTPUT);

  display.init(115200,true,70,false); // 70 for the 4inch works
  delay(1500);//giving it some extra time seems to help
  helloWorld();
  goToSleep();
}


void helloWorld()
{

  display.setRotation(0); // 0 rotation for 4inch from connector down 
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  
  fetchTelegramMessage();
  sprintf(message1, "USER1: %s", telegramMessage1);
  sprintf(message2, "USER2: %d", counter + 1);
  sprintf(message3, "USER3: %d", counter + 2);

  int16_t tbx1, tby1; uint16_t tbw1, tbh1;
  int16_t tbx2, tby2; uint16_t tbw2, tbh2;
  int16_t tbx3, tby3; uint16_t tbw3, tbh3;
  display.getTextBounds(message1, 0, 0, &tbx1, &tby1, &tbw1, &tbh1);
  display.getTextBounds(message2, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);
  display.getTextBounds(message3, 0, 0, &tbx3, &tby3, &tbw3, &tbh3);

  // POSITIONING User messages on the left in 3 rows
  uint16_t x = 0; uint16_t y = 0;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    //print message1
    display.setCursor(x, y+tbh1);
    display.print(message1);
    //print message2
    display.setCursor(x, y+tbh1+tbh2+display.height()/3);
    display.print(message2);
    //print message3
    display.setCursor(x, y+tbh1+tbh2+tbh3+display.height()/3*2);
    display.print(message3);
  }
  while (display.nextPage());
}


RTC_DATA_ATTR long lastUpdateId = 0;   //super necessary to keep track of last read message offset
void fetchTelegramMessage() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=1";   
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, payload);

      // Check if there is at least one message
      if (doc["result"].size() > 0) {
        const char* text = doc["result"][doc["result"].size() - 1]["message"]["text"];
        if (text && strlen(text) > 0) {
          strncpy(telegramMessage1, text, sizeof(telegramMessage1));
          telegramMessage1[sizeof(telegramMessage1)-1] = '\0'; // ensure null-termination
        }
        // Update offset to prevent re-reading this message
        lastUpdateId = doc["result"][0]["update_id"];
      }
    }
    http.end();
  }
  DEBUG_PRINTLN("Fetched Telegram message: " + String(telegramMessage1));
}



void goToSleep(){
  DEBUG_PRINTLN("Counter at "+String(counter)+", going to sleep for 6s");
  counter++;
  delay(1000); //give some time to see the message before sleeping
  esp_sleep_enable_timer_wakeup(6*1000000); // x times 1million microseconds = x*1s = xs
  esp_deep_sleep_start();
}

void loop() {
  //youll do nüthing
}




