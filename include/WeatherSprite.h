#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include "time.h"
#include "BitPotionFont.h"

// --- WI-FI SETTINGS ---
const char* ssid     = "12";
const char* password = "p00chiz!";

// --- TIME SETTINGS (NTP) ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;      // UTC +7 (Jakarta)
const int   daylightOffset_sec = 0;

// --- DISPLAY SETTINGS ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   5
#define CS_PIN        5        

MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX* mx; 

// The Manual HUD area is the 2 right-most modules (Cols 0-15)
static const int HUD_WIDTH = 16;
static const int HUD_RIGHT_MAX_COL = 15;

// Screen Buffer (To mix Clouds and Text without erasing each other)
uint8_t hudBuffer[HUD_WIDTH]; 

// -----------------------------------------------------------------------------
// BITMAPS
// -----------------------------------------------------------------------------
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

// --- CLOUD BITMAPS ---
// These are simple 2-row shapes
const uint8_t CLOUD_SHAPES[][4] = {
  {0x02, 0x03, 0x03, 0x02}, // Cloud 1
  {0x00, 0x02, 0x03, 0x01}, // Cloud 2
  {0x01, 0x03, 0x03, 0x02}, // Cloud 3
  {0x02, 0x03, 0x01, 0x00}, // Cloud 4
  {0x03, 0x03, 0x03, 0x03}  // Cloud 5 (Long)
};
const uint8_t CLOUD_LENS[] = { 4, 4, 4, 4, 4 };
const int NUM_CLOUDS = 5;

// -----------------------------------------------------------------------------
// STATE VARIABLES
// -----------------------------------------------------------------------------
bool showColon = true;
unsigned long lastClockTick = 0;

int lineHead = HUD_RIGHT_MAX_COL;
unsigned long lastLineTick = 0;

enum DateMode : uint8_t { MODE_DATE, MODE_SCRAMBLE, MODE_DAY };
DateMode currentMode = MODE_DATE;
DateMode nextMode    = MODE_DAY;
unsigned long modeTimer = 0;

uint64_t targetBitmaps[4]; 
int resolveFrame[4];   
int currentFrame = 0;

// Cloud Animation
float cloudPos = 0.0;          
unsigned long lastCloudTick = 0;
const int CLOUD_DELAY = 150;   // Higher = Slower
const float CLOUD_SPEED = 0.2; 

// -----------------------------------------------------------------------------
// TIME HELPERS
// -----------------------------------------------------------------------------
struct tm timeinfo;
void updateLocalTime() { getLocalTime(&timeinfo); }
int getDay() { return timeinfo.tm_mday; }
int getMonth() { return timeinfo.tm_mon + 1; }
int getWeekday() { return timeinfo.tm_wday + 1; }
int getHour() { return timeinfo.tm_hour; }
int getMinute() { return timeinfo.tm_min; }

// -----------------------------------------------------------------------------
// DRAWING HELPERS (BUFFER BASED)
// -----------------------------------------------------------------------------

// This function uses YOUR EXACT original bit logic, but writes to a buffer 
// instead of the screen directly. This prevents flipping.
void drawToBuffer(int startCol, uint64_t image)
{
  if (image == 0) return;

  uint8_t rows[4];
  for (int r = 0; r < 4; r++) {
    rows[r] = (image >> (8 * (3 - r))) & 0xFF; 
  }

  for (int col = 0; col < 3; col++) {
    uint8_t columnByte = 0;

    // YOUR ORIGINAL LOGIC PRESERVED:
    uint8_t b0 = (rows[0] >> col) & 0x01; 
    uint8_t b1 = (rows[1] >> col) & 0x01; 
    uint8_t b2 = (rows[2] >> col) & 0x01; 
    uint8_t b3 = (rows[3] >> col) & 0x01; 

    columnByte |= (b0 << 3);
    columnByte |= (b1 << 2);
    columnByte |= (b2 << 1);
    columnByte |= (b3 << 0);

    // Apply to buffer using OR (|=) so we don't erase clouds
    int targetIndex = startCol - col;
    if (targetIndex >= 0 && targetIndex < HUD_WIDTH) {
      hudBuffer[targetIndex] |= columnByte;
    }
  }
}

void drawCloudsToBuffer() {
  // Only draw clouds in AM (00:00 - 11:59)
  if (getHour() >= 12) return;

  int currentOffset = 0; 
  for(int i=0; i<NUM_CLOUDS; i++) {
    float pos = cloudPos + currentOffset;
    
    // Wrap around logic
    while(pos >= HUD_WIDTH) pos -= (HUD_WIDTH + 6);

    int startCol = (int)pos;
    int len = CLOUD_LENS[i];

    for(int c=0; c<len; c++) {
      int idx = startCol - c; // Drawing right-to-left direction style
      
      // Wrap index for seamless scrolling
      if (idx < 0) idx += HUD_WIDTH;
      if (idx >= HUD_WIDTH) idx -= HUD_WIDTH;

      if (idx >= 0 && idx < HUD_WIDTH) {
        // Shift cloud bits to the TOP (Bits 6 and 7)
        // Your text is in Bits 0-3. Clouds go in Bits 6-7.
        uint8_t cloudBits = CLOUD_SHAPES[i][c]; 
        hudBuffer[idx] |= (cloudBits << 6); 
      }
    }
    currentOffset += len + 3; // Spacing
  }
}

// -----------------------------------------------------------------------------
// UPDATE LOOPS
// -----------------------------------------------------------------------------

void updateHudFast(unsigned long nowMs)
{
  // 1. Clear the buffer completely
  memset(hudBuffer, 0, HUD_WIDTH);

  // 2. Update Timers
  if (nowMs - lastLineTick > 50) {
    lastLineTick = nowMs;
    lineHead--;
    if (lineHead < -5) lineHead = HUD_RIGHT_MAX_COL;
  }
  if (nowMs - lastCloudTick > CLOUD_DELAY) {
    lastCloudTick = nowMs;
    cloudPos += CLOUD_SPEED; 
    if(cloudPos > 1000.0) cloudPos -= (HUD_WIDTH + 6);
  }

  // 3. Scramble Logic
  if (currentMode == MODE_SCRAMBLE) {
    currentFrame++;
    bool allDone = true;
    for (int i = 0; i < 4; i++) {
      if (currentFrame < resolveFrame[i]) { allDone = false; break; }
    }
    if (allDone) { currentMode = nextMode; modeTimer = nowMs; }
  }

  updateLocalTime();

  // 4. Draw CLOUDS to Buffer
  drawCloudsToBuffer();

  // 5. Draw TEXT to Buffer
  int d = getDay(); int m = getMonth(); int wd = getWeekday();
  int cursor = HUD_RIGHT_MAX_COL;

  for (int i = 0; i < 4; i++) {
    uint64_t imageToDraw = 0;

    if (currentMode == MODE_SCRAMBLE) {
      if (currentFrame < resolveFrame[i]) imageToDraw = LETTERS[random(0, 15)];
      else imageToDraw = targetBitmaps[i];
    } else if (currentMode == MODE_DATE) {
      if (i == 0) imageToDraw = DIGITS[d / 10];
      if (i == 1) imageToDraw = DIGITS[d % 10];
      if (i == 2) imageToDraw = DIGITS[m / 10];
      if (i == 3) imageToDraw = DIGITS[m % 10];
    } else { // MODE_DAY
      if (i == 0) imageToDraw = LETTERS[DAY_MAP[wd][0]];
      if (i == 1) imageToDraw = LETTERS[DAY_MAP[wd][1]];
      if (i == 2) imageToDraw = LETTERS[DAY_MAP[wd][2]];
      if (i == 3) imageToDraw = 0x00;
    }

    drawToBuffer(cursor, imageToDraw);
    cursor -= 3;
    // Note: We don't setColumn(0) here because we want to preserve clouds in the gap
    cursor--; 
  }

  // 6. Push Buffer to Display
  for(int i=0; i<HUD_WIDTH; i++) {
    // Write the combined (Text + Cloud) byte to the column
    mx->setColumn(i, hudBuffer[i]);
  }
}

void startScrambleTransition(DateMode destination)
{
  currentMode  = MODE_SCRAMBLE;
  nextMode     = destination;
  currentFrame = 0;
  modeTimer    = millis();

  updateLocalTime(); 
  int d  = getDay();
  int m  = getMonth();
  int wd = getWeekday();

  if (destination == MODE_DATE) {
    targetBitmaps[0] = DIGITS[d / 10];
    targetBitmaps[1] = DIGITS[d % 10];
    targetBitmaps[2] = DIGITS[m / 10];
    targetBitmaps[3] = DIGITS[m % 10];
  } else { 
    targetBitmaps[0] = LETTERS[DAY_MAP[wd][0]];
    targetBitmaps[1] = LETTERS[DAY_MAP[wd][1]];
    targetBitmaps[2] = LETTERS[DAY_MAP[wd][2]];
    targetBitmaps[3] = 0x00;
  }
  for (int i = 0; i < 4; i++) resolveFrame[i] = random(5, 25); 
}

void updateModeSwitcher(unsigned long nowMs)
{
  if (currentMode == MODE_SCRAMBLE) return;
  unsigned long holdTime = (currentMode == MODE_DATE) ? 5000UL : 3000UL;
  if (nowMs - modeTimer > holdTime) {
    DateMode dest = (currentMode == MODE_DATE) ? MODE_DAY : MODE_DATE;
    startScrambleTransition(dest);
  }
}

void updateClockSlow(unsigned long nowMs)
{
  if (nowMs - lastClockTick < 500) return;
  lastClockTick = nowMs;
  updateLocalTime();

  showColon = !showColon;
  char clockStr[6];
  sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", getHour(), getMinute());
  P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
}

// -----------------------------------------------------------------------------
// SETUP & LOOP
// -----------------------------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  P.begin(1);           
  P.setZone(0, 2, 4);   
  P.setIntensity(1);
  P.setTextAlignment(0, PA_LEFT);
  P.setFont(0, BitPotion);

  mx = P.getGraphicObject();
  mx->control(0, MD_MAX72XX::INTENSITY, 1);
  mx->control(1, MD_MAX72XX::INTENSITY, 1);

  // WIFI
  P.print("WIFI...");
  WiFi.begin(ssid, password);
  int dotCount = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(dotCount == 0) P.print("WIFI.");
    else if(dotCount == 1) P.print("WIFI..");
    else if(dotCount == 2) P.print("WIFI...");
    dotCount = (dotCount + 1) % 3;
    Serial.print(".");
  }

  // TIME
  P.print("SYNC");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    P.print("WAIT..");
    delay(500);
  }

  P.displayClear();
  Serial.println("\nTime Synced!");
  modeTimer = millis();
}

void loop()
{
  unsigned long nowMs = millis();
  updateModeSwitcher(nowMs);
  updateHudFast(nowMs);
  updateClockSlow(nowMs);
}