# Installation

## Requirements

- ESP8266 NodeMCU v2 / ESP-12E
- USB data cable
- Visual Studio Code with PlatformIO IDE, or PlatformIO Core
- Compatible Bestway interface hardware
- A safe, isolated workspace

> Disconnect the spa from mains power before opening or modifying it. Follow the upstream hardware documentation. Do not work on energized equipment.

## Build

Open the repository root, which contains `platformio.ini`, then build:

```bash
pio run
```

The configured environment is `nodemcuv2`.

## Upload firmware

```bash
pio run --target upload
```

## Upload LittleFS

The web application, icons, pages and translations are stored in `data/` and must be uploaded separately:

```bash
pio run --target uploadfs
```

Upload both firmware and LittleFS for a complete first installation. For web-only changes, an `uploadfs` may be sufficient.

## First Wi-Fi setup

When no valid saved Wi-Fi connection is available, Spa Control starts a temporary access point. Connect to it, open the setup page and enter the target Wi-Fi credentials. The access point closes after a successful connection and becomes available again when connection setup fails.

## Serial monitor

```bash
pio device monitor
```

The configured monitor speed is `115200` baud.

## PWA installation

Open Spa Control in a supported browser and choose **Install app** or **Add to Home Screen**. When an older name or icon remains cached, remove the old installed web app, refresh the page and install it again.

## Updating

Create a backup of settings before major changes. Upload firmware and LittleFS for releases that contain both backend and web-interface changes.
