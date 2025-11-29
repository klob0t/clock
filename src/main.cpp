#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "BitPotionFont.h"
#include "MilfordFont.h"      

// =============================================================================
// 1. CONFIGURATION
// =============================================================================

const char* ssid     = "12";
const char* password = "p00chiz!";
String apiKey = "dd0bd7ff5c526a7abeb18234d403f935"; 
String city = "Jakarta";
String countryCode = "ID";

unsigned long lastWeatherCheck = 0;
unsigned long weatherCheckInterval = 600000; 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;      
const int   daylightOffset_sec = 0;

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   5
#define CS_PIN        5        

// 2 ZONES defined in Parola logic
MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX* mx; 

static const int TOTAL_WIDTH = 40;
static const int HUD_WIDTH = 16; 
uint8_t screenBuffer[TOTAL_WIDTH]; 

// =============================================================================
// 2. ENUMS & STATES
// =============================================================================
enum DateMode : uint8_t { MODE_DATE, MODE_SCRAMBLE, MODE_DAY };
enum PageState { PAGE_CLOCK, TRANS_TO_WEATHER, PAGE_WEATHER, TRANS_TO_CLOCK };

// =============================================================================
// 3. VARIABLES & BITMAPS
// =============================================================================

const uint64_t DIGITS[] = {
  0x0000000007050507, 0x0000000002020302, 0x0000000007010407, 0x0000000007040607,
  0x0000000004060505, 0x0000000007040107, 0x0000000007050701, 0x0000000002020407,
  0x0000000007050207, 0x0000000004070507
};
const uint64_t LETTERS[] = {
  0x0000000005050707, 0x0000000007050507, 0x0000000001030107, 0x0000000002020207,
  0x0000000007050505, 0x0000000007010307, 0x0000000007070505, 0x0000000007050503,
  0x0000000005050705, 0x0000000001030107, 0x0000000005030507, 0x0000000007020207,
  0x0000000007040107, 0x0000000005070507, 0x0000000005050503
};
const uint8_t DAY_MAP[8][3] = {
  {0,0,0}, {12,4,14}, {0,1,14}, {3,4,5}, {6,5,7}, {3,8,4}, {2,10,11}, {12,13,3}
};
const uint64_t CLOUDS[] = {
  0x0000000000000f06, 0x0000000000000f0c, 0x0000000000000f00, 
  0x0000000000000f04, 0x0000000000000e07, 0x0000000000000f05, 0x0000000000000f03
};
const uint8_t WEATHER_ICONS[8][8] = {
  {152, 100, 166, 109, 169, 98, 162, 92}, 
  {24, 102, 66, 129, 129, 66, 102, 24},   
  {24, 36, 36, 44, 40, 34, 34, 28},       
  {24, 36, 38, 45, 41, 34, 34, 28},       
  {24, 36, 6, 165, 113, 42, 2, 28},       
  {16, 46, 42, 45, 37, 34, 38, 28},       
  {80, 81, 85, 69, 17, 20, 84, 68},       
  {66, 195, 60, 36, 36, 60, 195, 66}      
};

// --- VARS ---
const int NUM_CLOUDS = 7;
const int CLOUD_WIDTH = 4;
const int CLOUD_GAP = 6;
const int TOTAL_CYCLE_LEN = NUM_CLOUDS * (CLOUD_WIDTH + CLOUD_GAP);

bool showColon = true;
unsigned long lastClockTick = 0;
DateMode currentMode = MODE_DATE;
DateMode nextMode    = MODE_DAY;
unsigned long modeTimer = 0;
uint64_t targetBitmaps[4]; 
int resolveFrame[4];   
int currentFrame = 0;

float cloudPos = 0.0;          
unsigned long lastCloudTick = 0;
const int CLOUD_DELAY = 150;   
const float CLOUD_SPEED = 0.2; 

PageState currentPage = PAGE_CLOCK;
unsigned long pageTimer = 0;
bool triggerZoneSetup = true; 

// Weather
int weatherIconID = 1; 
String weatherDesc = "Init..."; 
int feelsLikeTemp = 0;
char weatherTextBuffer[60]; 
char tempTextBuffer[10];

struct tm timeinfo;

// =============================================================================
// 4. FORWARD DECLARATIONS
// =============================================================================
void updateLocalTime();
int getDay(); int getMonth(); int getWeekday(); int getHour(); int getMinute();
void getWeatherData();
void drawToBuffer(int startCol, uint64_t image, int width, int yOffset);
void drawSpriteToBuffer(int startCol, int iconID);
void drawGlitch();
void startScrambleTransition(DateMode destination);
void updateModeSwitcher(unsigned long nowMs);
void updatePageLogic(unsigned long nowMs);
void updateDisplay(unsigned long nowMs);

// =============================================================================
// 5. SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  
  // Initialize with 2 Zones (We will reuse them)
  P.begin(2); 
  P.setIntensity(1);
  
  mx = P.getGraphicObject();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm t; while(!getLocalTime(&t)) delay(100);
  
  getWeatherData();
  pageTimer = millis();
}

void loop() {
  unsigned long nowMs = millis();
  
  if (nowMs - lastWeatherCheck > weatherCheckInterval) {
    getWeatherData();
    lastWeatherCheck = nowMs;
  }

  updatePageLogic(nowMs);
  updateDisplay(nowMs);
}

// =============================================================================
// 6. FUNCTIONS
// =============================================================================

void updateLocalTime() { getLocalTime(&timeinfo); }
int getDay() { return timeinfo.tm_mday; }
int getMonth() { return timeinfo.tm_mon + 1; }
int getWeekday() { return timeinfo.tm_wday + 1; }
int getHour() { return timeinfo.tm_hour; }
int getMinute() { return timeinfo.tm_min; }

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);
      
      int id = doc["weather"][0]["id"];
      String rawDesc = doc["weather"][0]["description"];
      float tempFloat = doc["main"]["feels_like"];
      feelsLikeTemp = round(tempFloat);

      if(rawDesc.length() > 0) rawDesc.setCharAt(0, toupper(rawDesc.charAt(0)));
      for(int i=1; i<rawDesc.length(); i++) {
        if(rawDesc.charAt(i-1) == ' ') rawDesc.setCharAt(i, toupper(rawDesc.charAt(i)));
      }
      weatherDesc = rawDesc;

      if (id == 800) weatherIconID = 1;                 
      else if (id >= 801 && id <= 802) weatherIconID = 2; 
      else if (id >= 803 && id <= 804) weatherIconID = 3; 
      else if (id >= 200 && id <= 232) weatherIconID = 4; 
      else if (id >= 300 && id <= 321) weatherIconID = 0; 
      else if (id >= 500 && id <= 531) weatherIconID = 5; 
      else if (id >= 600 && id <= 622) weatherIconID = 7; 
      else if (id >= 701 && id <= 781) weatherIconID = 6; 
      else weatherIconID = 3; 
      
      Serial.println("Updated: " + weatherDesc);
    }
    http.end();
  }
}

void drawToBuffer(int startCol, uint64_t image, int width, int yOffset) {
  if (image == 0) return;
  uint8_t rows[4];
  for (int r = 0; r < 4; r++) rows[r] = (image >> (8 * (3 - r))) & 0xFF; 

  for (int col = 0; col < width; col++) {
    uint8_t columnByte = 0;
    uint8_t b0 = (rows[0] >> col) & 0x01; 
    uint8_t b1 = (rows[1] >> col) & 0x01; 
    uint8_t b2 = (rows[2] >> col) & 0x01; 
    uint8_t b3 = (rows[3] >> col) & 0x01; 
    columnByte |= (b0 << 3); columnByte |= (b1 << 2); 
    columnByte |= (b2 << 1); columnByte |= (b3 << 0);

    int targetCol = startCol - col;
    if (targetCol >= 0 && targetCol < TOTAL_WIDTH) {
      screenBuffer[targetCol] |= (columnByte << yOffset);
    }
  }
}

void drawSpriteToBuffer(int startCol, int iconID) {
  if (iconID < 0 || iconID > 7) return;
  for(int i=0; i<8; i++) {
    uint8_t byte = WEATHER_ICONS[iconID][i];
    int targetCol = startCol - i;
    if (targetCol >= 0 && targetCol < TOTAL_WIDTH) {
      screenBuffer[targetCol] = byte;
    }
  }
}

void drawGlitch() {
  for(int i=0; i<TOTAL_WIDTH; i++) {
    if (random(0, 10) > 6) screenBuffer[i] = random(0, 255);
    else screenBuffer[i] = 0;
  }
  if (random(0,10) > 8) {
    int start = random(0, 32);
    for(int k=0; k<8; k++) screenBuffer[start+k] = ~screenBuffer[start+k];
  }
}

void startScrambleTransition(DateMode destination) {
  currentMode=MODE_SCRAMBLE; nextMode=destination; currentFrame=0; modeTimer=millis();
  updateLocalTime(); int d=getDay(); int m=getMonth(); int wd=getWeekday();
  if(destination==MODE_DATE){
    targetBitmaps[0]=DIGITS[d/10]; targetBitmaps[1]=DIGITS[d%10];
    targetBitmaps[2]=DIGITS[m/10]; targetBitmaps[3]=DIGITS[m%10];
  } else {
    targetBitmaps[0]=LETTERS[DAY_MAP[wd][0]]; targetBitmaps[1]=LETTERS[DAY_MAP[wd][1]];
    targetBitmaps[2]=LETTERS[DAY_MAP[wd][2]]; targetBitmaps[3]=0;
  }
  for(int i=0; i<4; i++) resolveFrame[i]=random(5,25);
}

void updateModeSwitcher(unsigned long nowMs) {
  if (currentMode==MODE_SCRAMBLE) return;
  if (nowMs - modeTimer > 3000UL) {
    DateMode dest = (currentMode == MODE_DATE) ? MODE_DAY : MODE_DATE;
    startScrambleTransition(dest);
  }
}

void updatePageLogic(unsigned long nowMs) {
  unsigned long duration = nowMs - pageTimer;

  switch (currentPage) {
    case PAGE_CLOCK:
      if (duration > 50000) { 
        currentPage = TRANS_TO_WEATHER;
        pageTimer = nowMs;
        P.displayClear(); 
      }
      break;

    case TRANS_TO_WEATHER:
      if (duration > 600) { 
        currentPage = PAGE_WEATHER;
        pageTimer = nowMs;
        triggerZoneSetup = true; 
      }
      break;

    case PAGE_WEATHER:
      if (duration > 10000) { 
        currentPage = TRANS_TO_CLOCK;
        pageTimer = nowMs;
        P.displayClear(); 
      }
      break;

    case TRANS_TO_CLOCK:
      if (duration > 600) { 
        currentPage = PAGE_CLOCK;
        pageTimer = nowMs;
        triggerZoneSetup = true;
      }
      break;
  }

  // --- ZONE RE-MAPPING LOGIC ---
  if (triggerZoneSetup) {
    mx->clear(); 
    
    if (currentPage == PAGE_CLOCK) {
      // PAGE 1 LAYOUT:
      // Zone 0: CLOCK -> Modules 2, 3, 4 (Indices 16-39)
      // Zone 1: DUMMY -> Module 0 (Index 0-7) 
      // WHY MODULE 0? Because the manual HUD also writes to Module 0.
      // If Parola tries to clear Zone 1, our manual HUD code 
      // runs AFTER Parola and redraws the clouds/date immediately.
      // This prevents Module 2 (part of the clock) from being erased.
      P.setZone(0, 2, 4); 
      P.setZone(1, 0, 0); 
      
      P.setFont(0, BitPotion);
      P.setTextAlignment(0, PA_LEFT);
      
      updateLocalTime();
      char clockStr[6];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", getHour(), getMinute());
      P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, "", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT); 
    } 
    else if (currentPage == PAGE_WEATHER) {
      // PAGE 2 LAYOUT:
      // Zone 0: DESC -> Modules 1, 2, 3 (Indices 8-31)
      // Zone 1: TEMP -> Module 0 (Indices 0-7)
      P.setZone(0, 1, 3); 
      P.setZone(1, 0, 0); 
      
      P.setFont(0, Milford); 
      P.setFont(1, Milford);
      P.setTextAlignment(0, PA_LEFT);
      P.setTextAlignment(1, PA_CENTER);
      
      strcpy(weatherTextBuffer, weatherDesc.c_str());
      sprintf(tempTextBuffer, "%d", feelsLikeTemp);
      
      // Init Text
      P.displayZoneText(0, weatherTextBuffer, PA_LEFT, 40, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayZoneText(1, tempTextBuffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      
      // FORCE START SCROLL
      // IMPORTANT: We must wait for one animate cycle before resetting? 
      // No, calling it here flags it to start.
    }
    
    mx->control(MD_MAX72XX::INTENSITY, 1);
    triggerZoneSetup = false;
  }
}

void updateDisplay(unsigned long nowMs) {
  memset(screenBuffer, 0, TOTAL_WIDTH);

  // --- TRANSITION GLITCH ---
  if (currentPage == TRANS_TO_WEATHER || currentPage == TRANS_TO_CLOCK) {
    drawGlitch(); 
    for(int i=0; i<TOTAL_WIDTH; i++) mx->setColumn(i, screenBuffer[i]);
    return; 
  }

  // --- ANIMATE PAROLA ---
  // If we are in Weather mode, ensure scroll loops
  if (P.displayAnimate()) {
    if (currentPage == PAGE_WEATHER) {
      P.displayReset(0); // Reset Zone 0 (Description) to loop
    }
  }

  // --- PAGE 1: CLOCK LOGIC ---
  if (currentPage == PAGE_CLOCK) {
    // Clock Tick
    if (nowMs - lastClockTick > 500) {
      lastClockTick = nowMs;
      updateLocalTime();
      showColon = !showColon;
      char clockStr[6];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", getHour(), getMinute());
      P.setTextBuffer(0, clockStr);
    }

    // Manual HUD (Modules 0,1) - THIS WILL OVERWRITE PAROLA ZONE 1 (Dummy)
    if (nowMs - lastCloudTick > CLOUD_DELAY) {
      lastCloudTick = nowMs;
      cloudPos += CLOUD_SPEED;
      if(cloudPos > TOTAL_CYCLE_LEN) cloudPos -= TOTAL_CYCLE_LEN;
    }
    if (getHour() < 12) {
      for(int i=0; i<NUM_CLOUDS; i++) {
        int offset = i * (CLOUD_WIDTH + CLOUD_GAP);
        float absolutePos = cloudPos + offset;
        float modPos = fmod(absolutePos, TOTAL_CYCLE_LEN);
        if(modPos < 0) modPos += TOTAL_CYCLE_LEN;
        if (modPos < HUD_WIDTH) drawToBuffer((int)modPos, CLOUDS[i], CLOUD_WIDTH, 5);
        if (modPos - TOTAL_CYCLE_LEN > -10) drawToBuffer((int)modPos - TOTAL_CYCLE_LEN, CLOUDS[i], CLOUD_WIDTH, 5);
      }
    }
    updateModeSwitcher(nowMs);
    if (currentMode == MODE_SCRAMBLE) {
      currentFrame++;
      bool allDone = true;
      for (int i=0; i<4; i++) if (currentFrame < resolveFrame[i]) allDone=false;
      if (allDone) { currentMode = nextMode; modeTimer = nowMs; }
    }
    int d=getDay(); int m=getMonth(); int wd=getWeekday();
    int cursor = 15; 
    for (int i=0; i<4; i++) {
      uint64_t img = 0;
      if (currentMode == MODE_SCRAMBLE) {
        if(currentFrame < resolveFrame[i]) img = LETTERS[random(0,15)]; else img = targetBitmaps[i];
      } else if (currentMode == MODE_DATE) {
        int val = (i==0)?d/10:(i==1)?d%10:(i==2)?m/10:m%10; img=DIGITS[val];
      } else {
        int idx = (i<3)?DAY_MAP[wd][i]:-1; img=(idx>=0)?LETTERS[idx]:0;
      }
      drawToBuffer(cursor, img, 3, 0);
      cursor -= 4;
    }
    // Push ONLY Modules 0 and 1 (This overwrites whatever P.displayAnimate did to Zone 1)
    for(int i=0; i<16; i++) mx->setColumn(i, screenBuffer[i]);
  }

  // --- PAGE 2: WEATHER LOGIC ---
  else if (currentPage == PAGE_WEATHER) {
    // Parola handles Zones 0 & 1 via displayAnimate()
    // We only need to draw the Sprite manually on Module 4
    
    drawSpriteToBuffer(39, weatherIconID);
    // Push ONLY Module 4
    for(int i=32; i<40; i++) mx->setColumn(i, screenBuffer[i]);
  }
}