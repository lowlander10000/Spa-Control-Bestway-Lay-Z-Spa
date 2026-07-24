#include "energyManager.h"
#include <LittleFS.h>
#include "settingsManager.h"
#include "spaState.h"

namespace { struct EnergyFile { uint32_t magic; uint16_t version; EnergyTotals totals; }; constexpr uint32_t MAGIC=0x454E4731; constexpr uint16_t VERSION=1; }
EnergyManager energyManager;
bool EnergyManager::begin() { totals_ = EnergyTotals{}; load(); lastTickAt_=millis(); lastSaveAt_=millis(); return true; }
void EnergyManager::loop() {
  const unsigned long now=millis(); const unsigned long elapsed=now-lastTickAt_; lastTickAt_=now;
  if (spa.heaterActive) totals_.heaterMs += elapsed;
  if (spa.filter) totals_.filterMs += elapsed;
  if (spa.bubbles) totals_.bubblesMs += elapsed;
  if (spa.jets) totals_.jetsMs += elapsed;
  if (now-lastSaveAt_>=SAVE_INTERVAL_MS) { lastSaveAt_=now; save(); }
}
const EnergyTotals& EnergyManager::totals() const { return totals_; }
double EnergyManager::totalKwh() const {
  const auto& e=settingsManager.energy();
  const double wh=(totals_.heaterMs/3600000.0)*e.heaterWatts+(totals_.filterMs/3600000.0)*e.filterWatts+(totals_.bubblesMs/3600000.0)*e.bubblesWatts+(totals_.jetsMs/3600000.0)*e.jetsWatts;
  return wh/1000.0;
}
double EnergyManager::estimatedCost() const { return totalKwh()*settingsManager.energy().pricePerKwh; }
String EnergyManager::toJson() const {
  String j="{";
  j += "\"heaterHours\":"+String(totals_.heaterMs/3600000.0,3);
  j += ",\"filterHours\":"+String(totals_.filterMs/3600000.0,3);
  j += ",\"bubblesHours\":"+String(totals_.bubblesMs/3600000.0,3);
  j += ",\"jetsHours\":"+String(totals_.jetsMs/3600000.0,3);
  j += ",\"totalKwh\":"+String(totalKwh(),3);
  j += ",\"estimatedCost\":"+String(estimatedCost(),2);
  const auto& cfg = settingsManager.energy();
  j += ",\"currency\":\""+cfg.currency+"\"";
  j += ",\"heaterWatts\":"+String(cfg.heaterWatts);
  j += ",\"filterWatts\":"+String(cfg.filterWatts);
  j += ",\"bubblesWatts\":"+String(cfg.bubblesWatts);
  j += ",\"jetsWatts\":"+String(cfg.jetsWatts);
  j += ",\"pricePerKwh\":"+String(cfg.pricePerKwh,3);
  j += "}";
  return j;
}
bool EnergyManager::reset() { totals_=EnergyTotals{}; if(LittleFS.exists(ENERGY_FILE)) LittleFS.remove(ENERGY_FILE); return save(); }
bool EnergyManager::load() { File f=LittleFS.open(ENERGY_FILE,"r"); if(!f) return false; EnergyFile d{}; bool ok=f.read((uint8_t*)&d,sizeof(d))==sizeof(d)&&d.magic==MAGIC&&d.version==VERSION; f.close(); if(ok) totals_=d.totals; return ok; }
bool EnergyManager::save() { File f=LittleFS.open(ENERGY_FILE,"w"); if(!f) return false; EnergyFile d{MAGIC,VERSION,totals_}; bool ok=f.write((const uint8_t*)&d,sizeof(d))==sizeof(d); f.close(); return ok; }
