# Spa Control v3.0.1 — Hardware page bug fix

## Fixed

- Removed obsolete Hardware-page JavaScript references to `hwPowerOverride`, `hwHeat1`, `hwHeat2`, `hwPump`, `hwAir`, `hwIdle`, and `hwJet`.
- Opening the Hardware page no longer fails with `null is not an object`.
- Hardware settings now save only hardware-related fields.
- Existing `pwr_levels` and other unknown fields in `bestway_hwcfg.json` are preserved when Hardware settings are saved.
- Energy and power-consumption values remain managed exclusively from the Energy page.
- Firmware, web interface and PWA cache versions updated to v3.0.1.
