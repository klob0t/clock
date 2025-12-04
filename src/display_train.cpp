#include "display_train.h"

// Simple 8x8 train icon (rightmost module).
static const uint8_t TRAIN_ICON[8] = {
    // Columns ordered like weather icons (no extra rotation/mirroring).
    171, 219, 190, 194, 221, 157, 194, 188};

void drawTrain(const TrainRenderState &state,
               const uint8_t *trainIcon,
               uint8_t *screenBuffer,
               int totalWidth)
{
  // Icon on module 4 (cols 39..32)
  for (int i = 0; i < 8; i++)
  {
    int targetCol = totalWidth - 1 - i;
    if (targetCol >= 0 && targetCol < totalWidth)
      screenBuffer[targetCol] = trainIcon[i];
  }

  // Optionally show a small marker for status (e.g., blink when stale); not used now.
  (void)state;
}

const uint8_t *getTrainIcon()
{
  return TRAIN_ICON;
}
