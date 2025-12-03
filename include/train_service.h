#pragma once
#include <Arduino.h>

struct TrainEntry
{
  bool valid;
  char origin[4];
  char destination[4];
  unsigned long departEpoch; // UTC epoch seconds
  String route;
};

// Fetch the next matching train from the station schedule.
// Filters: origin must match originFilter, destination must be one of destFilters.
TrainEntry fetchNextTrain(const String &stationId,
                          const String &originFilter,
                          const String *destFilters,
                          int destCount);
