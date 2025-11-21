#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>

// --- CONFIG ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 5

// --- PINS ---
#ifdef IS_SIMULATION
  #define CS_PIN    10 
#else
  #define CS_PIN    5
#endif

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// --- SCROLLING SETTINGS ---
// Speed: Lower is faster (milliseconds between updates)
// In Proteus, 50-100 is usually a safe sweet spot.
#define SCROLL_SPEED 30
#define PAUSE_TIME   0

void setup() {
  // 1. Proteus Stability Fix (Don't remove this!)
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  P.begin();
  P.setIntensity(5); // The fix! (0-15)

  // 2. Setup the Animation
  // Text: "Hello World"
  // Alignment: PA_CENTER
  // Speed: 100ms
  // Pause: 0ms
  // Effect IN: Scroll from Right to Left
  // Effect OUT: Continue Scrolling Left
  P.displayText("Hello World", PA_CENTER, SCROLL_SPEED, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loop() {
  // 3. The Animation Engine
  // P.displayAnimate() returns 'true' when the animation finishes (text disappears off-screen)
  if (P.displayAnimate()) {
    // Reset to start over
    P.displayReset();
  }
}