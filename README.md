# ESP32 LED Matrix Clock + Weather

A 5-module MAX7219/FC16 (8�40) LED matrix clock for ESP32 with a bouncing dot, date/day shuffle, and weather from OpenWeatherMap.

## Hardware
- ESP32 dev board (Arduino framework)
- 5 � FC16 MAX7219 8�8 modules, daisy-chained
- Wiring (VSPI):
  - MOSI (GPIO23) -> DIN
  - SCK  (GPIO18) -> CLK
  - CS   (GPIO5)  -> CS/LOAD
  - 5V (or 3.3V if your modules support it) and GND shared

## Build & Flash
- PlatformIO environment: env:esp32 (see platformio.ini)
- Dependencies handled by PlatformIO:
  - MD_Parola, MD_MAX72XX, Time, ArduinoJson
- Build/flash: `pio run -e esp32 -t upload`
- Serial monitor: 115200 baud

## Configuration
Set these near the top of src/main.cpp:
- ssid / password � your Wi-Fi
- apiKey � OpenWeatherMap API key
- city, countryCode � location for weather
- gmtOffset_sec, daylightOffset_sec � timezone

## Display Behavior
- Clock page: 12h format with blinking colon, bouncing dot across module 1.
- Date/Day: cycles via scramble transition.
- Weather page: icon on module 5, scrolling description, temperature on module 1.
- Intensity is fixed low (MD_MAX72XX::INTENSITY = 1); adjust as needed.

## AM/PM Indicator (manual draw)
If you want the tiny 2�2 AM/PM glyph at columns 23/24 (1-based):
- Use bytes:
  - PM: left 0x51, right 0x77
  - AM: left 0x55, right 0x72
- In updateDisplay, inside PAGE_CLOCK after writing the bounce pixel and before pushing columns, OR them into screenBuffer[18] and screenBuffer[17] (0-based).

## Notes
- Current startup waits for Wi-Fi and NTP before drawing; consider adding a timeout/offline message if you want immediate display.
- Weather refresh interval: 10 minutes by default.
