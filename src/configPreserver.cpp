#include "configPreserver.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <LittleFS.h>

namespace {
constexpr size_t EEPROM_TOTAL_SIZE = 4096;
constexpr size_t BACKUP_OFFSET = 512; // 0..511 is gereserveerd voor WiFiManager.
constexpr uint32_t MAGIC = 0x4C5A5343; // "LZSC"
constexpr uint16_t VERSION = 1;
constexpr const char* SETTINGS_FILE = "/settings.json";
constexpr const char* HARDWARE_FILE = "/bestway_hwcfg.json";

struct BackupHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t settingsLength;
  uint16_t hardwareLength;
  uint8_t restorePending;
  uint8_t reserved[3];
  uint32_t checksum;
};

constexpr size_t DATA_OFFSET = BACKUP_OFFSET + sizeof(BackupHeader);
constexpr size_t MAX_DATA_SIZE = EEPROM_TOTAL_SIZE - DATA_OFFSET;

uint32_t checksumBytes(const String& a, const String& b) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < a.length(); ++i) {
    hash ^= static_cast<uint8_t>(a[i]);
    hash *= 16777619UL;
  }
  for (size_t i = 0; i < b.length(); ++i) {
    hash ^= static_cast<uint8_t>(b[i]);
    hash *= 16777619UL;
  }
  return hash;
}

bool readFile(const char* path, String& output) {
  output = "";
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  output.reserve(file.size());
  while (file.available()) {
    output += static_cast<char>(file.read());
    yield();
  }
  file.close();
  return true;
}

bool writeFile(const char* path, const String& content) {
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const size_t written = file.print(content);
  file.close();
  return written == content.length();
}

void writeHeader(const BackupHeader& header) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&header);
  for (size_t i = 0; i < sizeof(BackupHeader); ++i) {
    EEPROM.write(BACKUP_OFFSET + i, bytes[i]);
  }
}

BackupHeader readHeader() {
  BackupHeader header{};
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&header);
  for (size_t i = 0; i < sizeof(BackupHeader); ++i) {
    bytes[i] = EEPROM.read(BACKUP_OFFSET + i);
  }
  return header;
}
}

bool configPreserverBackupForFilesystemUpdate() {
  String settings;
  String hardware;

  // Ontbrekende bestanden worden als leeg opgeslagen. Bij een clean build
  // wordt deze functie niet aangeroepen en blijven de defaults uit /data actief.
  readFile(SETTINGS_FILE, settings);
  readFile(HARDWARE_FILE, hardware);

  const size_t totalLength = settings.length() + hardware.length();
  if (totalLength > MAX_DATA_SIZE ||
      settings.length() > 0xFFFF || hardware.length() > 0xFFFF) {
    Serial.println("Configuratie te groot voor veilige LittleFS-back-up");
    return false;
  }

  EEPROM.begin(EEPROM_TOTAL_SIZE);

  BackupHeader header{};
  header.magic = MAGIC;
  header.version = VERSION;
  header.settingsLength = static_cast<uint16_t>(settings.length());
  header.hardwareLength = static_cast<uint16_t>(hardware.length());
  header.restorePending = 1;
  header.checksum = checksumBytes(settings, hardware);
  writeHeader(header);

  size_t address = DATA_OFFSET;
  for (size_t i = 0; i < settings.length(); ++i) EEPROM.write(address++, settings[i]);
  for (size_t i = 0; i < hardware.length(); ++i) EEPROM.write(address++, hardware[i]);

  const bool ok = EEPROM.commit();
  EEPROM.end();
  Serial.println(ok ? "Configuratie veiliggesteld voor LittleFS-update"
                    : "Configuratie veiligstellen mislukt");
  return ok;
}

bool configPreserverRestoreAfterFilesystemUpdate() {
  EEPROM.begin(EEPROM_TOTAL_SIZE);
  BackupHeader header = readHeader();

  if (header.magic != MAGIC || header.version != VERSION ||
      header.restorePending != 1) {
    EEPROM.end();
    return false;
  }

  const size_t totalLength =
    static_cast<size_t>(header.settingsLength) + header.hardwareLength;
  if (totalLength > MAX_DATA_SIZE) {
    header.restorePending = 0;
    writeHeader(header);
    EEPROM.commit();
    EEPROM.end();
    Serial.println("Ongeldige configuratieback-up genegeerd");
    return false;
  }

  String settings;
  String hardware;
  settings.reserve(header.settingsLength);
  hardware.reserve(header.hardwareLength);

  size_t address = DATA_OFFSET;
  for (uint16_t i = 0; i < header.settingsLength; ++i) {
    settings += static_cast<char>(EEPROM.read(address++));
  }
  for (uint16_t i = 0; i < header.hardwareLength; ++i) {
    hardware += static_cast<char>(EEPROM.read(address++));
  }

  const bool checksumOk = checksumBytes(settings, hardware) == header.checksum;
  bool restored = checksumOk;
  if (checksumOk && header.settingsLength > 0) {
    restored = writeFile(SETTINGS_FILE, settings) && restored;
  }
  if (checksumOk && header.hardwareLength > 0) {
    restored = writeFile(HARDWARE_FILE, hardware) && restored;
  }

  // Een back-up wordt maar eenmaal teruggezet. Een normale USB/Clean Build
  // maakt geen pending marker en gebruikt daardoor de standaardbestanden.
  header.restorePending = 0;
  writeHeader(header);
  EEPROM.commit();
  EEPROM.end();

  Serial.println(restored ? "Configuratie hersteld na LittleFS-update"
                          : "Configuratie herstellen na LittleFS-update mislukt");
  return restored;
}
