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

// ESP32-C3 CS(SS)=7,SCL(SCK)=4,SDA(MOSI)=6,BUSY=3,RES(RST)=2,DC=1
#define CS_PIN (7)
#define BUSY_PIN (3)
#define RES_PIN (2)
#define DC_PIN (1)

// 2.13'' EPD Module
//GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // DEPG0213BN 122x250, SSD1680
// 4.2'' EPD Module
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // 400x300, SSD1683

RTC_DATA_ATTR int counter = 0; // RTC_DATA_ATTR for retaining data during sleep
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

char message1[20];
char message2[20];

void helloWorld()
{

  display.setRotation(0); // 0 rotation for 4inch from connector down 
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  sprintf(message1, "Count: %d", counter);
  sprintf(message2, "Next: %d", counter + 1);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(message1, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y-tbh);
    display.print(message1);
    display.setTextColor(GxEPD_BLACK);
    display.getTextBounds(message2, 0, 0, &tbx, &tby, &tbw, &tbh);
    x = ((display.width() - tbw) / 2) - tbx;
    display.setCursor(x, y+tbh);
    display.print(message2);
  }
  while (display.nextPage());
}

void goToSleep(){
  DEBUG_PRINTLN("Counter at "+String(counter)+", going to sleep for 60s");
  counter++;
  delay(1000);
  esp_sleep_enable_timer_wakeup(60*1000000); // x times 1million microseconds = x*1s = xs
  esp_deep_sleep_start();
}

void loop() {
  //youll do nüthing
}




