#include "display_sun.h"
#include <math.h>

static uint8_t clampIndex(uint8_t val, uint8_t maxVal)
{
  return (val > maxVal) ? maxVal : val;
}

SunInfo computeSunInfo(const SunRenderState &state)
{
  SunInfo info{};
  info.hasData = (state.sunrise > 0 && state.sunset > 0);
  if (!info.hasData)
  {
    info.spriteIndex = 7; // horizon fallback
    return info;
  }

  unsigned long nowSec = state.now;
  unsigned long sunrise = state.sunrise;
  unsigned long sunset = state.sunset;

  bool isNight = (nowSec < sunrise) || (nowSec >= sunset);
  unsigned long target = 0;
  unsigned long span = 1;

  if (isNight)
  {
    // Determine the next sunrise and previous sunset to get night span.
    unsigned long nextSunrise = (nowSec < sunrise) ? sunrise : sunrise + 86400UL;
    unsigned long prevSunset = (nowSec < sunrise) ? (sunset - 86400UL) : sunset;
    span = (nextSunrise > prevSunset) ? (nextSunrise - prevSunset) : 1;
    unsigned long elapsed = (nowSec > prevSunset) ? (nowSec - prevSunset) : 0;
    float progress = (span > 0) ? ((float)elapsed / (float)span) : 0.0f;
    if (progress < 0.0f)
      progress = 0.0f;
    else if (progress > 1.0f)
      progress = 1.0f;
    // Map night progress to moon indices 8..14 (top at ~mid-night).
    float height = 1.0f - fabsf(progress - 0.5f) * 2.0f; // 0 at horizon, 1 at mid
    uint8_t step = clampIndex((uint8_t)(height * 6.0f + 0.5f), 6);
    info.spriteIndex = 14 - step; // 14=horizon-ish, 8=top
    target = nextSunrise;
    info.targetIsSunrise = true;
  }
  else
  {
    // Daytime: sunrise..sunset
    span = (sunset > sunrise) ? (sunset - sunrise) : 1;
    unsigned long elapsed = nowSec - sunrise;
    float progress = (span > 0) ? ((float)elapsed / (float)span) : 0.0f;
    if (progress < 0.0f)
      progress = 0.0f;
    else if (progress > 1.0f)
      progress = 1.0f;
    float height = 1.0f - fabsf(progress - 0.5f) * 2.0f; // high at mid-day
    uint8_t step = clampIndex((uint8_t)(height * 6.0f + 0.5f), 6);
    info.spriteIndex = 7 - step; // 7=horizon, 1=high, 0=highest
    target = sunset;
    info.targetIsSunrise = false;
  }

  unsigned long diff = (target > nowSec) ? (target - nowSec) : 0;
  info.diffSeconds = diff;
  info.hours = diff / 3600;
  info.minutes = (diff % 3600) / 60;
  return info;
}

void drawSun(const SunInfo &info,
             const uint64_t *icons,
             uint8_t *screenBuffer,
             int totalWidth)
{
  uint8_t idx = info.spriteIndex;
  if (idx > 14)
    idx = 14;

  uint64_t img = icons[idx];
  // Extract rows from MSB to LSB so the pattern isn't rotated.
  uint8_t rows[8];
  for (int r = 0; r < 8; r++)
    rows[r] = (img >> (56 - 8 * r)) & 0xFF;

  // Transpose rows->columns for the display orientation (top row -> bit7).
  uint8_t cols[8] = {0};
  for (int c = 0; c < 8; c++)
  {
    uint8_t colByte = 0;
    for (int r = 0; r < 8; r++)
    {
      uint8_t bit = (rows[r] >> (7 - c)) & 0x01;
      if (bit)
        colByte |= (1 << (7 - r));
    }
    cols[c] = colByte;
  }

  for (int i = 0; i < 8; i++)
  {
    int targetCol = totalWidth - 1 - i; // module 4
    if (targetCol >= 0 && targetCol < totalWidth)
      screenBuffer[targetCol] = cols[i];
  }
}
