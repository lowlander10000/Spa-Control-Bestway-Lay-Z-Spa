#pragma once

// Bewaart gebruikersconfiguratie tijdelijk buiten LittleFS tijdens een
// browser-LittleFS-update. WiFi-configuratie staat al in EEPROM en blijft
// daarom automatisch behouden.
bool configPreserverBackupForFilesystemUpdate();
bool configPreserverRestoreAfterFilesystemUpdate();
