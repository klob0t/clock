#include "weather_service.h"
#include <HTTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>

static String buildUrl(const String &apiKey, const String &lat, const String &lon)
{
    return "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&appid=" + apiKey + "&units=metric";
}

WeatherData fetchWeather(const String &apiKey, const String &lat, const String &lon)
{
    WeatherData data{};
    data.valid = false;

    HTTPClient http;
    const String url = buildUrl(apiKey, lat, lon);
    http.begin(url);
    const int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err)
        {
            data.id = doc["weather"][0]["id"] | 0;
            data.desc = doc["weather"][0]["description"] | "";
            float tempFloat = doc["main"]["feels_like"] | 0.0;
            data.feelsLike = round(tempFloat);

            if (data.desc.length() > 0)
                data.desc.setCharAt(0, toupper(data.desc.charAt(0)));
            for (int i = 1; i < data.desc.length(); i++)
            {
                if (data.desc.charAt(i - 1) == ' ')
                    data.desc.setCharAt(i, toupper(data.desc.charAt(i)));
            }

            data.valid = true;
        }
    }

    http.end();
    return data;
}