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

void setFrame();
void fetchTelegramMessage();
void partialUpdateTelegramMsgs();
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
RTC_DATA_ATTR char title[20] = "TITLE";
RTC_DATA_ATTR char user1[30] = "USER1: ";
RTC_DATA_ATTR char user2[30] = "USER2: ";
RTC_DATA_ATTR char user3[30] = "USER3: ";
RTC_DATA_ATTR uint16_t User1_X ;
RTC_DATA_ATTR uint16_t User1_Y ;
RTC_DATA_ATTR uint16_t User2_X ;
RTC_DATA_ATTR uint16_t User2_Y ;
RTC_DATA_ATTR uint16_t User3_X ;
RTC_DATA_ATTR uint16_t User3_Y ;
RTC_DATA_ATTR uint16_t pad = 6; //padding for text boxes
RTC_DATA_ATTR char telegramMessage1[110] = "No Msg"; //110 chars enough for 3 rows at font size 1   
RTC_DATA_ATTR char telegramMessage2[110] = "No Msg";
RTC_DATA_ATTR char telegramMessage3[110] = "No Msg";
RTC_DATA_ATTR long lastUpdateId = 0;   //super necessary to keep track of last read message offset
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);
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
  fetchTelegramMessage();
  partialUpdateTelegramMsgs();
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
  display.getTextBounds(user1, 0, 0, &tbx1, &tby1, &tbw1, &tbh1);
  display.getTextBounds(user2, 0, 0, &tbx2, &tby2, &tbw2, &tbh2);
  display.getTextBounds(user3, 0, 0, &tbx3, &tby3, &tbw3, &tbh3);
  // PRINTING TITLE HORIZONTALLY CENTERED AND USERS BELOW TITLE EVENLY SPACED
  uint16_t x = 0; uint16_t y = 0;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    //print title
    display.setCursor((display.width() - tbw0) / 2, y + tbh0);
    display.print(title);
    //print user1
    display.setCursor(x, y-tby1+tbh0+pad);
    display.print(user1);
    User1_X = tbw1;
    User1_Y = y - tby1 + tbh0 + pad;
    //print user2
    display.setCursor(x, y-tby2+tbh0+pad+(display.height()-tbh0-pad)/3);
    display.print(user2);
    User2_X = tbw2;
    User2_Y = y - tby2 + tbh0 + pad + (display.height()-tbh0-pad)/3;
    //print user3
    display.setCursor(x, y-tby3+tbh0+pad+(display.height()-tbh0-pad)/3*2);
    display.print(user3);
    User3_X = tbw3;
    User3_Y = y - tby3 + tbh0 + pad + (display.height()-tbh0-pad)/3*2;
  }
  while (display.nextPage());
  DEBUG_PRINTLN("tbx0: "+String(tbx0)+", tby0: "+String(tby0)+", tbw0: "+String(tbw0)+", tbh0: "+String(tbh0));
  DEBUG_PRINTLN("tbx1: "+String(tbx1)+", tby1: "+String(tby1)+", tbw1: "+String(tbw1)+", tbh1: "+String(tbh1));
  DEBUG_PRINTLN("tbx2: "+String(tbx2)+", tby2: "+String(tby2)+", tbw2: "+String(tbw2)+", tbh2: "+String(tbh2));
  DEBUG_PRINTLN("tbx3: "+String(tbx3)+", tby3: "+String(tby3)+", tbw3: "+String(tbw3)+", tbh3: "+String(tbh3));
}

bool user1Updated = false;
bool user2Updated = false;
bool user3Updated = false;

void fetchTelegramMessage() 
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
    DEBUG_PRINTLN("Connected WiFi, checking Telegram, lastUpdateId: " + String(lastUpdateId));
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&limit=1";   
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, payload);

      if (doc["result"].size() > 0) {
        DEBUG_PRINTLN("New message received");
        JsonObject message = doc["result"][doc["result"].size() - 1]["message"];

        const char* text = message["text"];
        const char* username = message["from"]["username"];  // sender username
        lastUpdateId = doc["result"][doc["result"].size() - 1]["update_id"];

        if (text && strlen(text) > 0) {
            if (username && strcmp(username, USER1_USERNAME) == 0) {
                strncpy(telegramMessage1, text, sizeof(telegramMessage1));
                telegramMessage1[sizeof(telegramMessage1)-1] = '\0';
                user1Updated = true;
                DEBUG_PRINTLN("User1 message: " + String(telegramMessage1));
            } 
            else if (username && strcmp(username, USER2_USERNAME) == 0) {
                strncpy(telegramMessage2, text, sizeof(telegramMessage2));
                telegramMessage2[sizeof(telegramMessage2)-1] = '\0';
                user2Updated = true;
                DEBUG_PRINTLN("User2 message: " + String(telegramMessage2));
            } 
            else if (username && strcmp(username, USER3_USERNAME) == 0) {
                strncpy(telegramMessage3, text, sizeof(telegramMessage3));
                telegramMessage3[sizeof(telegramMessage3)-1] = '\0';
                user3Updated = true; 
                DEBUG_PRINTLN("User3 message: " + String(telegramMessage3));
            }
            else {
                DEBUG_PRINTLN("Message from unknown user: " + String(username));
            }
        }
      }
    }
    http.end();
  }
}


void partialUpdateTelegramMsgs()
{
  const uint16_t msgBoxX = 0;
  const uint16_t msgBoxW = display.width(); // width of the message area
  const uint16_t msgBoxH = (display.height()-22)/3-24;  // 24 good number:)

  if (user1Updated){
    DEBUG_PRINTLN("Partial update USER1 Telegram message"); 
    user1Updated = false; // clear the flag
    const uint16_t msgBoxY = User1_Y+1; //1 to give a bit of padding

    display.setPartialWindow(msgBoxX, msgBoxY, msgBoxW, msgBoxH);
    display.firstPage();
    do {
        display.fillRect(msgBoxX, msgBoxY, msgBoxW, msgBoxH, GxEPD_WHITE);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(1);
        display.setCursor(msgBoxX, msgBoxY + 15);   // baseline offset
        display.print(telegramMessage1);
      } while (display.nextPage());
  }
  if (user2Updated){
  DEBUG_PRINTLN("Partial update USER2 Telegram message");
  user2Updated = false; // clear the flag
  const uint16_t msgBoxY = User2_Y + 1; //1 to give a bit of padding
  
  display.setPartialWindow(msgBoxX, msgBoxY, msgBoxW, msgBoxH);
  display.firstPage();
  do {
      display.fillRect(msgBoxX, msgBoxY, msgBoxW, msgBoxH, GxEPD_WHITE);
      display.setFont(&FreeMonoBold9pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setTextSize(1);
      display.setCursor(msgBoxX, msgBoxY + 15);   // baseline offset
      display.print(telegramMessage2);
    } while (display.nextPage());
  }
  if (user3Updated){
  DEBUG_PRINTLN("Partial update USER3 Telegram message");
  user3Updated = false; // clear the flag
  const uint16_t msgBoxY = User3_Y + 1; //1 to give a bit of padding

  display.setPartialWindow(msgBoxX, msgBoxY, msgBoxW, msgBoxH);
  display.firstPage();
  do {
      display.fillRect(msgBoxX, msgBoxY, msgBoxW, msgBoxH, GxEPD_WHITE);
      display.setFont(&FreeMonoBold9pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setTextSize(1);
      display.setCursor(msgBoxX, msgBoxY + 15);   // baseline offset
      display.print(telegramMessage3);
    } while (display.nextPage());
  }
}


void goToSleep()
{
  DEBUG_PRINTLN("Sleep counter at "+String(counter)+", going to sleep for 6s");
  counter++;
  delay(1000); //give some time to see the message before sleeping
  display.hibernate();  //put display to hibernate to save power
  esp_sleep_enable_timer_wakeup(10*1000000); // x times 1million microseconds = x*1s = xs
  esp_deep_sleep_start();
}


void loop() 
{
  //youll do nüthing
}

/*TODO LIST:
-battery monitor partial update, custom graphics
-user inputs handling
-text wrapping for telegram messages, truncation for too long messages
-cuts G in Hellwig shift messages a few pixels lower for each user
*/


