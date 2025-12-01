#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include "time.h"
#include "BitPotionFont.h"
#include "MilfordFont.h"
#include "config.h"
#include "time_service.h"
#include "weather_service.h"
#include "display_clock.h"
#include "display_weather.h"
#include "animations.h"

// =============================================================================
// 1. STATE
// =============================================================================

unsigned long lastWeatherCheck = 0;
unsigned long weatherCheckInterval = WEATHER_CHECK_INTERVAL;

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
constexpr int WEATHER_ICON_COUNT = 9;
enum WeatherIcon : uint8_t
{
  ICON_HEAVY_RAIN = 0,
  ICON_CLEAR = 1,
  ICON_FEW_CLOUDS = 2,
  ICON_BROKEN_CLOUDS = 3,
  ICON_THUNDERSTORM = 4,
  ICON_UNKNOWN = 5,
  ICON_DRIZZLE = 6,
  ICON_ATMOSPHERE = 7,
  ICON_SNOW = 8
};
const uint8_t WEATHER_ICONS[WEATHER_ICON_COUNT][8] = {
    {24, 36, 6, 175, 113, 42, 10, 28},
    {24, 102, 66, 129, 129, 66, 102, 24},
    {48, 72, 72, 72, 80, 84, 68, 56},
    {48, 72, 76, 82, 82, 68, 68, 56},
    {24, 36, 6, 165, 113, 42, 2, 28},
    {32, 92, 84, 90, 74, 68, 76, 56},
    {144, 110, 42, 45, 165, 98, 38, 28},
    {80, 81, 85, 69, 17, 20, 84, 68},
    {66, 195, 60, 36, 36, 60, 195, 66}};

// --- VARS ---
bool showColon = true;
int lastSecond = -1;
DateMode currentMode = MODE_DATE;
DateMode nextMode = MODE_DAY;
unsigned long modeTimer = 0;
uint64_t targetBitmaps[4];
int resolveFrame[4];
int currentFrame = 0;

int scrambleIndices[4];

unsigned long lastScrambleTick = 0;
const int SCRAMBLE_DELAY = 25;

PageState currentPage = PAGE_CLOCK;
unsigned long pageTimer = 0;
bool triggerZoneSetup = true;

// Clock page bounce dot on module 1 (cols 8-15)
const int BOUNCE_START_COL = 0;
const int BOUNCE_END_COL = 15;
float bouncePos = BOUNCE_START_COL;
float bounceVel = 0.0f;
const float BOUNCE_G_MAG = 58.0f;       // cols/s^2 toward the ground (left)
const float BOUNCE_LAUNCH_V = 40.0f;    // initial rightward launch speed
const unsigned long BOUNCE_MAX_DT = 40; // clamp dt to avoid big jumps
unsigned long lastBounceTick = 0;

// Weather
uint8_t weatherIconID = ICON_CLEAR;
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
int getSecond();
int getHour();
void getWeatherData();
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

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
    delay(100);

  initTime(NTP_SERVER, GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC);
  while (!getTimeState(500).valid)
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

void updateLocalTime()
{
  TimeState ts = getTimeState();
  if (ts.valid)
    timeinfo = ts.now;
}
int getDay() { return timeinfo.tm_mday; }
int getMonth() { return timeinfo.tm_mon + 1; }
int getWeekday() { return timeinfo.tm_wday + 1; }
int getHour() { return timeinfo.tm_hour; }
int getSecond() { return timeinfo.tm_sec; }
int getMinute() { return timeinfo.tm_min; }

void getWeatherData()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    weatherDesc = "No WiFi";
    weatherIconID = ICON_UNKNOWN;
    feelsLikeTemp = 0;
    return;
  }

  WeatherData data = fetchWeather(OWM_API_KEY, LATITUDE, LONGITUDE);
  if (data.valid)
  {
    int id = data.id;
    weatherDesc = data.desc;
    feelsLikeTemp = data.feelsLike;

    if (id >= 200 && id <= 232)
      weatherIconID = ICON_THUNDERSTORM;
    else if (id >= 300 && id <= 321)
      weatherIconID = ICON_DRIZZLE;
    else if (id >= 500 && id <= 501)
      weatherIconID = ICON_DRIZZLE;
    else if (id >= 502 && id <= 531)
      weatherIconID = ICON_HEAVY_RAIN;
    else if (id >= 600 && id <= 622)
      weatherIconID = ICON_SNOW;
    else if (id >= 700 && id <= 781)
      weatherIconID = ICON_ATMOSPHERE;
    else if (id == 800)
      weatherIconID = ICON_CLEAR;
    else if (id == 801 || id == 802)
      weatherIconID = ICON_FEW_CLOUDS;
    else if (id == 803 || id == 804)
      weatherIconID = ICON_BROKEN_CLOUDS;
    else
      weatherIconID = ICON_UNKNOWN;
  }
  else
  {
    weatherDesc = "No Data";
    weatherIconID = ICON_UNKNOWN;
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
      lastSecond = getSecond();
      showColon = (lastSecond % 2 == 0);
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
    updateLocalTime();
    if (getSecond() != lastSecond)
    {
      lastSecond = getSecond();
      int h = getHour();
      bool isPM = (h >= 12);
      int h12 = (h % 12 == 0) ? 12 : (h % 12);
      showColon = (lastSecond % 2 == 0);
      static char clockStr[10];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", h12, getMinute());
      P.setTextBuffer(0, clockStr);
      P.displayReset(0);
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

    ClockRenderState clockState{};
    clockState.hour = getHour();
    clockState.minute = getMinute();
    clockState.second = getSecond();
    clockState.day = getDay();
    clockState.month = getMonth();
    clockState.weekday = getWeekday();
    clockState.showColon = showColon;
    clockState.mode = currentMode;
    clockState.currentFrame = currentFrame;
    for (int i = 0; i < 4; i++)
    {
      clockState.resolveFrame[i] = resolveFrame[i];
      clockState.scrambleIndices[i] = scrambleIndices[i];
      clockState.targetBitmaps[i] = targetBitmaps[i];
    }
    drawClock(clockState, DIGITS, LETTERS, DAY_MAP, screenBuffer, TOTAL_WIDTH);

    // Animate a bouncing dot along columns 8-15 (module 1) with gravity-like motion
    if (lastBounceTick == 0)
      if (lastBounceTick == 0)
      {
        lastBounceTick = nowMs;
        bounceVel = BOUNCE_LAUNCH_V;
      }

    unsigned long dtMs = nowMs - lastBounceTick;
    if (dtMs > BOUNCE_MAX_DT)
      dtMs = BOUNCE_MAX_DT;
    lastBounceTick = nowMs;

    float dt = dtMs / 1000.0f; // convert to seconds

    bounceVel += -BOUNCE_G_MAG * dt; // constant pull toward left
    bouncePos += bounceVel * dt;

    // Bounce off the ground (left wall)
    if (bouncePos < BOUNCE_START_COL)
    {
      bouncePos = BOUNCE_START_COL;
      bounceVel = BOUNCE_LAUNCH_V;
    }

    // Clamp at right wall just in case
    if (bouncePos > BOUNCE_END_COL)
      bouncePos = BOUNCE_END_COL;

    int col = int(lround(bouncePos));
    if (col < BOUNCE_START_COL)
      col = BOUNCE_START_COL;
    else if (col > BOUNCE_END_COL)
      col = BOUNCE_END_COL;

    screenBuffer[col] |= 0x40; // second row from bottom on rotated matrix

    int hourNow = getHour();
    bool isPM = (hourNow >= 12);

    screenBuffer[17] |= isPM ? 0x51 : 0x55;
    screenBuffer[16] |= isPM ? 0x77 : 0x72;

    mx->setColumn(17, screenBuffer[17]);
    mx->setColumn(16, screenBuffer[16]);

    for (int i = 0; i < 16; i++)
      mx->setColumn(i, screenBuffer[i]);
  }

  else if (currentPage == PAGE_WEATHER)
  {
    WeatherRenderState weatherState{};
    weatherState.data.valid = true;
    weatherState.data.id = weatherIconID;
    weatherState.data.desc = weatherDesc;
    weatherState.data.feelsLike = feelsLikeTemp;
    weatherState.iconId = weatherIconID;

    drawWeather(weatherState, WEATHER_ICONS, DIGITS, screenBuffer, TOTAL_WIDTH);

    // Push Module 4 and Module 0
    for (int i = 32; i < 40; i++)
      mx->setColumn(i, screenBuffer[i]);
    for (int i = 0; i < 8; i++)
      mx->setColumn(i, screenBuffer[i]);
  }
}
