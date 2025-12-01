#include "time_service.h"

bool initTime(const char *ntpServer, long gmtOffsetSec, int daylightOffsetSec)
{
  configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);
  tm t{};
  for (int i = 0; i < 10; ++i)
  {
    if (getLocalTime(&t, 500)) // 500 ms timeout per attempt
      return true;
    delay(100);
  }
  return false;
}

TimeState getTimeState(unsigned long timeoutMs)
{
  TimeState s{};
  tm t{};
  if (getLocalTime(&t, timeoutMs))
  {
    s.valid = true;
    s.now = t;
  }
  else
  {
    s.valid = false;
  }
  return s;
}
