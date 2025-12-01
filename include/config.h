#pragma once

//Wi-Fi
constexpr char WIFI_SSID[] = "HALUU, how are you?";
constexpr char WIFI_PASSWORD[] = "haluubanget!";

//OpenWeatherMap
constexpr char OWM_API_KEY[] = "dd0bd7ff5c526a7abeb18234d403f935";
constexpr char LATITUDE[] = "-6.1488";
constexpr char LONGITUDE[] = "106.8468";

//Time/NTP
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr long GMT_OFFSET_SEC = 25200;
constexpr int DAYLIGHT_OFFSET_SEC = 0;

//Weather polling
constexpr unsigned long WEATHER_CHECK_INTERVAL = 600000;
constexpr unsigned long WEATHER_RETRY_INTERVAL = 10000;