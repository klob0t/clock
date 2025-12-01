# Repository Guidelines

## Project Structure & Module Organization
- `src/`: main loop (`main.cpp`) handles Wi-Fi, NTP, weather polling, and pushes a 40-column `screenBuffer`; rendering helpers live in `display_clock.cpp`, `display_weather.cpp`, `animations.cpp`, `time_service.cpp`, and `weather_service.cpp`.
- `include/`: matching headers plus configuration (`config.h`), fonts, and render state structs. Keep secrets and tuning constants here rather than scattering literals in code.
- `.pio/`: PlatformIO build output; never commit. `.vscode/`: editor hints. `temp/`: scratch space for experiments.

## Build, Test, and Development Commands
- `pio run -e esp32`: compile the ESP32 target.
- `pio run -e esp32 -t upload`: build and flash over the default serial port (set `--upload-port COMx` if needed).
- `pio device monitor -b 115200`: serial monitor for logs and runtime state.
- `pio run -e esp32 -t clean`: clear build artifacts when deps or board config change.

## Coding Style & Naming Conventions
- Prefer 2-space indentation and the existing brace style seen in the display helpers; keep functions small and focused.
- Naming: camelCase for functions/locals (`drawWeather`), PascalCase for structs/enums (`WeatherData`, `DateMode`), SCREAMING_SNAKE_CASE for constants/macros (`GMT_OFFSET_SEC`).
- Use `constexpr` for configuration, keep helper functions `static` inside `.cpp`, and add `#pragma once` to headers with minimal includes.
- Avoid heap use; reuse buffers (`screenBuffer`) and pass by reference where possible.

## Testing Guidelines
- No automated suite yet; always run `pio run -e esp32` before opening a PR.
- Hardware smoke test: boot, confirm time updates, weather fetch cycles, bounce dot animates, and serial logs stay clean.
- For new logic, add PlatformIO Unity tests under `test/` named `test_<feature>.cpp` and run `pio test -e esp32`.
- Simulate offline cases (bad Wi-Fi or API key) to verify graceful handling and retry intervals from `config.h`.

## Commit & Pull Request Guidelines
- Use clear, imperative commit messages (e.g., `fix: normalize weather icon ids`, `feat: add ntp retry backoff`) instead of placeholders.
- PRs should include: summary of changes, commands/tests run, hardware details (board, matrix modules), and screenshots/photos or serial captures of the display.
- Call out any `config.h` changes and avoid committing real Wi-Fi or API keys; use placeholders in shared diffs.

## Security & Configuration Tips
- Keep credentials centralized in `include/config.h` and strip personal values before publishing.
- Do not commit `.pio` artifacts or transient calibration data; stash experiments in `temp/` and clean up.
- When modifying weather/time requests, favor HTTPS where available and handle HTTP failures with retries defined by the config constants.
