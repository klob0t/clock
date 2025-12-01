#include "display_weather.h"

// Local helper for 5-pixel digits.
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

static void drawSpriteToBuffer(int startCol, int iconID, const uint8_t icons[][8], uint8_t *screenBuffer, int totalWidth)
{
  if (iconID < 0)
    return;
  for (int i = 0; i < 8; i++)
  {
    uint8_t byte = icons[iconID][i];
    int targetCol = startCol - i;
    if (targetCol >= 0 && targetCol < totalWidth)
    {
      screenBuffer[targetCol] = byte;
    }
  }
}

void drawWeather(const WeatherRenderState &state,
                 const uint8_t icons[][8],
                 const uint64_t *digits,
                 uint8_t *screenBuffer,
                 int totalWidth)
{
  // Draw icon on the rightmost module (cols 39..32)
  drawSpriteToBuffer(totalWidth - 1, state.iconId, icons, screenBuffer, totalWidth);

  // Draw temperature on the leftmost module (cols 7..0)
  int t = state.data.feelsLike;
  int tens = t / 10;
  int units = t % 10;

  // Tens at col 7 (width 3), yOffset 2 to center if >=10
  if (t >= 10)
    drawToBuffer(7, digits[tens], 3, 2, screenBuffer, totalWidth);
  // Units at col 3
  drawToBuffer(3, digits[units], 3, 2, screenBuffer, totalWidth);
  // Degree dot at top-left of module: bit 3 (row 2 from top)
  screenBuffer[0] |= 0x08;
}
