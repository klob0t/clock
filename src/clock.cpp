#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>
#include <DS3232RTC.h>  // <--- Library Include
#include <TimeLib.h>    // <--- Time Library
#include "BitPotionFont.h"

// --- CONFIG ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 5

#ifdef IS_SIMULATION
  #define CS_PIN    10 
#else
  #define CS_PIN    5
#endif

// --- INSTANTIATE OBJECTS ---
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_MAX72XX *mx = P.getGraphicObject(); 

// +++ FIX: CREATE THE RTC OBJECT HERE +++
DS3232RTC RTC; 

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

const uint8_t DAY_MAP[8][3] = {
  {0,0,0},         // 0: Unused
  {12, 4, 14},     // 1: SUN (S, U, N)
  {0,  1, 14},     // 2: MON (M, O, N)
  {3,  4,  5},     // 3: TUE (T, U, E)
  {6,  5,  7},     // 4: WED (W, E, D)
  {3,  8,  4},     // 5: THU (T, H, U)
  {2, 10, 11},     // 6: FRI (F, R, I)
  {12, 13, 3}      // 7: SAT (S, A, T)
};

// --- GLOBALS ---
bool showColon = true;
unsigned long lastTick = 0;

// Animation State
int lineHead = 15;
unsigned long lastLineTick = 0;

// --- SCRAMBLE ENGINE ---
enum DateMode { MODE_DATE, MODE_SCRAMBLE, MODE_DAY };
DateMode currentMode = MODE_DATE;
DateMode nextMode = MODE_DAY; 
unsigned long modeTimer = 0;

uint64_t targetBitmaps[4]; 
int      resolveFrame[4];  
int      currentFrame = 0; 

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

void startScrambleTransition(DateMode destination) {
  currentMode = MODE_SCRAMBLE;
  nextMode = destination;
  currentFrame = 0;
  modeTimer = millis();

  // LIVE DATA FROM RTC
  int d = day();
  int m = month();
  int wd = weekday(); 

  if (destination == MODE_DATE) {
    targetBitmaps[0] = DIGITS[d / 10];
    targetBitmaps[1] = DIGITS[d % 10];
    targetBitmaps[2] = DIGITS[m / 10];
    targetBitmaps[3] = DIGITS[m % 10];
  } 
  else { // MODE_DAY
    targetBitmaps[0] = LETTERS[DAY_MAP[wd][0]];
    targetBitmaps[1] = LETTERS[DAY_MAP[wd][1]];
    targetBitmaps[2] = LETTERS[DAY_MAP[wd][2]];
    targetBitmaps[3] = 0x00; 
  }

  for (int i=0; i<4; i++) {
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

  // --- RTC SETUP ---
  RTC.begin();              
  setSyncProvider(RTC.get); 
  setSyncInterval(300);     

  if(timeStatus() != timeSet) {
      // Default time if RTC is fresh (Set to: 12:00:00, Jan 1, 2025)
      setTime(12, 0, 0, 1, 1, 2025); 
      RTC.set(now()); 
  }
  
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

    // --- 3. DRAW RIGHT SIDE ---
    int cursor = 15;
    
    int d = day();
    int m = month();
    int wd = weekday(); 

    for (int i=0; i<4; i++) {
        uint64_t imageToDraw = 0;
        
        if (currentMode == MODE_SCRAMBLE) {
           if (currentFrame < resolveFrame[i]) imageToDraw = LETTERS[random(0, 15)];
           else imageToDraw = targetBitmaps[i];
        } else {
           if (currentMode == MODE_DATE) {
             if (i==0) imageToDraw = DIGITS[d / 10];
             if (i==1) imageToDraw = DIGITS[d % 10];
             if (i==2) imageToDraw = DIGITS[m / 10];
             if (i==3) imageToDraw = DIGITS[m % 10];
           } else { 
             if (i==0) imageToDraw = LETTERS[DAY_MAP[wd][0]];
             if (i==1) imageToDraw = LETTERS[DAY_MAP[wd][1]];
             if (i==2) imageToDraw = LETTERS[DAY_MAP[wd][2]];
             if (i==3) imageToDraw = 0; 
           }
        }

        drawRawBitmap(cursor, imageToDraw);
        cursor -= 3; 
        
        mx->setColumn(cursor, 0x00); 
        
        if (i == 1 && (currentMode == MODE_DATE || (currentMode == MODE_SCRAMBLE && nextMode == MODE_DATE))) {
           cursor--; 
        } else {
           cursor--; 
        }
    }
    
    drawMovingLine();
  }

  // --- 4. SLOW CLOCK LOOP (500ms) ---
  if (currentMillis - lastTick >= 500) { 
      lastTick = currentMillis;
      showColon = !showColon;
      
      char clockStr[6];
      sprintf(clockStr, showColon ? "%02d:%02d" : "%02d %02d", hour(), minute());
      P.displayZoneText(0, clockStr, PA_LEFT, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayAnimate();
  }
}