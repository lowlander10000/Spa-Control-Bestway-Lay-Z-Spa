# Spa Control v3.2.0 RC17

## Backup and restore

- Restores settings, hardware, maintenance, planner, energy and browser preferences independently.
- Wi-Fi credentials and access-point settings are never restored.
- A failure in one section no longer blocks other valid sections.
- Empty MQTT passwords in a backup preserve the currently stored password.
- Compatible with SpaControlBackup v1.0 and v2.0.
- Old Automation navigation references are removed during browser-setting restore.
- Large JSON configuration sections are restored without allocating a large ArduinoJson document.
