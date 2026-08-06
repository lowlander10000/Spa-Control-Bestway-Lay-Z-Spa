# Spa Control v3.2.0 RC14

Stability-only release candidate. No user-facing functionality changed.

## Internal improvements
- Removed the large MQTT configuration fingerprint String from every loop iteration.
- MQTT now reacts to a lightweight settings revision counter.
- Removed duplicate MQTT payload allocation.
- Added capacity reservations to frequently generated JSON strings.
- Reduced the temporary maintenance JSON document size.
- Updated web cache identifiers to RC14.
