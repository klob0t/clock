# ESP32 LED Matrix Clock + Weather + Sun + Scope

A 5-module MAX7219/FC16 (8x40) LED matrix clock for ESP32 with a bouncing dot, date/day shuffle, weather from OpenWeatherMap, a sunrise/sunset countdown, and an oscilloscope/spectrum page fed over UDP.

## Hardware
- ESP32 dev board (Arduino framework)
- 5 FC16 MAX7219 8x8 modules, daisy-chained
- Wiring (VSPI):
  - MOSI (GPIO23) -> DIN
  - SCK  (GPIO18) -> CLK
  - CS   (GPIO5)  -> CS/LOAD
  - 5V (or 3.3V if your modules support it) and GND shared

## Build & Flash
- PlatformIO env: `env:esp32` (see platformio.ini)
- Dependencies via PlatformIO: MD_Parola, MD_MAX72XX, Time, ArduinoJson
- Build: `pio run -e esp32`
- Flash: `pio run -e esp32 -t upload` (set `--upload-port COMx` if needed)
- Monitor: `pio device monitor -b 115200`

## Configuration
Edit `include/config.h`:
- `WIFI_SSID` / `WIFI_PASSWORD`
- `OWM_API_KEY`, `LATITUDE`, `LONGITUDE`
- `NTP_SERVER`, `GMT_OFFSET_SEC`, `DAYLIGHT_OFFSET_SEC`
- Page timings, control port/codes, mDNS hostname (`myclock` by default), display intensity

## Display Behavior
- Page cycle: Clock  Weather  Sun  Clock (Scope is manual/sticky via control).
- Clock: 12h with blinking colon; bouncing dot on module 1; AM/PM glyph on module 2.
- Weather: icon on module 5; scrolling description on modules 1; temperature on module 1.
- Sun: sun/moon sprite on module 5; scrolling Milford text Sunrise/Sunset in X hours/minutes on modules 1.
- Scope: oscilloscope/spectrum plot across all columns. Feed samples or spectrum over UDP (see below). Scope stays until you trigger a page change.

## Controls (UDP + mDNS)
- mDNS hostname: `myclock.local` (IP prints on boot).
- Control UDP port: `CONTROL_UDP_PORT` (default 4210).
- Control codes: `0x01` clock, `0x02` weather, `0x03` sun, `0x04` scope, `0x20` next page (cycles clockweathersunscopeclock when used repeatedly).
- Example Python sender with hotkey in `send_keys_udp.py` (Ctrl+Alt+F10 sends `0x20`).
- Audio UDP port: `AUDIO_UDP_PORT` (default 4211). Send 40 bytes of 0255 to draw columns; extra bytes are ignored.

## PC Senders
- `send_audio_udp.py`: phase-aligned oscilloscope sender using soundcard loopback; maps audio to 40 bytes.
- `send_spectrum_udp.py`: FFT-based spectrum sender with log bands, auto-scaling, and tilt compensation.
- `sc_device_test.py` / `sc_loopback_test.py`: list devices and verify loopback audio capture.

## Notes
- Weather refresh interval: 10 minutes by default (`WEATHER_CHECK_INTERVAL`).
- Startup waits for Wi-Fi + NTP before drawing; logs IP and control packets to serial.
