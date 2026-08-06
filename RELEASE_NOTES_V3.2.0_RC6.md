# Spa Control v3.2.0 RC6

LittleFS OTA compile and diagnostics correction.

- Removed unsupported `UPDATE_SIZE_UNKNOWN` for the installed ESP8266 core.
- Uses the detected LittleFS partition capacity for `Update.begin(..., U_FS)`.
- Logs multipart content length and detected filesystem update capacity.
- Retains RC5 serial diagnostics and temporarily disabled pre-upload backup.
