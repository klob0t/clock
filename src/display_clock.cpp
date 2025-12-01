#include "display_clock.h"
#include <Arduino.h>

// Local helper to write a 5-pixel tall glyph into the buffer.
static void drawToBuffer(int startCol, uint64_t image, int width, int yOffset, uint8_t *screenBuffer, int totalWidth)
{
  if (image == 0)
    return;

  uint8_t rows[5];
  rows[0] = (image >> 32) & 0xFF;
  rows[1] = (image >> 24) & 0xFF;
  rows[2] = (image >> 16) & 0xFF;
  rows[3] = (image >> 8) & 0xFF;
  rows[4] = (image >> 0) & 0xFF;

  for (int col = 0; col < width; col++)
  {
    uint8_t columnByte = 0;
    uint8_t b0 = (rows[0] >> col) & 0x01;
    uint8_t b1 = (rows[1] >> col) & 0x01;
    uint8_t b2 = (rows[2] >> col) & 0x01;
    uint8_t b3 = (rows[3] >> col) & 0x01;
    uint8_t b4 = (rows[4] >> col) & 0x01;

    columnByte |= (b0 << 4);
    columnByte |= (b1 << 3);
    columnByte |= (b2 << 2);
    columnByte |= (b3 << 1);
    columnByte |= (b4 << 0);

    int targetCol = startCol - col;
    if (targetCol >= 0 && targetCol < totalWidth)
      screenBuffer[targetCol] |= (columnByte << yOffset);
  }
}

void drawAMPM(bool isPM, int colLeft, uint8_t *screenBuffer)
{
  // 2x2-ish glyph: choose the left/right column bytes.
  screenBuffer[colLeft] |= isPM ? 0x51 : 0x55;
  screenBuffer[colLeft + 1] |= isPM ? 0x77 : 0x72;
}

void drawClock(const ClockRenderState &state,
               const uint64_t *digits,
               const uint64_t *letters,
               const uint8_t dayMap[8][3],
               uint8_t *screenBuffer,
               int totalWidth)
{
  // cursor starts at column 14 to match existing layout
  int cursor = 14;
  for (int i = 0; i < 4; i++)
  {
    uint64_t img = 0;
    int charWidth = 3;
    if (state.mode == MODE_SCRAMBLE)
    {
      if (state.currentFrame < state.resolveFrame[i])
      {
        img = letters[state.scrambleIndices[i]];
        charWidth = 3;
      }
      else
      {
        img = state.targetBitmaps[i];
        charWidth = 3;
      }
    }
    else if (state.mode == MODE_DATE)
    {
      int val = (i == 0) ? state.day / 10 : (i == 1) ? state.day % 10
                                  : (i == 2)        ? state.month / 10
                                                    : state.month % 10;
      img = digits[val];
      charWidth = 3;
    }
    else
    {
      int idx = (i < 3) ? dayMap[state.weekday][i] : -1;
      img = (idx >= 0) ? letters[idx] : 0;
      charWidth = 3;
    }
    drawToBuffer(cursor, img, charWidth, 0, screenBuffer, totalWidth);
    cursor -= charWidth;
    cursor -= 1;
  }
}
