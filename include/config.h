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

//Pages and transitions
constexpr unsigned long CLOCK_PAGE_DURATION_MS = 50000;
constexpr unsigned long WEATHER_PAGE_DURATION_MS = 10000;
constexpr unsigned long SUN_PAGE_DURATION_MS = 10000;
constexpr unsigned long SCOPE_PAGE_DURATION_MS = 10000;
constexpr unsigned long PAGE_TRANSITION_MS = 600;

//Display/control
constexpr uint8_t DISPLAY_INTENSITY = 1;
constexpr uint16_t CONTROL_UDP_PORT = 4210;
constexpr uint16_t AUDIO_UDP_PORT = 4211;
constexpr uint8_t CTRL_CODE_PAGE_CLOCK = 0x01;
constexpr uint8_t CTRL_CODE_PAGE_WEATHER = 0x02;
constexpr uint8_t CTRL_CODE_PAGE_SUN = 0x03;
constexpr uint8_t CTRL_CODE_PAGE_SCOPE = 0x04;
constexpr uint8_t CTRL_CODE_NEXT_PAGE = 0x20;
constexpr char MDNS_HOSTNAME[] = "myclock";
