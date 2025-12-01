#pragma once
#include <Arduino.h>
#include "weather_service.h"

struct WeatherRenderState
{
  WeatherData data;
  int iconId;
};

// Draw weather icon and temperature into screenBuffer. Does not clear the buffer.
void drawWeather(const WeatherRenderState &state,
                 const uint8_t icons[][8],
                 const uint64_t *digits,
                 uint8_t *screenBuffer,
                 int totalWidth);
