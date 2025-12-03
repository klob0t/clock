#include "train_service.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <time.h>

static unsigned long parseUtcStringToEpoch(const char *ts)
{
  // Expected: "YYYY-MM-DD HH:MM:SS.xxx+00"
  if (!ts || strlen(ts) < 19)
    return 0;

  int year, mon, day, hour, minute, second;
  if (sscanf(ts, "%4d-%2d-%2d %2d:%2d:%2d", &year, &mon, &day, &hour, &minute, &second) != 6)
    return 0;

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = mon - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  // mktime assumes the tm is in local time. Input is UTC, so add the local offset to get correct UTC epoch.
  time_t localEpoch = mktime(&t);
  if (localEpoch < 0)
    return 0;
  long offset = GMT_OFFSET_SEC + DAYLIGHT_OFFSET_SEC;
  return (offset >= 0) ? (unsigned long)(localEpoch + offset) : (unsigned long)(localEpoch - (-offset));
}

static bool isDestAllowed(const String &dest, const String *list, int count)
{
  for (int i = 0; i < count; i++)
  {
    if (dest.equalsIgnoreCase(list[i]))
      return true;
  }
  return false;
}

TrainEntry fetchNextTrain(const String &stationId,
                          const String &originFilter,
                          const String *destFilters,
                          int destCount)
{
  TrainEntry result{};
  result.valid = false;

  String url = "https://api.comuline.com/v1/schedule/" + stationId;
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code <= 0 || code != HTTP_CODE_OK)
  {
    Serial.printf("Train fetch failed http=%d\n", code);
    http.end();
    return result;
  }

  // Parse full payload; schedule payload is ~60 KB.
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(70000);
  DeserializationError err = deserializeJson(doc, payload);
  if (err)
  {
    Serial.printf("Train JSON parse error: %s\n", err.c_str());
    return result;
  }

  JsonArray arr = doc["data"].as<JsonArray>();
  if (arr.isNull())
  {
    Serial.println("Train: no data array");
    return result;
  }

  Serial.printf("Train: total entries %u\n", arr.size());

  unsigned long nowEpoch = (unsigned long)time(nullptr);
  unsigned long bestEpoch = 0;
  int matchCount = 0;
  int originCount = 0;
  int destCount = 0;
  int futureCount = 0;

  int logIdx = 0;
  for (JsonObject item : arr)
  {
    const char *orig = item["station_origin_id"] | "";
    const char *dest = item["station_destination_id"] | "";
    const char *depStr = item["departs_at"] | "";
    if (logIdx < 5)
    {
      Serial.printf("Train entry %d: %s -> %s @ %s\n", logIdx, orig, dest, depStr);
      logIdx++;
    }

    if (strlen(orig) == 0 || strlen(dest) == 0 || strlen(depStr) == 0)
      continue;
    if (originFilter.equalsIgnoreCase(orig))
      originCount++;
    if (isDestAllowed(dest, destFilters, destCount))
      destCount++;

    if (!originFilter.equalsIgnoreCase(orig))
      continue;
    if (!isDestAllowed(dest, destFilters, destCount))
      continue;

    unsigned long depEpoch = parseUtcStringToEpoch(depStr);
    if (depEpoch == 0)
    {
      Serial.printf("Train: parse fail for %s\n", depStr);
      continue;
    }
    if (depEpoch == 0 || depEpoch <= nowEpoch)
      continue;
    futureCount++;

    matchCount++;
    if (!result.valid || depEpoch < bestEpoch)
    {
      result.valid = true;
      bestEpoch = depEpoch;
      result.departEpoch = depEpoch;
      snprintf(result.origin, sizeof(result.origin), "%s", orig);
      snprintf(result.destination, sizeof(result.destination), "%s", dest);
      result.route = item["route"] | "";
    }
  }

  Serial.printf("Train: origin matches %d, dest matches %d, future matches %d\n", originCount, destCount, futureCount);
  if (!result.valid)
  {
    Serial.printf("Train: no future matches (checked %d)\n", matchCount);
  }
  else
  {
    unsigned long eta = (bestEpoch > nowEpoch) ? (bestEpoch - nowEpoch) : 0;
    Serial.printf("Train: %s->%s in %lus (matches %d)\n", result.origin, result.destination, eta, matchCount);
  }

  return result;
}
