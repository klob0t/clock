#pragma once
#include <Arduino.h>

// Date mode used across clock rendering and animations.
enum DateMode : uint8_t
{
  MODE_DATE,
  MODE_SCRAMBLE,
  MODE_DAY
};

// Placeholder for future bounce/scramble state and helpers.
struct BounceState
{
  float pos;
  float vel;
  unsigned long lastTick;
};
