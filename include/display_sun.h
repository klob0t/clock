#pragma once
#include <Arduino.h>

struct SunRenderState
{
  unsigned long sunrise; // UTC epoch seconds
  unsigned long sunset;  // UTC epoch seconds
  unsigned long now;     // UTC epoch seconds
};

struct SunInfo
{
  bool hasData;
  bool targetIsSunrise;
  unsigned long diffSeconds;
  int hours;
  int minutes;
  uint8_t spriteIndex;
};

SunInfo computeSunInfo(const SunRenderState &state);

// Draw the 8x8 sun/moon sprite into screenBuffer (module 4).
void drawSun(const SunInfo &info,
             const uint64_t *icons,
             uint8_t *screenBuffer,
             int totalWidth);
