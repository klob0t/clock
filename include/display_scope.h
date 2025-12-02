#pragma once
#include <Arduino.h>

struct ScopeRenderState
{
  const uint8_t *samples;
  int sampleCount;
  bool hasData;
};

// Draw oscilloscope samples (0-255 amplitude) across the 40-column buffer.
void drawScope(const ScopeRenderState &state,
               uint8_t *screenBuffer,
               int totalWidth);
