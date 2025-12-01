#pragma once
#include <Arduino.h>

struct WeatherData {
    bool valid;
    int id;
    String desc;
    int feelsLike;
};

WeatherData fetchWeather(const String &apiKey, const String &lat, const String &lon);
