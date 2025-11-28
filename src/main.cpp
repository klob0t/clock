#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Adjust if needed, but this matches your project:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   5
#define CS_PIN        5        

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
