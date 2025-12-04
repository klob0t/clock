#include "train_service.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <time.h>

static unsigned long parseTimeToEpochToday(const char *ts)
{
  // Ignore date; parse only HH:MM:SS and treat as daily recurring.
  int hour, minute, second;
  if (!ts || sscanf(ts, "%*d-%*d-%*d %2d:%2d:%2d", &hour, &minute, &second) != 3)
    return 0;

  time_t now = time(nullptr);
  if (now == (time_t)(-1))
    return 0;
  struct tm t = *localtime(&now);
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t candidate = mktime(&t); // today at that time (local)
  if (candidate <= now)
  {
    t.tm_mday += 1; // roll forward a day
    candidate = mktime(&t);
  }
  return (candidate < 0) ? 0 : (unsigned long)candidate;
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
  int destMatchCount = 0;
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
      destMatchCount++;

    if (!originFilter.equalsIgnoreCase(orig))
      continue;
    if (!isDestAllowed(dest, destFilters, destCount))
      continue;

    unsigned long depEpoch = parseTimeToEpochToday(depStr);
    if (depEpoch == 0)
    {
      Serial.printf("Train: parse fail for %s\n", depStr);
      continue;
    }
    if (depEpoch <= nowEpoch)
      depEpoch += 86400UL; // treat schedule as daily; roll to tomorrow if already passed
    if (depEpoch <= nowEpoch)
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

  Serial.printf("Train: origin matches %d, dest matches %d, future matches %d\n", originCount, destMatchCount, futureCount);
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
