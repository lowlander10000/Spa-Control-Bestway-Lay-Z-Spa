# Spa Control v3.2.0

Stabiele release voor ESP8266-gebaseerde Bestway Lay-Z-Spa-bediening.

## Belangrijkste functies

- Responsieve lokale PWA voor bediening en live status.
- Planner met terugkerende schema's.
- MQTT-integratie en Home Assistant Discovery.
- Instelbare MQTT-statustopics, inclusief daadwerkelijk verwarmen.
- Onderhoud voor filter vervangen, filter schoonmaken en chloor toevoegen.
- Onderhoudsmeldingen op het dashboard en via MQTT.
- Sorteerbare en instelbare navigatie en dashboardkaarten.
- Firmware- en LittleFS-update via de webinterface.
- Back-up en herstel van instellingen, hardware, onderhoud, planner, energie en interfacevoorkeuren.
- WiFi-gegevens worden bewust niet uit een back-up hersteld.
- Nederlandse, Engelse, Duitse en Franse interface.

## Stabiliteit

- Automation is verwijderd om heapgebruik te verlagen en de ESP8266 stabieler te maken.
- Minder heapfragmentatie en kleinere tijdelijke JSON-buffers.
- Betrouwbaardere LittleFS-upload via de webinterface.
- Uitgebreidere diagnostiek voor heap, MQTT, OTA en filesystem.
- Verbeterde mobiele layout met voldoende ruimte boven de onderste navigatie.

## Bijwerken

1. Maak eerst een back-up in Spa Control.
2. Upload `Spa_Control_v3.2.0_firmware.bin`.
3. Upload `Spa_Control_v3.2.0_littlefs.bin`.
4. Ververs de browser geforceerd of herstart de geïnstalleerde PWA.
5. Herstel zo nodig de back-up. WiFi blijft behouden en wordt niet teruggezet.
