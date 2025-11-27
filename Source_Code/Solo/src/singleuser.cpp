/* board: ESPC3 Dev Module
--- Pin mapping for ESP32-C3-WROOM-02---
#define CS_PIN 7 // Chip select - IO7
#define BUSY_PIN 3 // Busy - IO3
#define RES_PIN 2 // Reset - IO2
#define DC_PIN 1 // Data/Command - IO1
#define SCL_PIN 4 // SCK - IO4
#define SDA_PIN 6 // MOSI - IO6
#define USER1 (5) // button1
#define USER2 (10) // button2
*/
#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <esp_sleep.h>
#include <esp32/rtc.h>
#include <WiFi.h>
#include <private.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

void setFrame();
void checkTelegram();
void partialUpdateTasks();
void goToSleep();

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

// ESP32-C3 CS(SS)=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RES(RST)=2,DC=1
#define CS_PIN (7)
#define BUSY_PIN (3)
#define RES_PIN (2)
#define DC_PIN (1)

#define INPUT1 (5)
#define INPUT2 (10)

// 2.13'' EPD Module
//GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // DEPG0213BN 122x250, SSD1680
// 4.2'' EPD Module
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // 400x300, SSD1683


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// RTC_DATA_ATTR for retaining data during sleep
RTC_DATA_ATTR bool FrameSet = 0; //flag to know if frame was set already
RTC_DATA_ATTR int counter = 0; //just for testing increments on each wakeup
RTC_DATA_ATTR char title[20] = "TODO:";
RTC_DATA_ATTR char NewTask[110] = "No Tasks"; //110 chars enough for 3 rows at font size 1
RTC_DATA_ATTR bool userUpdated = false; // flag to mark new message received for user
RTC_DATA_ATTR int RowIndex = 0; // to keep track of which row to write the new task
RTC_DATA_ATTR long lastUpdateId = 0;   //super necessary to keep track of last read message offset
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  setenv("TZ", "CET-1CEST,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
  tzset();
  DEBUG_PRINTLN("Setup/ woke up");
  //eink pins
  pinMode(CS_PIN, OUTPUT);
  pinMode(DC_PIN, OUTPUT);
  pinMode(RES_PIN, OUTPUT);
  pinMode(BUSY_PIN,OUTPUT);
  //buttons
  pinMode(INPUT1, INPUT);
  pinMode(INPUT2, INPUT);

  if (!FrameSet) {
    display.init(115200,true,50,false);
    delay(1500);//giving it some extra time seems to help
    setFrame();
    FrameSet = 1;
    DEBUG_PRINTLN("First boot, setting frame");
  } 
  else {
    DEBUG_PRINTLN("Frame set -> partial update");
  }
  display.init(115200,false,50,false); //partial update mode
  delay(1500);//giving it some extra time seems to help
  checkTelegram();
  partialUpdateTasks();
  goToSleep();
}


void setFrame() //first display update after init
{
  display.setRotation(0); // 0 rotation for 4inch from connector down 
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(2);
  //x,y of top left corner of the text bounding box, height and width of text in pixels
  int16_t tbx0, tby0; uint16_t tbw0, tbh0;
  int16_t tbx1, tby1; uint16_t tbw1, tbh1;
  int16_t tbx2, tby2; uint16_t tbw2, tbh2;
  int16_t tbx3, tby3; uint16_t tbw3, tbh3;
  display.getTextBounds(title, 0, 0, &tbx0, &tby0, &tbw0, &tbh0);
  // PRINTING TITLE AND TASK PLACEHOLDERS
  uint16_t x = 0; uint16_t y = 0;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // print title
    display.setCursor(x, y + tbh0);
    display.print(title);
    //print task arrows
    for (int i = 0; i < 10; i++) {
      // vertical spacing: tbh0 (title height) + padding + (i+1)*27 pixels per row
      display.setCursor(x, y + tbh0 + 3 + (i + 1) * 27);
      display.print(">");
    }
  } while (display.nextPage());
}

void checkTelegram()
{
  if (WiFi.status() != WL_CONNECTED) { //connect to WiFi if not connected
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("Connected WiFi: " + String(WIFI_SSID));
    DEBUG_PRINTLN("Telegram, lastUpdateId: " + String(lastUpdateId));
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=1";   
    http.begin(url);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, payload);

      if (doc["result"].size() > 0) {
        DEBUG_PRINTLN("New task received");
        JsonObject message = doc["result"][doc["result"].size() - 1]["message"];

        const char* text = message["text"];
        lastUpdateId = doc["result"][doc["result"].size() - 1]["update_id"];

        if (text && strlen(text) > 0) {
          const size_t maxMsgLen = 20;   // Max length before truncation
          char messagePart[32];          // For truncated message
          char timeBuffer[20];           // For formatted timestamp
          char combined[128];            // Timestamp + message

          // --- Truncate message if too long ---
          if (strlen(text) > maxMsgLen) {
            strncpy(messagePart, text, maxMsgLen);
            messagePart[maxMsgLen] = '\0';
            strcat(messagePart, "..");
          } else {
            strncpy(messagePart, text, sizeof(messagePart) - 1);
            messagePart[sizeof(messagePart) - 1] = '\0';
          }

          // --- Get Telegram timestamp (convert Unix -> local time) ---
          time_t msgTime = message["date"]; // Telegram’s Unix timestamp
          struct tm *timeinfo = localtime(&msgTime);
          strftime(timeBuffer, sizeof(timeBuffer), "%d/%m @%H", timeinfo);

          // --- Combine into one string ---
          snprintf(combined, sizeof(combined), "%s [%s]", messagePart, timeBuffer);

          // --- Store in NewTask safely ---
          strncpy(NewTask, combined, sizeof(NewTask) - 1);
          NewTask[sizeof(NewTask) - 1] = '\0';

          userUpdated = true;
          DEBUG_PRINTLN("User message: " + String(NewTask));
        }
      }
    }
    http.end();
  }
}


void partialUpdateTasks()
{
  const uint16_t TaskBoxX = 24;
  const uint16_t TaskBoxW = display.width()-20; // width of the message area
  const uint16_t TaskBoxH = 27;

  if (userUpdated){
    DEBUG_PRINTLN("Task Recived, updating display"); 
    userUpdated = false; // clear the flag
    const uint16_t TaskBoxY = RowIndex * TaskBoxH + 6 + 27; //6 + 27 to give a bit of padding
    RowIndex++;
    if (RowIndex >= 10) { // reset to first row if exceeding 10
      RowIndex = 0;
    }
    display.setPartialWindow(TaskBoxX, TaskBoxY, TaskBoxW, TaskBoxH);
    display.firstPage();
    do {
        display.fillRect(TaskBoxX, TaskBoxY, TaskBoxW, TaskBoxH, GxEPD_WHITE);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(1);
        display.setCursor(TaskBoxX, TaskBoxY + 15);   // baseline offset
        display.print(NewTask);
      } while (display.nextPage());
  }
}


void goToSleep()
{
  DEBUG_PRINTLN("Sleep counter at "+String(counter)+", going to sleep for 2s");
  counter++;
  delay(1000); //give some time to see the message before sleeping
  display.hibernate();  //put display to hibernate to save power
  esp_sleep_enable_timer_wakeup(2*1000000); // x times 1million microseconds = x*1s = xs
  esp_deep_sleep_start();
}


void loop() 
{
  //youll do nüthing
}

/*
TODO:
select and delete finished task:
through message command or button press

deal with this later:
The connection indicated an EOF
*/
