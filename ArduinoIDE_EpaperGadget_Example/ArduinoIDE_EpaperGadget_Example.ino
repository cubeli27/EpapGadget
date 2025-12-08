/*
STEPS: 
BOARDS MANAGER: install "esp32" or "Arduino ESP32 Boards"
LIBRARY MANAGER: install GxEPD2 by Jean-Marc Zingg
Tools:
-Select Board: "ESPC3 Dev Module"
-Select Port: "COM{something}"
-USB CDC On Boot: Enabled
|
|
--- Pin mapping for ESP32-C3-WROOM-02---
#define CS_PIN 7 // Chip select - IO7
#define BUSY_PIN 3 // Busy - IO3
#define RES_PIN 2 // Reset - IO2
#define DC_PIN 1 // Data/Command - IO1
#define SCL_PIN 4 // SCK - IO4
#define SDA_PIN 6 // MOSI - IO6
#define USER1 (5) // Button1 - IO5
#define USER2 (10) // Button2 -IO10
*/

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
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

// 4.2'' EPD Module
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN)); // 400x300, SSD1683

int counter = 0;  
void setup()
{
  Serial.begin(115200);
  Serial.println("setup");
  display.init(115200,true,50,false);
  delay(1000);
  setFrame();
  display.init(115200,false,50,false);
}

const char Line1[] = "Hello";
const char Line2[] = "World";
int16_t y2;
void setFrame()
{
  display.setRotation(0);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    //Line1
    display.setTextSize(1);
    display.getTextBounds(Line1, 0, 0, &tbx, &tby, &tbw, &tbh);
    int16_t x1 = (display.width() - tbw)/2 - tbx;
    int16_t y1 = display.height()/2 - tbh;
    display.setCursor(x1, y1);
    display.print(Line1);
    //Line 2
    display.setTextSize(2);
    display.getTextBounds(Line2, 0, 0, &tbx, &tby, &tbw, &tbh);
    int16_t x2 = (display.width() - tbw)/2 - tbx;
    y2 = y1 + tbh + 10;
    display.setCursor(x2, y2);
    display.print(Line2);
  } while (display.nextPage());
}

char counterChar[20];
void partialUpdateCounter()
{
  sprintf(counterChar,"%d",counter);
  display.setTextSize(1);
  int16_t ptbx, ptby; uint16_t ptbw, ptbh;
  display.getTextBounds(counterChar, 0, 0, &ptbx, &ptby, &ptbw, &ptbh); 
  uint16_t PartialUpdateX = (display.width() - ptbw)/2 - ptbx;
  uint16_t PartialUpdateY = y2 + 20;
  uint16_t PartialUpdateW = ptbw + 10;
  uint16_t PartialUpdateH = ptbh + 10;
  display.setPartialWindow(PartialUpdateX,PartialUpdateY,PartialUpdateW,PartialUpdateH);
  display.firstPage();
  do{
    display.fillRect(PartialUpdateX,PartialUpdateY,PartialUpdateW,PartialUpdateH,GxEPD_WHITE);
    display.setCursor(PartialUpdateX,PartialUpdateY - ptby);
    display.print(counterChar);
  }
  while(display.nextPage());
  Serial.println("Counter:"+String(counter));
  counter += 1;
}


void loop() 
{
  Serial.println("loop");
  partialUpdateCounter();
  delay(5000);//
}
