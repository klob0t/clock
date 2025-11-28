// #include <Arduino.h>
// #include <MD_Parola.h>
// #include <MD_MAX72XX.h>
// #include <SPI.h>
// #include <DS3232RTC.h>  // RTC
// #include <TimeLib.h>    // Time
// #include "BitPotionFont.h"
// #include "MilfordFont.h"
// #include "WeatherSprite.h"
// // Adjust if needed, but this matches your project:
// #define HARDWARE_TYPE MD_MAX72XX::FC16_HW
// #define MAX_DEVICES   5
// #define CS_PIN        5        

// static const int HUD_RIGHT_MAX_COL = 15;

// // -----------------------------------------------------------------------------
// // OBJECTS
// // -----------------------------------------------------------------------------
// MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// MD_MAX72XX* mx = P.getGraphicObject();

// DS3232RTC RTC;

// // -----------------------------------------------------------------------------
// // BITMAPS (3x4 pixels packed into low 32 bits of uint64_t)
// // -----------------------------------------------------------------------------
// const uint64_t DIGITS[] = {
//   0x0000000007050507,  // 0
//   0x0000000002020302,  // 1
//   0x0000000007010407,  // 2
//   0x0000000007040607,  // 3
//   0x0000000004060505,  // 4
//   0x0000000007040107,  // 5
//   0x0000000007050701,  // 6
//   0x0000000002020407,  // 7
//   0x0000000007050207,  // 8
//   0x0000000004070507   // 9
// };

// const uint64_t LETTERS[] = {
//   0x0000000005050707, //  0: M
//   0x0000000007050507, //  1: O
//   0x0000000001030107, //  2: F
//   0x0000000002020207, //  3: T
//   0x0000000007050505, //  4: U
//   0x0000000007010307, //  5: E
//   0x0000000007070505, //  6: W
//   0x0000000007050503, //  7: D
//   0x0000000005050705, //  8: H
//   0x0000000001030107, //  9: F
//   0x0000000005030507, // 10: R
//   0x0000000007020207, // 11: I
//   0x0000000007040107, // 12: S
//   0x0000000005070507, // 13: A
//   0x0000000005050503  // 14: N
// };

// // weekday() -> indices into LETTERS[]
// const uint8_t DAY_MAP[8][3] = {
//   {0, 0, 0},        // 0: unused
//   {12,  4, 14},     // 1: SUN
//   { 0,  1, 14},     // 2: MON
//   { 3,  4,  5},     // 3: TUE
//   { 6,  5,  7},     // 4: WED
//   { 3,  8,  4},     // 5: THU
//   { 2, 10, 11},     // 6: FRI
//   {12, 13,  3}      // 7: SAT
// };

// // -----------------------------------------------------------------------------
// // STATE
// // -----------------------------------------------------------------------------
// bool showColon       = true;
// unsigned long lastClockTick = 0;

// int  lineHead        = HUD_RIGHT_MAX_COL;
// unsigned long lastLineTick  = 0;

// // Scramble engine
// enum DateMode : uint8_t { MODE_DATE, MODE_SCRAMBLE, MODE_DAY };

// DateMode currentMode = MODE_DATE;
// DateMode nextMode    = MODE_DAY;
// unsigned long modeTimer = 0;

// uint64_t targetBitmaps[4];  // per glyph
// int      resolveFrame[4];   // frame index when each glyph resolves
// int      currentFrame = 0;

// // -----------------------------------------------------------------------------
// // LOW-LEVEL DRAW HELPERS
// // -----------------------------------------------------------------------------

// // Draw a 3x4 bitmap (DIGITS / LETTERS) ending at startCol, in rows 3..6.
// void drawRawBitmap(int startCol, uint64_t image)
// {
//   // If image is zero, just clear the 3 columns and return
//   if (image == 0) {
//     for (int col = 0; col < 3; col++) {
//       mx->setColumn(startCol - col, 0x00);
//     }
//     return;
//   }

//   uint8_t rows[4];
//   // rows 0..3 correspond to display rows 3..6 (top..bottom)
//   for (int r = 0; r < 4; r++) {
//     rows[r] = (image >> (8 * (3 - r))) & 0xFF;  // keep same visual orientation
//   }

//   for (int col = 0; col < 3; col++) {
//     uint8_t columnByte = 0;

//     uint8_t b0 = (rows[0] >> col) & 0x01; // map to bit3
//     uint8_t b1 = (rows[1] >> col) & 0x01; // bit2
//     uint8_t b2 = (rows[2] >> col) & 0x01; // bit1
//     uint8_t b3 = (rows[3] >> col) & 0x01; // bit0

//     columnByte |= (b0 << 3);
//     columnByte |= (b1 << 2);
//     columnByte |= (b2 << 1);
//     columnByte |= (b3 << 0);

//     mx->setColumn(startCol - col, columnByte);
//   }
// }

// void drawMovingLine()
// {
//   // Head and tail on row 5 in right block (0..15)
//   if (lineHead >= 0 && lineHead <= HUD_RIGHT_MAX_COL)
//     mx->setPoint(5, lineHead, true);

//   int tail = lineHead + 5;
//   if (tail >= 0 && tail <= HUD_RIGHT_MAX_COL)
//     mx->setPoint(5, tail, false);
// }

// // -----------------------------------------------------------------------------
// // SCRAMBLE ENGINE
// // -----------------------------------------------------------------------------
// void startScrambleTransition(DateMode destination)
// {
//   currentMode  = MODE_SCRAMBLE;
//   nextMode     = destination;
//   currentFrame = 0;
//   modeTimer    = millis();

//   int d  = day();
//   int m  = month();
//   int wd = weekday();

//   if (destination == MODE_DATE) {
//     targetBitmaps[0] = DIGITS[d / 10];
//     targetBitmaps[1] = DIGITS[d % 10];
//     targetBitmaps[2] = DIGITS[m / 10];
//     targetBitmaps[3] = DIGITS[m % 10];
//   } else { // MODE_DAY
//     targetBitmaps[0] = LETTERS[DAY_MAP[wd][0]];
//     targetBitmaps[1] = LETTERS[DAY_MAP[wd][1]];
//     targetBitmaps[2] = LETTERS[DAY_MAP[wd][2]];
//     targetBitmaps[3] = 0x00;
//   }

//   for (int i = 0; i < 4; i++) {
//     resolveFrame[i] = random(5, 25);  // when each glyph locks in
//   }
// }

// // -----------------------------------------------------------------------------
// // RIGHT-SIDE HUD UPDATE (DATE / DAY / SCRAMBLE + LINE)
// // -----------------------------------------------------------------------------
// void updateHudFast(unsigned long nowMs)
// {
//   if (nowMs - lastLineTick < 50) return;
//   lastLineTick = nowMs;

//   // Move line
//   lineHead--;
//   if (lineHead < -5) lineHead = HUD_RIGHT_MAX_COL;

//   // Scramble timing
//   if (currentMode == MODE_SCRAMBLE) {
//     currentFrame++;
//     bool allDone = true;
//     for (int i = 0; i < 4; i++) {
//       if (currentFrame < resolveFrame[i]) {
//         allDone = false;
//         break;
//       }
//     }
//     if (allDone) {
//       currentMode = nextMode;
//       modeTimer   = nowMs;
//     }
//   }

//   // Draw four 3x4 glyphs from right to left, starting at column 15
//   int d  = day();
//   int m  = month();
//   int wd = weekday();
//   int cursor = HUD_RIGHT_MAX_COL;

//   for (int i = 0; i < 4; i++) {
//     uint64_t imageToDraw = 0;

//     if (currentMode == MODE_SCRAMBLE) {
//       if (currentFrame < resolveFrame[i]) {
//         // still scrambling
//         imageToDraw = LETTERS[random(0, 15)];
//       } else {
//         imageToDraw = targetBitmaps[i];
//       }
//     } else if (currentMode == MODE_DATE) {
//       // DDMM
//       if (i == 0) imageToDraw = DIGITS[d / 10];
//       if (i == 1) imageToDraw = DIGITS[d % 10];
//       if (i == 2) imageToDraw = DIGITS[m / 10];
//       if (i == 3) imageToDraw = DIGITS[m % 10];
//     } else { // MODE_DAY
//       if (i == 0) imageToDraw = LETTERS[DAY_MAP[wd][0]];
//       if (i == 1) imageToDraw = LETTERS[DAY_MAP[wd][1]];
//       if (i == 2) imageToDraw = LETTERS[DAY_MAP[wd][2]];
//       if (i == 3) imageToDraw = 0x00;
//     }

//     drawRawBitmap(cursor, imageToDraw);
//     cursor -= 3;        // glyph width
//     mx->setColumn(cursor, 0x00); // spacing
//     cursor--;           // move to next glyph position
//   }

//   drawMovingLine();
// }

// // -----------------------------------------------------------------------------
// // MODE SWITCHER
// // -----------------------------------------------------------------------------
// void updateModeSwitcher(unsigned long nowMs)
// {
//   if (currentMode == MODE_SCRAMBLE) return;

//   unsigned long holdTime = (currentMode == MODE_DATE) ? 5000UL : 3000UL;
//   if (nowMs - modeTimer > holdTime) {
//     DateMode dest = (currentMode == MODE_DATE) ? MODE_DAY : MODE_DATE;
//     startScrambleTransition(dest);
//   }
// }

// // -----------------------------------------------------------------------------
// // CLOCK DISPLAY UPDATE (ZONE 0)
// // -----------------------------------------------------------------------------
// void updateClockSlow(unsigned long nowMs)
// {
//   if (nowMs - lastClockTick < 500) return;
//   lastClockTick = nowMs;

//   showColon = !showColon;

//   char clockStr[6];
//   sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", hour(), minute());

//   P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
//   P.displayAnimate();
// }

// // -----------------------------------------------------------------------------
// // SETUP / LOOP
// // -----------------------------------------------------------------------------
// void setup()
// {
//   SPI.setClockDivider(SPI_CLOCK_DIV16);

//   P.begin(1);           // one zone
//   P.setZone(0, 2, 4);   // zone 0 uses modules 2..4 for big clock
//   P.setIntensity(1);
//   P.setFont(0, BitPotion);
//   P.setTextAlignment(0, PA_LEFT);

//   int tinyBrightness = 1;

//   mx->control(0, MD_MAX72XX::INTENSITY, tinyBrightness);
//   mx->control(1, MD_MAX72XX::INTENSITY, tinyBrightness);

//   // RTC + time sync
//   RTC.begin();
//   setSyncProvider(RTC.get);
//   setSyncInterval(300);

//   if (timeStatus() != timeSet) {
//     // Default time: 12:00:00, Jan 1, 2025
//     setTime(12, 0, 0, 1, 1, 2025);
//     RTC.set(now());
//   }

//   modeTimer = millis();
// }

// void loop()
// {
//   unsigned long nowMs = millis();

//   updateModeSwitcher(nowMs);
//   updateHudFast(nowMs);
//   updateClockSlow(nowMs);
// }


#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Adjust if needed, but this matches your project:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   5         // 5 x 8x8 = 8x40
#define CS_PIN        19        // ESP32 GPIO5 -> CS

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX* mx;

void testAllOnOff() {
  // All on
  P.displayClear();
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8 * MAX_DEVICES; col++) {
      mx->setPoint(row, col, true);
    }
  }
  delay(1000);

  // All off
  P.displayClear();
  delay(500);

  // Blink a couple times
  for (int i = 0; i < 3; i++) {
    for (uint8_t row = 0; row < 8; row++) {
      for (uint8_t col = 0; col < 8 * MAX_DEVICES; col++) {
        mx->setPoint(row, col, true);
      }
    }
    delay(300);
    P.displayClear();
    delay(300);
  }
}

void testBrightnessSweep() {
  P.displayClear();
  P.print("BRITE");
  delay(500);

  for (uint8_t b = 0; b <= 15; b++) {
    P.setIntensity(b);
    delay(150);
  }

  for (int b = 15; b >= 0; b--) {
    P.setIntensity(b);
    delay(150);
  }

  P.displayClear();
}

void testMovingDot() {
  P.displayClear();
  uint8_t width = 8 * MAX_DEVICES;

  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < width; col++) {
      P.displayClear();
      mx->setPoint(row, col, true);
      delay(10);
    }
  }

  P.displayClear();
}

void testScrollText() {
  P.displayClear();
  P.setTextAlignment(PA_LEFT);
  P.print("HELLO");
  delay(1000);

  P.displayClear();
  P.displayText("ESP32 CLOCK", PA_CENTER, 80, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  while (!P.displayAnimate()) {
    // let Parola handle the animation
  }

  P.displayClear();
}

void setup() {
  Serial.begin(115200);

  P.begin();
  P.setIntensity(5);       // mid brightness
  P.displayClear();

  mx = P.getGraphicObject();   // for direct pixel tests

  delay(500);
}

void loop() {
  testAllOnOff();      // check every LED lights
  delay(500);


  testMovingDot();     // verify orientation, all columns/rows
  delay(500);

}

