#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include "BitPotionFont.h"

// --- CONFIG ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 5

#ifdef IS_SIMULATION
  #define CS_PIN    10 
#else
  #define CS_PIN    5
#endif

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX *mx = P.getGraphicObject(); 

// --- BITMAPS ---
const uint64_t DIGITS[] = {
  0x0000000007050507, 0x0000000002020302, 0x0000000007010407, 0x0000000007040607,
  0x0000000004060505, 0x0000000007040107, 0x0000000007050701, 0x0000000002020407,
  0x0000000007050207, 0x0000000004070507,
};

const uint64_t LETTERS[] = {
  0x0000000005050707, // 0: M
  0x0000000007050507, // 1: O
  0x0000000001030107, // 2: F
  0x0000000002020207, // 3: T
  0x0000000007050505, // 4: U
  0x0000000007010307, // 5: E
  0x0000000007070505, // 6: W
  0x0000000007050503, // 7: D
  0x0000000005050705, // 8: H
  0x0000000001030107, // 9: F
  0x0000000005030507, // 10: R
  0x0000000007020207, // 11: I
  0x0000000007040107, // 12: S
  0x0000000005070507, // 13: A
  0x0000000005050503  // 14: N
};

const uint8_t DAY_MAP[7][3] = {
  {12, 4, 14}, {0,  1, 14}, {3,  4,  5}, {6,  5,  7},
  {3,  8,  4}, {2, 10, 11}, {12, 13, 3}
};

// --- GLOBALS ---
uint8_t hours = 12; uint8_t minutes = 0; uint8_t seconds = 0;
uint8_t day = 24;   uint8_t month = 11;
uint8_t dayOfWeek = 5; 

bool showColon = true;
unsigned long lastTick = 0;

// Animation State
int lineHead = 15;
unsigned long lastLineTick = 0;

// --- SCRAMBLE ENGINE ---
enum DateMode { MODE_DATE, MODE_SCRAMBLE, MODE_DAY };
DateMode currentMode = MODE_DATE;
DateMode nextMode = MODE_DAY; // Where we are going after scramble
unsigned long modeTimer = 0;

// The "React-style" per-character state
uint64_t targetBitmaps[4]; // What the 4 slots will eventually show
int      resolveFrame[4];  // At which frame # does slot X stop scrambling?
int      currentFrame = 0; // Current animation frame counter

// --- HELPERS ---

void drawRawBitmap(int startCol, uint64_t image) {
  uint8_t rows[4];
  rows[0] = (image >> 24) & 0xFF; 
  rows[1] = (image >> 16) & 0xFF;
  rows[2] = (image >> 8)  & 0xFF;
  rows[3] = (image >> 0)  & 0xFF; 

  for (int col = 0; col < 3; col++) {
    uint8_t columnByte = 0;
    int bitIndex = col; 
    uint8_t b0 = (rows[0] >> bitIndex) & 1;
    uint8_t b1 = (rows[1] >> bitIndex) & 1;
    uint8_t b2 = (rows[2] >> bitIndex) & 1;
    uint8_t b3 = (rows[3] >> bitIndex) & 1;
    
    columnByte |= (b0 << 3); 
    columnByte |= (b1 << 2); 
    columnByte |= (b2 << 1);
    columnByte |= (b3 << 0); 

    mx->setColumn(startCol - col, columnByte);
  }
}

void drawMovingLine() {
  if (lineHead >= 0 && lineHead <= 15) mx->setPoint(5, lineHead, true);
  int tail = lineHead + 5;
  if (tail >= 0 && tail <= 15) mx->setPoint(5, tail, false);
}

void clearRightZone() {
  for(int i=0; i<=15; i++) mx->setColumn(i, 0x00);
}

// +++ NEW: Initialize the Scramble Sequence +++
void startScrambleTransition(DateMode destination) {
  currentMode = MODE_SCRAMBLE;
  nextMode = destination;
  currentFrame = 0;
  modeTimer = millis();

  // 1. Calculate TARGET Bitmaps (What we want to see at the end)
  if (destination == MODE_DATE) {
    targetBitmaps[0] = DIGITS[day / 10];
    targetBitmaps[1] = DIGITS[day % 10];
    targetBitmaps[2] = DIGITS[month / 10];
    targetBitmaps[3] = DIGITS[month % 10];
  } 
  else { // MODE_DAY
    // Note: Day is 3 chars, but we have 4 slots. We'll leave slot 3 empty or shift.
    // Let's Center it: Space, Char, Char, Char? 
    // Or just Left align: Char, Char, Char, Space.
    // Let's try: Char, Char, Char, Space (easiest logic)
    targetBitmaps[0] = LETTERS[DAY_MAP[dayOfWeek][0]];
    targetBitmaps[1] = LETTERS[DAY_MAP[dayOfWeek][1]];
    targetBitmaps[2] = LETTERS[DAY_MAP[dayOfWeek][2]];
    targetBitmaps[3] = 0x00; // Empty
  }

  // 2. Assign Random Resolve Times (The React Logic)
  // We want a fast effect. Let's say between 5 and 20 frames.
  for (int i=0; i<4; i++) {
    // Random length for each character creates the "cascading" effect
    resolveFrame[i] = random(5, 25); 
  }
}

void setup() {
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  P.begin(1); 
  P.setZone(0, 2, 4); 
  P.setIntensity(5);
  P.setFont(0, BitPotion);
  P.setTextAlignment(0, PA_LEFT); 
  
  modeTimer = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- 1. STATE SWITCHER ---
  if (currentMode != MODE_SCRAMBLE) {
    unsigned long holdTime = (currentMode == MODE_DATE) ? 5000 : 3000;
    if (currentMillis - modeTimer > holdTime) {
      DateMode dest = (currentMode == MODE_DATE) ? MODE_DAY : MODE_DATE;
      startScrambleTransition(dest);
    }
  }

  // --- 2. FAST UPDATE (50ms) ---
  if (currentMillis - lastLineTick >= 50) {
    lastLineTick = currentMillis;
    
    // Line Animation
    lineHead--;
    if (lineHead < -5) lineHead = 15;
    
    // Draw Line (Calculated here, but we rely on the redraw below to actually show it)
    // We don't need to call drawMovingLine() here because the section below does it every 50ms anyway.

    // +++ SCRAMBLE LOGIC +++
    if (currentMode == MODE_SCRAMBLE) {
       currentFrame++; 
       bool allDone = true;
       for(int i=0; i<4; i++) {
         if (currentFrame < resolveFrame[i]) allDone = false;
       }
       if (allDone) {
         currentMode = nextMode;
         modeTimer = currentMillis; 
       }
    }

    // --- 3. DRAW RIGHT SIDE (Every 50ms) ---
    // We moved this OUT of the slow timer so the scramble looks smooth.
    // REMOVED: clearRightZone(); -> This caused the blinking!
    
    int cursor = 15;
     
    // LOOP THROUGH THE 4 SLOTS
    for (int i=0; i<4; i++) {
        uint64_t imageToDraw = 0;
        
        // Decide what to draw
        if (currentMode == MODE_SCRAMBLE) {
           if (currentFrame < resolveFrame[i]) imageToDraw = LETTERS[random(0, 15)];
           else imageToDraw = targetBitmaps[i];
        } else {
           if (currentMode == MODE_DATE) {
             if (i==0) imageToDraw = DIGITS[day / 10];
             if (i==1) imageToDraw = DIGITS[day % 10];
             if (i==2) imageToDraw = DIGITS[month / 10];
             if (i==3) imageToDraw = DIGITS[month % 10];
           } else { 
             if (i==0) imageToDraw = LETTERS[DAY_MAP[dayOfWeek][0]];
             if (i==1) imageToDraw = LETTERS[DAY_MAP[dayOfWeek][1]];
             if (i==2) imageToDraw = LETTERS[DAY_MAP[dayOfWeek][2]];
             if (i==3) imageToDraw = 0; 
           }
        }

        // Draw the Character
        drawRawBitmap(cursor, imageToDraw);
        cursor -= 3; // Move past the 3-pixel character
        
        // +++ THE ANTI-FLICKER FIX +++
        // Instead of clearing the whole screen, we just wipe the 1-pixel gap here.
        // This overwrites any old "ghost" pixels with a clean space.
        mx->setColumn(cursor, 0x00); 
        
        // Special case: Add Separator for Date
        if (i == 1 && (currentMode == MODE_DATE || (currentMode == MODE_SCRAMBLE && nextMode == MODE_DATE))) {
           // We are at the gap between Day and Month. 
           // We just wrote 0x00 above. Now let's writing the Space + Dot if needed.
           // Actually, we need to be careful with cursor math.
           
           // Current cursor is at the gap.
           cursor--; // Move past the gap
           // (This naturally creates the wider spacing for the date)
        } else {
           cursor--; // Standard tight spacing for letters
        }
    }
    
    // Update Line Visualization
    drawMovingLine();
  }

  // --- 4. SLOW CLOCK LOOP (500ms) ---
  if (currentMillis - lastTick >= 500) { // Keep clock separate
      lastTick = currentMillis;
      showColon = !showColon;
      if (showColon) {
        seconds++;
        if (seconds > 59) { seconds = 0; minutes++; if (minutes > 59) { minutes = 0; hours++; if (hours > 23) hours = 0; }}
      }
      
      char clockStr[6];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", hours, minutes);
      P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayAnimate();
  }
}