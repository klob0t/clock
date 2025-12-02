#include "display_scope.h"

void drawScope(const ScopeRenderState &state,
               uint8_t *screenBuffer,
               int totalWidth)
{
  if (!state.samples || state.sampleCount <= 0)
    return;

  int columns = totalWidth;
  for (int x = 0; x < columns; x++)
  {
    int idx = (x * state.sampleCount) / columns;
    if (idx < 0)
      idx = 0;
    if (idx >= state.sampleCount)
      idx = state.sampleCount - 1;

    uint8_t sample = state.samples[idx];
    // Map 0-255 -> row 0..7 (row 0 = top). Clamp just in case.
    if (sample > 255)
      sample = 255;
    uint8_t row = 7 - ((sample * 7) / 255);
    if (row > 7)
      row = 7;

    screenBuffer[x] |= (1 << row);
  }

  if (!state.hasData)
  {
    // Draw a mid-line to show activity even when no data.
    for (int x = 0; x < columns; x++)
      screenBuffer[x] |= (1 << 3);
  }
}
