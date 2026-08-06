# MQTT

MQTT support is optional and configured in the Spa Control web interface.

## Configuration

Required fields normally include:

- broker host;
- broker port;
- client ID;
- base topic;
- optional username and password;
- optional Home Assistant discovery.

The source default base topic is `layzspa`. Replace `<base>` below with your configured base topic.

## Published topics

| Topic | Payload |
|---|---|
| `<base>/temperature` | Current spa temperature |
| `<base>/target/state` | Target temperature |
| `<base>/heater/state` | `ON` or `OFF` |
| `<base>/filter/state` | `ON` or `OFF` |
| `<base>/bubbles/state` | `ON` or `OFF` |
| `<base>/jets/state` | `ON` or `OFF` |
| `<base>/spa/connected` | `ON` or `OFF` |
| `<base>/availability` | `online` or `offline` |
| `<base>/state` | Combined JSON state |

State values are published as retained messages.

## Command topics

| Topic | Accepted payload |
|---|---|
| `<base>/command/heater` | On/off command |
| `<base>/command/filter` | On/off command |
| `<base>/command/bubbles` | On/off command |
| `<base>/command/jets` | On/off command |
| `<base>/command/target` | Integer from 20 through 40 |

On/off parsing is case-insensitive in the firmware and accepts common boolean command forms.

## Home Assistant

When Home Assistant discovery is enabled, Spa Control publishes discovery configuration under the standard `homeassistant/` prefix. Confirm the broker, credentials and discovery setting in the MQTT page.

## Security

Do not expose the MQTT broker publicly. Use authentication and network isolation. Never commit broker passwords or exported settings to GitHub.
