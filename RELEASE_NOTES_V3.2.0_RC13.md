# Spa Control v3.2.0 RC13 — Automation removed

## Changed
- Automation functionality has been fully removed to reduce ESP8266 heap usage and improve stability.
- Planner and Maintenance remain available and unchanged.
- Removed Automation page, menu entries, bottom navigation entry, API routes, MQTT subscriptions, diagnostics, backup fields, local cache use, translations, CSS, JavaScript and configuration files.
- Navigation defaults and interface ordering no longer contain Automation.
- Version and PWA cache updated to RC13.

## Upgrade note
Build and upload both firmware and LittleFS. Existing `/automation.json` files from older installations are no longer read and may safely remain until a clean LittleFS image is installed.
