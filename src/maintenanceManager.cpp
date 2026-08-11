#include "maintenanceManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "timeManager.h"

MaintenanceManager maintenanceManager;

bool MaintenanceManager::begin() {
  if (!LittleFS.exists(FILE_PATH)) return save();
  return load();
}

bool MaintenanceManager::load() {
  File file=LittleFS.open(FILE_PATH,"r"); if(!file) return false;
  DynamicJsonDocument doc(1024); auto err=deserializeJson(doc,file); file.close();
  if(err) return false;
  auto read=[&](const char* key, MaintenanceItem& item){
    JsonObject obj=doc[key]; if(obj.isNull()) return;
    item.enabled=obj["enabled"] | item.enabled;
    item.intervalDays=constrain((int)(obj["intervalDays"] | item.intervalDays),1,3650);
    item.lastDone=(time_t)(obj["lastDone"] | (long)item.lastDone);
  };
  read("filterReplace",filterReplace_); read("filterClean",filterClean_); read("chlorine",chlorine_);
  return true;
}

bool MaintenanceManager::save() {
  DynamicJsonDocument doc(1024);
  auto write=[&](const char* key,const MaintenanceItem& item){JsonObject o=doc.createNestedObject(key);o["enabled"]=item.enabled;o["intervalDays"]=item.intervalDays;o["lastDone"]=(long)item.lastDone;};
  write("filterReplace",filterReplace_);write("filterClean",filterClean_);write("chlorine",chlorine_);
  File file=LittleFS.open(FILE_PATH,"w"); if(!file) return false; serializeJsonPretty(doc,file); file.close(); return true;
}

void MaintenanceManager::loop() {}

int MaintenanceManager::daysRemaining(const MaintenanceItem& item) const {
  if(!item.enabled) return 9999;
  if(item.lastDone<=0 || !timeManager.isSynchronized()) return item.lastDone<=0 ? 0 : 9999;
  time_t now=timeManager.nowUtc();
  long seconds=(long)(item.lastDone + (time_t)item.intervalDays*86400L - now);
  if(seconds>=0) return (int)((seconds+86399L)/86400L);
  return -(int)((-seconds)/86400L);
}

bool MaintenanceManager::due(const MaintenanceItem& item) const { return item.enabled && daysRemaining(item)<=0; }

String MaintenanceManager::isoDate(time_t value) const {
  if (value <= 0) {
    return "";
  }

  struct tm localTime;
  if (localtime_r(&value, &localTime) == nullptr) {
    return "";
  }

  char buffer[11] = {0};
  if (strftime(buffer, sizeof(buffer), "%Y-%m-%d", &localTime) == 0) {
    return "";
  }

  return String(buffer);
}
String MaintenanceManager::nextDate(const MaintenanceItem& item) const { return item.lastDone<=0 ? "" : isoDate(item.lastDone+(time_t)item.intervalDays*86400L); }
String MaintenanceManager::overallStatus() const {
  if(due(filterReplace_)||due(filterClean_)||due(chlorine_)) return "ALARM";
  if((filterReplace_.enabled&&daysRemaining(filterReplace_)<=2)||(filterClean_.enabled&&daysRemaining(filterClean_)<=2)||(chlorine_.enabled&&daysRemaining(chlorine_)<=2)) return "WARNING";
  return "OK";
}

String MaintenanceManager::toJson() const {
  DynamicJsonDocument doc(1536);
  auto add=[&](const char* key,const MaintenanceItem& item){JsonObject o=doc.createNestedObject(key);o["enabled"]=item.enabled;o["intervalDays"]=item.intervalDays;o["lastDate"]=isoDate(item.lastDone);o["nextDate"]=nextDate(item);o["daysRemaining"]=daysRemaining(item);o["due"]=due(item);};
  add("filterReplace",filterReplace_);add("filterClean",filterClean_);add("chlorine",chlorine_);doc["status"]=overallStatus();doc["timeSynchronized"]=timeManager.isSynchronized();String out;out.reserve(640);serializeJson(doc,out);return out;
}

bool MaintenanceManager::updateSettings(bool fre,uint16_t frd,bool fce,uint16_t fcd,bool ce,uint16_t cd){
  filterReplace_.enabled=fre;filterReplace_.intervalDays=constrain((int)frd,1,3650);
  filterClean_.enabled=fce;filterClean_.intervalDays=constrain((int)fcd,1,3650);
  chlorine_.enabled=ce;chlorine_.intervalDays=constrain((int)cd,1,3650);return save();
}

bool MaintenanceManager::markDone(const String& item){if(!timeManager.isSynchronized()) return false;time_t now=timeManager.nowUtc();if(item=="filterReplace")filterReplace_.lastDone=now;else if(item=="filterClean")filterClean_.lastDone=now;else if(item=="chlorine")chlorine_.lastDone=now;else return false;return save();}
