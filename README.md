# FlightDot ◉ ✈

![FlightDot running on the Waveshare 1.43-inch AMOLED](https://github.com/user-attachments/assets/a8eb5f1f-8bff-4143-9952-ea2f0b018ebf)

I built FlightDot as a small live ADS-B radar for the **Waveshare ESP32-S3-Touch-AMOLED-1.43**. It is made for the round 466×466 AMOLED board with 16 MB flash and 8 MB PSRAM.

This is an internet-connected radar, not a radio receiver. The ESP32 connects to Wi-Fi and requests nearby aircraft from public ADS-B services.

## What it does

- Shows nearby aircraft on a live green radar display.
- Draws heading-aware aircraft icons, range rings, compass points, and a smooth sweep.
- Opens a larger aircraft card with callsign, registration, manufacturer, model, altitude, speed, heading, squawk, and origin/destination cities.
- Lets you search for a city, airport, flight number, or callsign from the local setup page.
- Can follow a selected aircraft and keep the radar centred on it.
- Stores the radar centre, range, brightness, sweep setting, and Wi-Fi credentials in NVS.
- Includes a Wi-Fi setup portal, local configuration page, desktop LVGL/SDL2 simulator, and a mock-data build.

The main screen stays intentionally simple. Radar range and aircraft count are available in the statistics view, while the web page is used for Wi-Fi, search, centre, range, and brightness.

## Hardware

The firmware targets the Waveshare ESP32-S3-Touch-AMOLED-1.43 board. The display driver follows the official Waveshare Demo V3 and supports the panel detection used by this board. Touch uses the FT3168-compatible protocol over I²C at `0x38`.

| Function | Pin / address |
| --- | --- |
| AMOLED QSPI CS / clock | GPIO 9 / GPIO 10 |
| AMOLED QSPI data | GPIO 11, 12, 13, 14 |
| AMOLED reset / enable | GPIO 21 / GPIO 42 |
| I²C SDA / SCL | GPIO 47 / GPIO 48 |
| Touch | I²C `0x38` |
| RTC | I²C `0x51` |

More hardware notes are in [docs/HARDWARE.md](docs/HARDWARE.md).

## First connection

1. Flash the firmware and power the board.
2. On first boot, connect your phone or computer to **FlightDot-Setup**.
3. Open `http://192.168.4.1/` and select a nearby 2.4 GHz Wi-Fi network. Enter its password and save.
4. Reconnect your phone or computer to the same home network, then open `http://flightdot.local/`.

If `flightdot.local` is not available on your network, use the IP address printed in the serial monitor.

The configuration page can scan for nearby Wi-Fi networks, set the radar centre by searching for a city or airport, change the range from 10 to 250 km, adjust brightness, enable or disable the sweep, and search for a flight to follow.

## Build and flash

Install PlatformIO, then run:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install platformio

pio run -e radar
pio run -e radar -t upload
pio device monitor -b 115200
```

If PlatformIO does not find the board automatically, list ports and pass the correct one:

```sh
pio device list
pio run -e radar -t upload --upload-port /dev/cu.usbmodemXXXX
```

Useful development environments:

```sh
# Radar with simulated aircraft, no network requests
pio run -e mock -t upload

# Native checks
pio test -e test-native

# Desktop simulator (requires SDL2)
brew install sdl2
pio run -e native -t exec
```

## Data sources

FlightDot tries these public ADS-B sources in order and backs off when a service fails or rate-limits requests:

- [airplanes.live](https://airplanes.live/)
- [ADSB.lol](https://adsb.lol/)
- [adsb.fi](https://adsb.fi/)

[adsbdb](https://github.com/mrjackwills/adsbdb) is used when available to enrich selected aircraft with route and type information. Airport search data comes from [OurAirports](https://ourairports.com/data/), and city search is performed in the browser with [Open-Meteo Geocoding](https://open-meteo.com/en/docs/geocoding-api).

Coverage and aircraft details depend on the public data sources. No aircraft are invented when a provider is unavailable. Please use the services responsibly and respect their terms and rate limits.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/hardware` | AMOLED, touch, RTC, and power hardware |
| `src/net` | Wi-Fi portal, ADS-B client, and local web server |
| `src/ui` | Radar, list, details, statistics, and gestures |
| `src/core` | Data model, parser, storage, mock data, and location helpers |
| `lib/WavesharePanel` | Waveshare/Espressif display support code |
| `data` and `certs` | Airport index and HTTPS certificate bundle |

## Credits

[Capsule Radar](https://github.com/socquique/capsule-radar) was a useful architectural reference. Hardware support is based on the official [Waveshare ESP32-S3-Touch-AMOLED-1.43 documentation](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43). Third-party components and data sources are listed in [THIRD_PARTY.md](THIRD_PARTY.md).
