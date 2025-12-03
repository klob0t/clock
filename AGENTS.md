# Repository Guidelines

## Project Structure & Module Organization
- `src/`: main loop (`main.cpp`) orchestrates Wi-Fi/NTP/weather, drives page state, and writes the 40-column `screenBuffer`; display helpers live in `display_clock.cpp`, `display_weather.cpp`, `display_sun.cpp`, and `display_scope.cpp`.
- `include/`: matching headers, config (`config.h`), fonts, and render state structs. Keep secrets and tuning constants here.
- `.pio/`: PlatformIO build output (do not commit). `.vscode/`: editor hints. `temp/`: scratch space.

## Build, Test, and Development Commands
- `pio run -e esp32`: build the ESP32 firmware.
- `pio run -e esp32 -t upload`: build and flash (set `--upload-port COMx` if needed).
- `pio device monitor -b 115200`: serial logs/runtime state (IP prints on boot, control packets logged).
- `pio run -e esp32 -t clean`: remove build artifacts after board/deps changes.

## Coding Style & Naming Conventions
- 2-space indentation; keep existing brace style from display helpers.
- Naming: camelCase for functions/locals, PascalCase for structs/enums, SCREAMING_SNAKE_CASE for constants.
- Prefer `constexpr` for configuration; minimize heap use; pass buffers by reference.
- Add `#pragma once` to headers with minimal includes; keep literals centralized in `config.h`.

## Testing Guidelines
- No automated suite yet; run `pio run -e esp32` before pushing.
- Hardware smoke test: verify time updates, weather fetch, page cycling (clock ? weather ? sun ? scope), UDP control hotkey works, matrix renders cleanly.
- For new logic, add Unity tests under `test/` (`test_<feature>.cpp`) and run `pio test -e esp32`.
- Simulate offline cases (Wi-Fi/API) to confirm graceful handling and retry intervals from `config.h`.

## Commit & Pull Request Guidelines
- Use clear, imperative commit messages (e.g., `fix: normalize weather icon ids`, `feat: add sun page countdown`).
- PRs should include: summary, commands/tests run, hardware details (board/matrix modules), and screenshots/photos or serial captures. Call out any `config.h` changes; never commit real Wi-Fi/API keys.

## Security & Configuration Tips
- Keep credentials only in `include/config.h`; strip personal values before sharing.
- Do not commit `.pio` artifacts or transient data; stash experiments in `temp/` and clean up.
- Favor HTTPS for requests; handle HTTP failures with configured retries.

## PC Senders & Tools
- `send_keys_udp.py`: hotkey to control pages (`0x20` cycles through pages).
- `send_audio_udp.py`: phase-aligned oscilloscope sender using soundcard loopback.
- `send_spectrum_udp.py`: FFT-based spectrum sender with log bands, auto-scaling, and tilt compensation.
- `sc_device_test.py` / `sc_loopback_test.py`: list devices and verify loopback audio capture.
