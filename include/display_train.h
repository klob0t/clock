#pragma once
#include <Arduino.h>

struct TrainRenderState
{
  bool hasData;
  char origin[4];
  char destination[4];
  unsigned long departEpoch; // UTC epoch seconds
  unsigned long nowEpoch;    // UTC epoch seconds
};

void drawTrain(const TrainRenderState &state,
               const uint8_t *trainIcon,
               uint8_t *screenBuffer,
               int totalWidth);

const uint8_t *getTrainIcon();
