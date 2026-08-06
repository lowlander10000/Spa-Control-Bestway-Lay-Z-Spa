#include "eventLog.h"
#include <LittleFS.h>
#include "timeManager.h"
EventLog eventLog;
bool EventLog::begin(){ rotateIfNeeded(); info("Controller gestart"); return true; }
void EventLog::info(const String& m){ append("INFO",m); }
void EventLog::warning(const String& m){ append("WARN",m); }
void EventLog::error(const String& m){ append("ERROR",m); }
void EventLog::append(const char* level,const String& message){ rotateIfNeeded(); File f=LittleFS.open(LOG_FILE,"a"); if(!f)return; String line=String((unsigned long)timeManager.nowUtc())+"|"+level+"|"+message; line.replace("\n"," "); line.replace("\r"," "); f.println(line); f.close(); }
void EventLog::rotateIfNeeded(){ if(!LittleFS.exists(LOG_FILE))return; File f=LittleFS.open(LOG_FILE,"r"); size_t size=f?f.size():0; if(f)f.close(); if(size>MAX_FILE_SIZE){ LittleFS.remove("/events.old.log"); LittleFS.rename(LOG_FILE,"/events.old.log"); } }
String EventLog::toJson(uint16_t maxLines) const { File f=LittleFS.open(LOG_FILE,"r"); if(!f)return "[]"; String lines[50]; uint16_t cap=maxLines>50?50:maxLines,count=0; while(f.available()){ String line=f.readStringUntil('\n'); line.trim(); if(line.isEmpty())continue; lines[count%cap]=line; count++; yield(); } f.close(); uint16_t shown=count<cap?count:cap; uint16_t start=count<cap?0:count%cap; String j="["; for(uint16_t i=0;i<shown;i++){ String line=lines[(start+i)%cap]; int a=line.indexOf('|'),b=line.indexOf('|',a+1); if(a<0||b<0)continue; String msg=line.substring(b+1); msg.replace("\\","\\\\"); msg.replace("\"","\\\""); if(j.length()>1)j+=","; j+="{\"timestamp\":"+line.substring(0,a)+",\"level\":\""+line.substring(a+1,b)+"\",\"message\":\""+msg+"\"}"; } j+="]"; return j; }
bool EventLog::clear(){ if(LittleFS.exists(LOG_FILE))LittleFS.remove(LOG_FILE); if(LittleFS.exists("/events.old.log"))LittleFS.remove("/events.old.log"); return true; }
