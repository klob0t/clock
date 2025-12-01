#pragma once
#include <Arduino.h>
#include <time.h>

struct TimeState
{
  bool valid;
  tm now;
};

// Call once after Wi-Fi is up; returns true if NTP sync succeeds within a few tries.
bool initTime(const char *ntpServer, long gmtOffsetSec, int daylightOffsetSec);

// Grab the latest time; valid=false if getLocalTime fails.
TimeState getTimeState(unsigned long timeoutMs = 0);
