#pragma once
#include <Arduino.h>
#include "animations.h"

struct ClockRenderState
{
  int hour;
  int minute;
  int second;
  int day;
  int month;
  int weekday;
  bool showColon;
  DateMode mode;
  int currentFrame;
  int resolveFrame[4];
  int scrambleIndices[4];
  uint64_t targetBitmaps[4];
};

void drawClock(const ClockRenderState &state,
               const uint64_t *digits,
               const uint64_t *letters,
               const uint8_t dayMap[8][3],
               uint8_t *screenBuffer,
               int totalWidth);

void drawAMPM(bool isPM, int colLeft, uint8_t *screenBuffer);
