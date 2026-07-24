# Spa Control for Bestway Lay Z Spa

[![Build](https://github.com/lowlander10000/Spa-Control/actions/workflows/build.yml/badge.svg)](https://github.com/lowlander10000/Spa-Control/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/lowlander10000/Spa-Control?display_name=tag&sort=semver)](https://github.com/lowlander10000/Spa-Control/releases/latest)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP8266-orange?logo=platformio)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino)](https://www.arduino.cc/)
[![License](https://img.shields.io/github/license/lowlander10000/Spa-Control)](LICENSE)

**Spa Control** is an ESP8266-based Wi-Fi controller and responsive Progressive Web App for compatible Bestway Lay-Z-Spa control systems.

This repository is a modified derivative of **WiFi-remote-for-Bestway-Lay-Z-SPA** by [visualapproach](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA) and its contributors. The original communication research, hardware support and core spa-control implementation belong to the upstream authors. See [CREDITS.md](CREDITS.md).

> [!WARNING]
> This is an unofficial hardware modification. Disconnect mains power before opening or modifying spa equipment. Mains voltage can cause serious injury or death. You use this software and any connected hardware entirely at your own risk.

## Highlights

- Responsive dashboard for phone, tablet and desktop
- Installable PWA named **Spa Control**
- Live WebSocket status updates
- Light and dark mode
- Dutch, English, German and French interface translations
- Non-blocking Wi-Fi setup and fallback access point
- MQTT control and optional Home Assistant discovery
- Scheduler and timer support
- Weather card using Open-Meteo
- OTA firmware update support
- Diagnostics, system information, event log and history
- LittleFS-hosted web interface
- Support for the Bestway communication drivers included in the upstream project

## Hardware and software

- ESP8266 NodeMCU v2 / ESP-12E
- PlatformIO
- Arduino framework for ESP8266
- LittleFS
- A compatible Bestway Lay-Z-Spa interface and level-shifting hardware as described by the upstream project

The project is configured for the `nodemcuv2` PlatformIO environment at 160 MHz.
## Quick start

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode).
2. Clone or download this repository.
3. Open the repository folder in PlatformIO.
4. Connect the ESP8266 by USB.
5. Build and upload the firmware.
6. Upload the LittleFS filesystem image.
7. Connect to the temporary Spa Control access point when no saved Wi-Fi network is available.
8. Enter your Wi-Fi credentials and open the controller in a browser.

Detailed instructions: [docs/installation.md](docs/installation.md)


## Ready-to-flash releases

Prebuilt firmware and LittleFS images are published on the
[Releases page](https://github.com/lowlander10000/Spa-Control/releases).
For a complete update, install both the firmware image and the matching LittleFS image.


### Nederlandse korte uitleg

Open de map in PlatformIO, upload eerst **firmware** en daarna **LittleFS**. Wanneer de ESP nog geen Wi-Fi-gegevens heeft, start hij een tijdelijk access point waarmee je het netwerk kunt instellen.

## PlatformIO commands

```bash
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

## MQTT

MQTT is optional. The default base topic in the current source is `layzspa`. Commands and status topics are documented in [docs/mqtt.md](docs/mqtt.md).

## Weather data

The dashboard weather card retrieves public forecast data directly from [Open-Meteo](https://open-meteo.com/). Location and update interval are configured in the web interface. The browser retains the last known weather data so the card does not immediately become empty after a controller restart.

## Project structure

```text
src/        ESP8266 application source
include/    Application headers
lib/        Bestway communication and display drivers
data/       PWA, pages, scripts, styles, icons and translations
docs/       Installation, MQTT, hardware and release documentation
```

## Upstream project and attribution

Spa Control is based on:

- [visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA](https://github.com/visualapproach/WiFi-remote-for-Bestway-Lay-Z-SPA)
- Fully compatible with the hardware used so dont like mine? use this great one

Please consult the upstream repository for the original hardware research, wiring information, schematics, PCB designs, manual, discussions and contributor history. Spa Control is not affiliated with or endorsed by Bestway.

## Contributing

Bug reports and improvements are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Never post Wi-Fi or MQTT passwords in an issue, screenshot or log.

## License

This project is distributed under the **GNU General Public License v3.0** because it is derived from GPL-3.0-licensed upstream work. See [LICENSE](LICENSE). Modified versions must preserve the applicable copyright and license notices and remain available under GPL-3.0 when distributed.
