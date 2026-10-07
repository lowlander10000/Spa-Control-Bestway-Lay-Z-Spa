# Changelog

## v3.2.6 — Connector test controlled bus pause

- Fixed connector test being falsely blocked by traffic generated/handled by the controller itself.
- Normal BWC CIO/DSP communication is now stopped before the bench connector test starts.
- Bus pins are released to INPUT and allowed to settle before the bidirectional pin test runs.
- Normal BWC communication is restored automatically immediately after the test.
- Removed the previous live-traffic blocking check from the connector test flow.
- Hardware configuration, translations, light/dark mode styling and test result indicators are unchanged.
- Updated firmware, web interface and PWA cache version to v3.2.6.

## v3.2.5 — Connector test live traffic fix

- Fixed the connector test being blocked by a stale spa-connected state after disconnecting the connector.
- Safety check now measures live CIO/DSP packet activity for 700 ms immediately before the hardware test.
- The test is blocked only when real spa bus traffic is detected during that check.
- Existing connector-test UI, translations, light/dark mode styling and PASS/FAIL indicators are unchanged.
- Updated firmware, web interface and PWA cache version to v3.2.5.

## v3.2.4 — Connector hardware test

- Restored the original BWC-style connector loopback test in the Spa Control interface.
- Tests the three CIO ↔ DSP signal pairs in both directions with 100 alternating HIGH/LOW samples per direction.
- Added clear green/red status dots and per-direction error counts.
- Added a safety confirmation and blocks the test while active spa communication is detected.
- 5V, GND and the separate audio line are explicitly excluded from the digital loopback test.
- Added full NL / EN / DE / FR translations.
- Added matching dark- and light-mode styling.
- Updated firmware, web interface and PWA cache version to v3.2.4.

## v3.2.3 — Light-mode WiFi network readability

- Improved readability of scanned WiFi networks in light mode.
- Network names now use the normal dark text color instead of forced white text.
- Added a clearer light card background and border for each discovered network.
- Improved contrast for security details and RSSI/signal information.
- Dark mode WiFi styling is unchanged.
- Updated firmware, web interface and PWA cache version to v3.2.3.

## v3.2.2 — WiFi clean restart

- Added an automatic controlled restart 3.5 seconds after a first successful WiFi setup.
- The success screen and assigned IP address are shown before the restart begins.
- After reboot, Spa Control starts in station-only mode when saved WiFi credentials exist, avoiding the slow AP+STA transition.
- The setup access point is started only when no WiFi credentials exist, when the saved connection fails, or when an established connection is lost.
- Failed WiFi setup still keeps or restores `LayZSpa-Setup` so the user can retry.
- Added restart-pending diagnostics to the WiFi status JSON.
- Updated firmware, web interface and PWA cache version to v3.2.2.

## v3.2.1 — WiFi connection feedback

- Added clear step-by-step WiFi connection feedback: connecting, waiting, success and failure.
- Shows the assigned IP address and an Open Spa Control button after a successful connection.
- Keeps the setup access point active for 8 seconds after success so the browser can receive the final status before the AP is disabled.
- Temporary request failures during the AP-to-home-WiFi handoff no longer show as an immediate load failure.
- Fixed WiFi polling to use the backend `state` field consistently.
- Updated firmware, web interface and PWA cache version to v3.2.1.

## v3.2.0 — Final

- Finalized maintenance tracking and multilingual maintenance alerts.
- Added robust backup and restore while intentionally excluding Wi-Fi credentials.
- Improved MQTT integration, diagnostics, navigation, translations and responsive layout.
- Fixed Planner Add Schedule behavior and weather/settings persistence issues.
- Improved LittleFS web upload reliability.
- Removed Automation to reduce heap pressure and improve ESP8266 stability.
- Updated firmware, web interface and PWA cache version to v3.2.0.
