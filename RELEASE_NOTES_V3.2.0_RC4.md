# Spa Control v3.2.0 RC4

## LittleFS upload fix

During a LittleFS OTA upload, normal runtime managers and WebSocket traffic are paused. The HTTP upload handler remains active. If the upload fails or is aborted, LittleFS is remounted and normal operation resumes. A successful upload remains paused until restart.
