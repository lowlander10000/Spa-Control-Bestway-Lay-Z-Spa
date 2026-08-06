# Spa Control v3.2.0

Stable release for ESP8266-based Bestway Lay-Z-Spa control.

## Highlights

- Responsive local PWA for spa control and live status.
- Planner with recurring schedules.
- MQTT integration and Home Assistant discovery.
- Configurable MQTT status topics, including active heating state.
- Maintenance tracking for filter replacement, filter cleaning and chlorine.
- Maintenance warnings on the dashboard and via MQTT.
- Sortable and configurable navigation and dashboard cards.
- Firmware and LittleFS update through the web interface.
- Backup and restore for settings, hardware, maintenance, planner, energy and interface preferences.
- Wi-Fi credentials are intentionally not restored from backups.
- Dutch, English, German and French interface translations.

## Stability improvements

- Automation module removed to reduce ESP8266 heap usage and improve stability.
- Lower heap fragmentation and reduced temporary JSON allocations.
- Reliable LittleFS web upload handling.
- Improved diagnostics for heap, MQTT, OTA and filesystem status.
- Improved mobile layout and safe spacing above the bottom navigation.

## Upgrade instructions

1. Create a backup from the current Spa Control interface.
2. Upload `Spa_Control_v3.2.0_firmware.bin`.
3. Upload `Spa_Control_v3.2.0_littlefs.bin`.
4. Hard-refresh the browser or restart the installed PWA.
5. Restore the backup if required. Wi-Fi settings remain unchanged and are not restored.

## Notes

- Target board: ESP8266 NodeMCU v2 (`nodemcuv2`).
- Filesystem: LittleFS.
- This release does not include the former Automation page. Planner and Maintenance remain available.
