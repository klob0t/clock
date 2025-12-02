#pragma once
#include <Arduino.h>

struct WeatherData {
    bool valid;
    int id;
    String desc;
    int feelsLike;
    unsigned long sunrise; // UTC epoch seconds
    unsigned long sunset;  // UTC epoch seconds
};

WeatherData fetchWeather(const String &apiKey, const String &lat, const String &lon);
