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

const char *ssid = "12";
const char *password = "p00chiz!";
String apiKey = "dd0bd7ff5c526a7abeb18234d403f935";
String city = "Jakarta";
String countryCode = "ID";

unsigned long lastWeatherCheck = 0;
unsigned long weatherCheckInterval = 600000;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 5
#define CS_PIN 5

// 3 ZONES:
// ZONE 0: Clock (Modules 2-4)
// ZONE 1: Weather Desc (Modules 1-3)
// ZONE 2: Temp (Module 0)
MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX *mx;

static const int TOTAL_WIDTH = 40;
static const int HUD_WIDTH = 16;
uint8_t screenBuffer[TOTAL_WIDTH];

// =============================================================================
// 2. ENUMS & STATES
// =============================================================================
enum DateMode : uint8_t
{
  MODE_DATE,
  MODE_SCRAMBLE,
  MODE_DAY
};
enum PageState
{
  PAGE_CLOCK,
  TRANS_TO_WEATHER,
  PAGE_WEATHER,
  TRANS_TO_CLOCK
};

// =============================================================================
// 3. VARIABLES & BITMAPS
// =============================================================================

// 5-PIXEL TALL BITMAPS (Stored in Bytes 4,3,2,1,0)
const uint64_t DIGITS[] = {
    0x0000000305050506,
    0x0000000702020302,
    0x0000000702040507,
    0x0000000704020407,
    0x0000000404060505,
    0x0000000205040307,
    0x0000000205050306,
    0x0000000202040407,
    0x0000000705020507,
    0x0000000306050502};
const uint64_t LETTERS[] = {
    // 0x0000000505050707, // 1
    // 0x0000000305050506, // 2
    // 0x0000000101030107, // 3
    // 0x0000000202020207, // 4
    // 0x0000000305050505, // 5
    // 0x0000000701030107, // 6
    // 0x0000000707050505, // 7
    // 0x0000000705050503, //
    // 0x0000000505070505,
    // 0x0000000505030503,
    // 0x0000000702020207,
    // 0x0000000704060107,
    // 0x0000000505070503,
    // 0x0000000505050503,
    // 0x0000000505020505

    0x0000000505020505,
    0x0000000505050707,
    0x0000000305050506,
    0x0000000101030107,
    0x0000000202020207,
    0x0000000605050505,
    0x0000000701030107,
    0x0000000707050505,
    0x0000000705050503,
    0x0000000505070505,
    0x0000000505030503,
    0x0000000702020207,
    0x0000000704060107,
    0x0000000505070503,
    0x0000000505050503

};
const uint8_t DAY_MAP[8][3] = {
    {0, 0, 0}, {12, 5, 14}, {1, 2, 14}, {4, 5, 6}, {7, 6, 8}, {4, 9, 5}, {3, 10, 11}, {12, 13, 4}};
const uint64_t CLOUDS[] = {
    0x0000000000000f06, 0x0000000000000f0c, 0x0000000000000f00,
    0x0000000000000f04, 0x0000000000000e07, 0x0000000000000f05, 0x0000000000000f03};
const uint8_t WEATHER_ICONS[8][8] = {
    {152, 100, 166, 109, 169, 98, 162, 92},
    {24, 102, 66, 129, 129, 66, 102, 24},
    {24, 36, 36, 44, 40, 34, 34, 28},
    {24, 36, 38, 45, 41, 34, 34, 28},
    {24, 36, 6, 165, 113, 42, 2, 28},
    {16, 46, 42, 45, 37, 34, 38, 28},
    {80, 81, 85, 69, 17, 20, 84, 68},
    {66, 195, 60, 36, 36, 60, 195, 66}};

// --- VARS ---
const int NUM_CLOUDS = 7;
const int CLOUD_WIDTH = 4;
const int CLOUD_GAP = 6;
const int TOTAL_CYCLE_LEN = NUM_CLOUDS * (CLOUD_WIDTH + CLOUD_GAP);

bool showColon = true;
unsigned long lastClockTick = 0;
DateMode currentMode = MODE_DATE;
DateMode nextMode = MODE_DAY;
unsigned long modeTimer = 0;
uint64_t targetBitmaps[4];
int resolveFrame[4];
int currentFrame = 0;

int scrambleIndices[4];

float cloudPos = 0.0;
unsigned long lastCloudTick = 0;
const int CLOUD_DELAY = 150;
const float CLOUD_SPEED = 0.2;

unsigned long lastScrambleTick = 0;
const int SCRAMBLE_DELAY = 25;

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
int getDay();
int getMonth();
int getWeekday();
int getMinute();
int getHour();
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
void setup()
{
  Serial.begin(115200);

  P.begin(3);
  P.setIntensity(1);

  // DEFINE INITIAL ZONES (Page 1)
  P.setZone(0, 2, 4);
  P.setZone(1, 0, 0);
  P.setZone(2, 0, 0);

  mx = P.getGraphicObject();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    delay(100);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm t;
  while (!getLocalTime(&t))
    delay(100);

  getWeatherData();
  pageTimer = millis();
}

void loop()
{
  unsigned long nowMs = millis();

  if (nowMs - lastWeatherCheck > weatherCheckInterval)
  {
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

void getWeatherData()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&appid=" + apiKey + "&units=metric";
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      String payload = http.getString();
      JsonDocument doc;
      deserializeJson(doc, payload);

      int id = doc["weather"][0]["id"];
      String rawDesc = doc["weather"][0]["description"];
      float tempFloat = doc["main"]["feels_like"];
      feelsLikeTemp = round(tempFloat);

      if (rawDesc.length() > 0)
        rawDesc.setCharAt(0, toupper(rawDesc.charAt(0)));
      for (int i = 1; i < rawDesc.length(); i++)
      {
        if (rawDesc.charAt(i - 1) == ' ')
          rawDesc.setCharAt(i, toupper(rawDesc.charAt(i)));
      }
      weatherDesc = rawDesc;

      if (id == 800)
        weatherIconID = 1;
      else if (id >= 801 && id <= 802)
        weatherIconID = 2;
      else if (id >= 803 && id <= 804)
        weatherIconID = 3;
      else if (id >= 200 && id <= 232)
        weatherIconID = 4;
      else if (id >= 300 && id <= 321)
        weatherIconID = 0;
      else if (id >= 500 && id <= 531)
        weatherIconID = 5;
      else if (id >= 600 && id <= 622)
        weatherIconID = 7;
      else if (id >= 701 && id <= 781)
        weatherIconID = 6;
      else
        weatherIconID = 3;
    }
    http.end();
  }
}

// --- CORRECTED DRAW FUNCTION FOR 5-PIXEL FONTS ---
void drawToBuffer(int startCol, uint64_t image, int width, int yOffset)
{
  if (image == 0)
    return;

  uint8_t rows[5];

  // Extract 5 rows: Byte 4 (Top) to Byte 0 (Bottom)
  rows[0] = (image >> 32) & 0xFF; // Top Row
  rows[1] = (image >> 24) & 0xFF;
  rows[2] = (image >> 16) & 0xFF;
  rows[3] = (image >> 8) & 0xFF;
  rows[4] = (image >> 0) & 0xFF; // Bottom Row

  for (int col = 0; col < width; col++)
  {
    uint8_t columnByte = 0;

    // Explicitly map Row Index to Bit Position (Higher row = Higher Bit)
    uint8_t b0 = (rows[0] >> col) & 0x01; // Top Row
    uint8_t b1 = (rows[1] >> col) & 0x01;
    uint8_t b2 = (rows[2] >> col) & 0x01;
    uint8_t b3 = (rows[3] >> col) & 0x01;
    uint8_t b4 = (rows[4] >> col) & 0x01; // Bottom Row

    columnByte |= (b0 << 4); // Bit 4 = Top
    columnByte |= (b1 << 3);
    columnByte |= (b2 << 2);
    columnByte |= (b3 << 1);
    columnByte |= (b4 << 0); // Bit 0 = Bottom

    int targetCol = startCol - col;
    if (targetCol >= 0 && targetCol < TOTAL_WIDTH)
    {
      screenBuffer[targetCol] |= (columnByte << yOffset);
    }
  }
}

void drawSpriteToBuffer(int startCol, int iconID)
{
  if (iconID < 0 || iconID > 7)
    return;
  for (int i = 0; i < 8; i++)
  {
    uint8_t byte = WEATHER_ICONS[iconID][i];
    int targetCol = startCol - i;
    if (targetCol >= 0 && targetCol < TOTAL_WIDTH)
    {
      screenBuffer[targetCol] = byte;
    }
  }
}

void drawGlitch()
{
  for (int i = 0; i < TOTAL_WIDTH; i++)
  {
    if (random(0, 10) > 6)
      screenBuffer[i] = random(0, 255);
    else
      screenBuffer[i] = 0;
  }
  if (random(0, 10) > 8)
  {
    int start = random(0, 32);
    for (int k = 0; k < 8; k++)
      screenBuffer[start + k] = ~screenBuffer[start + k];
  }
}

void startScrambleTransition(DateMode destination)
{
  currentMode = MODE_SCRAMBLE;
  nextMode = destination;
  currentFrame = 0;
  modeTimer = millis();
  updateLocalTime();
  int d = getDay();
  int m = getMonth();
  int wd = getWeekday();
  if (destination == MODE_DATE)
  {
    targetBitmaps[0] = DIGITS[d / 10];
    targetBitmaps[1] = DIGITS[d % 10];
    targetBitmaps[2] = DIGITS[m / 10];
    targetBitmaps[3] = DIGITS[m % 10];
  }
  else
  {
    targetBitmaps[0] = LETTERS[DAY_MAP[wd][0]];
    targetBitmaps[1] = LETTERS[DAY_MAP[wd][1]];
    targetBitmaps[2] = LETTERS[DAY_MAP[wd][2]];
    targetBitmaps[3] = 0;
  }
  for (int i = 0; i < 4; i++)
  {
    resolveFrame[i] = random(20, 60);   // (Kept your longer duration)
    scrambleIndices[i] = random(0, 15); // <--- ADD THIS: Seed initial random char
  }
}

void updateModeSwitcher(unsigned long nowMs)
{
  if (currentMode == MODE_SCRAMBLE)
    return;
  if (nowMs - modeTimer > 3000UL)
  {
    DateMode dest = (currentMode == MODE_DATE) ? MODE_DAY : MODE_DATE;
    startScrambleTransition(dest);
  }
}

void updatePageLogic(unsigned long nowMs)
{
  unsigned long duration = nowMs - pageTimer;

  switch (currentPage)
  {
  case PAGE_CLOCK:
    if (duration > 50000)
    {
      currentPage = TRANS_TO_WEATHER;
      pageTimer = nowMs;
      P.displayClear();
    }
    break;

  case TRANS_TO_WEATHER:
    if (duration > 600)
    {
      currentPage = PAGE_WEATHER;
      pageTimer = nowMs;
      triggerZoneSetup = true;
    }
    break;

  case PAGE_WEATHER:
    if (duration > 10000)
    {
      currentPage = TRANS_TO_CLOCK;
      pageTimer = nowMs;
      P.displayClear();
    }
    break;

  case TRANS_TO_CLOCK:
    if (duration > 600)
    {
      currentPage = PAGE_CLOCK;
      pageTimer = nowMs;
      triggerZoneSetup = true;
    }
    break;
  }

  if (triggerZoneSetup)
  {
    mx->clear();
    P.begin(3);

    if (currentPage == PAGE_CLOCK)
    {
      P.setZone(0, 2, 4);
      P.setZone(1, 0, 0);
      P.setZone(2, 0, 0);

      P.setFont(0, BitPotion);
      P.setTextAlignment(0, PA_LEFT);

      updateLocalTime();
      int h = getHour();
      int h12 = (h % 12 == 0) ? 12 : (h % 12);
      char clockStr[6];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", h12, getMinute());
      P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, "", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(2, "", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    }
    else if (currentPage == PAGE_WEATHER)
    {
      // PAGE 2
      P.setZone(0, 4, 4); // Hide Clock
      P.setZone(1, 1, 3); // Desc
      P.setZone(2, 0, 0); // Hide Temp (Manual Draw)

      P.setFont(1, Milford);
      P.setTextAlignment(1, PA_LEFT);

      strcpy(weatherTextBuffer, weatherDesc.c_str());

      P.displayZoneText(0, "", PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, weatherTextBuffer, PA_LEFT, 40, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      P.displayZoneText(2, "", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT); // Clear Zone 2 for manual
    }

    mx->control(MD_MAX72XX::INTENSITY, 1);
    triggerZoneSetup = false;
  }
}

void updateDisplay(unsigned long nowMs)
{
  memset(screenBuffer, 0, TOTAL_WIDTH);

  if (currentPage == TRANS_TO_WEATHER || currentPage == TRANS_TO_CLOCK)
  {
    drawGlitch();
    for (int i = 0; i < TOTAL_WIDTH; i++)
      mx->setColumn(i, screenBuffer[i]);
    return;
  }

  if (P.displayAnimate())
  {
    if (currentPage == PAGE_WEATHER)
    {
      if (P.getZoneStatus(1))
        P.displayReset(1);
    }
  }

  if (currentPage == PAGE_CLOCK)
  {
    if (nowMs - lastClockTick > 500)
    {
      lastClockTick = nowMs;
      updateLocalTime();
      int h = getHour();
      int h12 = (h % 12 == 0) ? 12 : (h % 12);
      showColon = !showColon;
      static char clockStr[10];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", h12, getMinute());
      P.setTextBuffer(0, clockStr);
      P.displayReset(0);
    }

    if (nowMs - lastCloudTick > CLOUD_DELAY)
    {
      lastCloudTick = nowMs;
      cloudPos += CLOUD_SPEED;
      if (cloudPos > TOTAL_CYCLE_LEN)
        cloudPos -= TOTAL_CYCLE_LEN;
    }
    if (getHour() < 12)
    {
      for (int i = 0; i < NUM_CLOUDS; i++)
      {
        int offset = i * (CLOUD_WIDTH + CLOUD_GAP);
        float absolutePos = cloudPos + offset;
        float modPos = fmod(absolutePos, TOTAL_CYCLE_LEN);
        if (modPos < 0)
          modPos += TOTAL_CYCLE_LEN;
        if (modPos < HUD_WIDTH)
          drawToBuffer((int)modPos, CLOUDS[i], CLOUD_WIDTH, 5);
        if (modPos - TOTAL_CYCLE_LEN > -10)
          drawToBuffer((int)modPos - TOTAL_CYCLE_LEN, CLOUDS[i], CLOUD_WIDTH, 5);
      }
    }
    updateModeSwitcher(nowMs);
    // if (currentMode == MODE_SCRAMBLE)
    // {
    //   currentFrame++;
    //   bool allDone = true;
    //   for (int i = 0; i < 4; i++)
    //     if (currentFrame < resolveFrame[i])
    //       allDone = false;
    //   if (allDone)
    //   {
    //     currentMode = nextMode;
    //     modeTimer = nowMs;
    //   }
    // }

    if (currentMode == MODE_SCRAMBLE)
    {
      if (nowMs - lastScrambleTick > SCRAMBLE_DELAY)
      {
        lastScrambleTick = nowMs;
        currentFrame++;

        for (int k = 0; k < 4; k++)
          scrambleIndices[k] = random(0, 15);
      }

      bool allDone = true;
      for (int i = 0; i < 4; i++)
      {
        if (currentFrame < resolveFrame[i])
          allDone = false;
      }
      if (allDone)
      {
        currentMode = nextMode;
        modeTimer = nowMs;
      }
    }

    int d = getDay();
    int m = getMonth();
    int wd = getWeekday();
    int cursor = 15;
    for (int i = 0; i < 4; i++)
    {
      uint64_t img = 0;
      int charWidth = 3;
      if (currentMode == MODE_SCRAMBLE)
      {
        if (currentFrame < resolveFrame[i])
        {
          img = LETTERS[scrambleIndices[i]];
          charWidth = 3;
        }
        else
        {
          img = targetBitmaps[i];
          charWidth = 3;
        }
      }
      else if (currentMode == MODE_DATE)
      {
        int val = (i == 0) ? d / 10 : (i == 1) ? d % 10
                                  : (i == 2)   ? m / 10
                                               : m % 10;
        img = DIGITS[val];
        charWidth = 3;
      }
      else
      {
        int idx = (i < 3) ? DAY_MAP[wd][i] : -1;
        img = (idx >= 0) ? LETTERS[idx] : 0;
        charWidth = 3;
      }
      drawToBuffer(cursor, img, charWidth, 0);
      cursor -= charWidth;
      cursor -= 1;
    }
    for (int i = 0; i < 16; i++)
      mx->setColumn(i, screenBuffer[i]);
  }

  else if (currentPage == PAGE_WEATHER)
  {
    // Draw Sprite (Module 4)
    drawSpriteToBuffer(39, weatherIconID);

    // Draw Temp Manually (Module 0)
    // Layout: [Tens][Units][Degree]
    // 3px + 3px + 1px = 7px width. Fits in 8px.
    int t = feelsLikeTemp;
    int tens = t / 10;
    int units = t % 10;

    // Draw Tens at Col 7 (Width 3)
    if (t >= 10)
      drawToBuffer(7, DIGITS[tens], 3, 2); // yOffset 2 to center
    // Draw Units at Col 3 (Width 3) (1px gap from tens)
    drawToBuffer(3, DIGITS[units], 3, 2);
    // Draw Degree at Col 0 (Top dot)
    // 0x10 is Bit 4. If yOffset is 0, it's top.
    // Let's manually set bit 4 at column 0.
    screenBuffer[0] |= 0x10;

    // Push Module 4 and Module 0
    for (int i = 32; i < 40; i++)
      mx->setColumn(i, screenBuffer[i]);
    for (int i = 0; i < 8; i++)
      mx->setColumn(i, screenBuffer[i]);
  }
}