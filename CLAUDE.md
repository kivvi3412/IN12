# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

IN-12 Soviet nixie tube clock on ESP32-C3. Four nixie tubes driven by two daisy-chained 74HC595 shift registers, a neon bulb seconds indicator, WiFi AP+STA for provisioning, NTP time sync, and an iOS-dark-mode web UI for configuration.

## Build Commands

```bash
idf.py build          # Compile
idf.py flash          # Flash to device
idf.py monitor        # Serial monitor
idf.py flash monitor  # Combined flash + monitor
idf.py menuconfig     # Configure sdkconfig (though this project uses hardcoded defines, not Kconfig)
idf.py -p PORT flash  # Flash to specific serial port
```

Requires ESP-IDF 6.0.0+ with `IDF_PATH` set. Target: `esp32c3`.

## Architecture

### Initialization Sequence (app_main.cpp)

NVS → LittleFS → WiFi (AP+STA) → Controller → WebServer. Order matters — Controller needs NVS and time; WebServer needs Controller and WiFi.

### Component Dependency Graph

```
main → nvs_module, littlefs_module, wifi_module, controller, web_server
web_server → esp_http_server, controller, wifi_module, timezone_module, arduinojson
controller → in12_driver, timezone_module, nvs_module, esp_timer, project_config
wifi_module → esp_wifi, nvs_flash, mdns, nvs_module, timezone_module, arduinojson, project_config
in12_driver → driver, esp_driver_gpio, project_config
timezone_module → esp_sntp, freertos
nvs_module → nvs_flash, esp_wifi, project_config
littlefs_module → littlefs, project_config
project_config → (header-only, no dependencies)
```

### Key Design Patterns

- **Singletons**: `Controller` and `TimeManager` use Meyer's singleton (`getInstance()`)
- **Display priority system** (Controller resolves display state each tick):
  - Priority 3: Anti-cathode poisoning animation (temporary, 100ms `esp_timer`)
  - Priority 2: Demo/display mode (manual digit override)
  - Priority 1: Sleep window (tubes off)
  - Priority 0: Clock mode (default HH:MM)
- **Tube master switch**: `tube_on=false` overrides all priorities
- **Neon indicator**: Independent from display, driven by clock seconds
- **Thread safety**: FreeRTOS mutex protects all shared Controller state
- **Partial-update JSON API**: POST endpoints merge only provided fields into current settings

### NVS Persistence

Settings stored as POD blobs via `NVSModule` (template-based serialize/deserialize with size-mismatch safety). Namespaces/keys defined in `project_config.h`. If blob size changes between firmware versions, NVS erases and reinitializes.

### Web Frontend

Static files in `littlefs_root/` are flashed to a LittleFS partition and served by the HTTP server. Supports Range requests (206 Partial Content) for mobile video playback. The frontend is an Apple-style 3-tab settings UI.

## Configuration

All hardware and app constants are in `components/project_config/include/project_config.h` — no Kconfig files.

## Partition Table

Factory app: 1.5MB at 0x10000. LittleFS data: ~1.56MB at 0x190000. Total flash: 4MB.

## REST API

| Method | URI | Purpose |
|--------|-----|---------|
| GET/POST | `/api/light_data` | Tube/neon/display settings |
| GET/POST | `/api/time_data` | NTP status, timezone, manual time |
| POST | `/api/wifi/scan` | Trigger WiFi scan |
| GET | `/api/wifi/list` | Cached scan results |
| POST | `/api/wifi/connect` | Connect to AP |
| GET | `/api/wifi/status` | Current WiFi state |
| POST | `/api/reset` | Factory reset (erase NVS + reboot) |
| GET | `/*` | Static files from LittleFS |

## Managed Dependencies

Declared in `main/idf_component.yml`: ArduinoJson 7.4.3, espressif/mdns 1.11.0, joltwallet/littlefs 1.20.4. Locked in `dependencies.lock`.

## Notes

- Source comments are in Chinese
- WiFi module uses a state machine: IDLE → SCANNING → CONNECTING → SWITCHING_AP → CONNECTED_LOCAL / DISCONNECTED
- TimeManager polls `gettimeofday()` at 10Hz via a FreeRTOS task, dispatching half-second and minute callbacks
- NTP servers: ntp.aliyun.com + pool.ntp.org