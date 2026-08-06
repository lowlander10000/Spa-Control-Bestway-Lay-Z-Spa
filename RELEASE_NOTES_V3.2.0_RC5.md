# Spa Control v3.2.0 RC5 — LittleFS OTA Diagnostics

- Uitgebreide seriële logging voor iedere fase van een LittleFS OTA-upload.
- Concrete ESP8266 Update-foutcode en Nederlandse fouttekst in de HTTP-response.
- Tijdelijke uitschakeling van de configuratieback-up om deze als oorzaak uit te sluiten.
- Gebruik van de volledige U_FS-updatepartitie via UPDATE_SIZE_UNKNOWN.
- Browserlogging, timeout- en abortmeldingen toegevoegd.

Deze RC is bedoeld om de oorzaak van de uploadstop op 10% vast te stellen.
