# Changelog

## v3.2.1 — Stable

- Fixed Planner trash action so a scheduled command can be removed per individual day instead of deleting the complete multi-day schedule.
- Removed obsolete unused atomic JSON helper code that caused the `-Wunused-function` compiler warning.
- Updated firmware, web interface, MQTT firmware reporting and PWA cache version to v3.2.1.

## v3.2.0 — Final

- Finalized maintenance tracking and multilingual maintenance alerts.
- Added robust backup and restore while intentionally excluding Wi-Fi credentials.
- Improved MQTT integration, diagnostics, navigation, translations and responsive layout.
- Fixed Planner Add Schedule behavior and weather/settings persistence issues.
- Improved LittleFS web upload reliability.
- Removed Automation to reduce heap pressure and improve ESP8266 stability.
- Updated firmware, web interface and PWA cache version to v3.2.0.
