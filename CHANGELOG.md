# Changelog

All notable changes to Spa Control are recorded here.

## [3.0.0] - 2026-07-24

First stable Spa Control release.

### Added
- Installable PWA named Spa Control with application icons.
- Responsive phone, tablet and desktop interface.
- Light and dark themes.
- Dutch, English, German and French translations.
- Weather card with retained last-known data.
- MQTT settings and Home Assistant discovery support.
- Diagnostics, scheduler, history and event-log interfaces.

### Fixed
- Time Sync status now uses translated `Synchronized` / `Waiting` strings.
- Power toggle is no longer simulated during every ESP reset.
- Dashboard cards follow light mode correctly.
- System information loads when the Info page is opened directly.
- Desktop dashboard width and hamburger-menu scrolling.
- MQTT password preservation and discovery compilation issue.

### Changed
- Version information, PWA caches and release labels finalized as v3.0.0 Stable.

## 2.x development series

The 2.x versions were iterative development builds leading to v3.0.0. Detailed historical notes remain available in the `RELEASE_NOTES_V2.*.md` files.
