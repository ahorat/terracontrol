# terracontrol

Firmware for a WiFi-enabled 4-channel relay controller (230VAC/5A per channel)
built on an ESP32-C6, with an RTC-backed scheduler that keeps running
independently of the network, and a web UI served directly from the device.

Each of the 4 channels can be configured independently as:

- **Interval** — repeats every N minutes for M seconds, optionally restricted
  to a time window and/or specific weekdays.
- **Fixed time** — on/off at fixed times of day, optionally restricted to
  specific weekdays.
- **Sunrise/sunset** — switches relative to sunrise/sunset for Bern,
  Switzerland (fixed location, DST-aware), with a per-edge offset in minutes.
- **Manual override** — force a channel on/off for a set duration, or
  indefinitely, from the dashboard. Takes priority over the schedule while
  active and reverts automatically when it expires.

See [`aufgabenbeschreibung-relaiscontroller.md`](aufgabenbeschreibung-relaiscontroller.md)
for the original requirements this implements.

## Hardware

| Component | Notes |
|---|---|
| MCU board | Waveshare ESP32-C6-Zero (ESP32-C6FH4, RISC-V, 4MB flash) |
| RTC | DS3231 with AT24C32 EEPROM (I2C breakout) |
| Relays | 4-channel module, 5V/opto-isolated, **active-LOW** by default |
| Reset button | Momentary, wired to GND (internal pull-up, active-LOW) |
| Status LED | Onboard WS2812 RGB LED (no wiring needed) — see [Status LED](#status-led) |

### Wiring

| Signal | GPIO |
|---|---|
| Relay 1 | 0 |
| Relay 2 | 1 |
| Relay 3 | 2 |
| Relay 4 | 3 |
| Reset button (other leg → GND) | 4 |
| I2C SDA (DS3231 + AT24C32) | 21 |
| I2C SCL (DS3231 + AT24C32) | 22 |

All GPIOs above are unremarkable on the ESP32-C6 (no strapping/UART/USB
role). The board's own USB-C port, onboard BOOT button, and onboard WS2812
LED occupy GPIO8/9/12/13 and are intentionally left alone — see the comment
block at the top of `include/Config.h` for the full reasoning if you need to
remap pins.

**Relay polarity**: if your relay board energizes on HIGH instead of LOW,
flip `RELAY_ACTIVE_LOW` in `include/Config.h`.

Common ground between the ESP32-C6, the relay board's logic side, and the
RTC module is required. Keep the 230VAC side of the relay board fully
isolated and verify your own mains wiring — this firmware only concerns
itself with the low-voltage control side.

## Building and flashing

This is a [PlatformIO](https://platformio.org/) project.

```
pip install platformio
git clone <this-repo>
cd terracontrol
pio run -e esp32-c6-zero -t upload
```

PlatformIO auto-detects the upload port; if it picks the wrong one, add
`--upload-port /dev/ttyACM0` (or your OS's equivalent).

### Why a non-default platform

The official PlatformIO `espressif32` platform does not currently ship
Arduino-framework support for the ESP32-C6 (its bundled `arduino-esp32` core
is missing the `esp32c6` variant, even though its own board list mentions
C6 boards). This project uses the community-maintained
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork
instead, pinned to a specific release tag in `platformio.ini` for
reproducibility. If PlatformIO ships proper upstream C6 support later, this
can be switched back.

### Serial console

The ESP32-C6-Zero has no separate USB-UART bridge chip — the same USB-C port
used for flashing carries the native USB-Serial/JTAG console. Two
consequences, both already handled in `platformio.ini`/`main.cpp`:

- `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1` are required for
  `Serial` to actually route over USB-C at all.
- Every reset or power-cycle makes the USB device re-enumerate, so any
  attached monitor session drops and has to reattach. `setup()` waits 8s
  before printing anything, to give a monitor time to reconnect first. To
  reliably capture the very first boot lines, start the monitor in a retry
  loop *before* plugging in / resetting the board:

  ```
  until pio device monitor -e esp32-c6-zero; do sleep 0.5; done
  ```

## First-time setup

1. On first boot (no saved WiFi credentials), the device opens its own
   access point, **`RelayController-Setup`** (open, no password), and serves
   the web UI at `http://192.168.4.1/`.
2. Connect to that AP, open the address above, go to the **WLAN** tab, and
   enter your target network's SSID/password.
3. The device switches to client mode and joins your network. From then on,
   it's reachable at **`http://terracontrol.local/`** (mDNS) or via whatever
   IP your router assigns it.
4. Holding the reset button (GPIO4) for more than 3 seconds at any time
   switches the device back into AP mode, regardless of current state.
5. If the device loses its WiFi connection, relay scheduling is entirely
   unaffected (it only depends on the RTC); it retries reconnecting in the
   background roughly every 5 minutes.

## Status LED

The board's onboard WS2812 RGB LED blinks at ~0.5Hz (once per second) as a
WiFi state indicator, so you can tell what's going on without opening the
web UI:

| Color | Meaning |
|---|---|
| 🔴 Red | No WiFi credentials saved yet (AP mode, needs first-time setup) |
| 🔵 Blue | Credentials saved, but not currently connected (includes AP mode forced via the reset button, and STA mode while reconnecting) |
| 🟢 Green | Connected |

Driven via the ESP32 core's built-in `rgbLedWriteOrdered()` (no external
library) in `WifiController::updateStatusLed()`. If a future board revision
uses a different WS2812 byte order and colors come out swapped, adjust
`RGB_LED_COLOR_ORDER` in `include/Config.h`.

## Web UI

Single page, four tabs:

- **Status** — per-channel current state, active mode, next scheduled
  switch time, and override controls (on/off with a duration in minutes and
  seconds, or 0:0 for "until cleared").
- **Kanäle** — per-channel mode and parameters.
- **WLAN** — connection status, target network setup, and a manual "back to
  AP mode" button.
- **Zeit** — RTC validity, current time, last NTP sync time, and manual
  time entry.

It talks to the device purely via a small JSON API under `/api/*`
(`status`, `config`, `override`, `wifi`, `time` — see `src/WebPortal.cpp`
for the exact request/response shapes).

## Time handling

- The DS3231 always stores **local** (Europe/Zurich) wall-clock time, not
  UTC — this keeps schedule and sunrise/sunset comparisons free of timezone
  math elsewhere in the code.
- NTP sync (on boot, then every 12h) fetches UTC and converts it to local
  time using the EU DST rule (last Sunday of March/October) before writing
  it to the RTC.
- If the RTC reports lost power or an implausible date, all relays are
  forced off (failsafe) until a valid time is available (NTP or manual
  entry via the web UI).

## Persistent storage

- **Channel configuration** (mode + parameters per channel) is written
  immediately to the AT24C32 EEPROM on the RTC module whenever changed via
  the web UI, so it survives power loss and firmware reflashes (it's a
  separate physical chip from the ESP32's own flash).
- **WiFi credentials** are stored in the ESP32's own NVS (via `Preferences`),
  separately from the RTC EEPROM.
- **Manual overrides are not persisted** — they're runtime-only and reset
  on reboot.

## Project layout

```
include/            Header files (one class per hardware/logic concern)
src/                 Implementation + main.cpp (setup/loop)
  Config.h/.cpp       Pin assignment and other tunables
  ChannelConfig.h      Per-channel schedule data + EEPROM byte layout
  EepromStore          AT24C32 driver + config load/save
  WifiCredStore        WiFi credentials in NVS
  RtcClock             DS3231 wrapper, local/UTC conversion, DST rule
  SunTimes             Sunrise/sunset calculation for Bern
  RelayChannel         Per-channel schedule evaluation + override + GPIO
  WifiController       AP/STA switching, reset button, reconnect, mDNS, status LED
  WebPortal/WebAssets  Web UI (embedded HTML/CSS/JS) + JSON API
  main.cpp             Wires everything together; scheduler tick + NTP
```

All 4 relay channels share the same `RelayChannel` implementation — only the
GPIO pin and per-channel `ChannelConfig` differ.

## Known limitations / tuning knobs

- **Flash usage is at ~90%** of the default 1.25MB app partition (4MB flash,
  two-OTA-slot layout). There's headroom for small additions, but a larger
  feature may need a custom partition table (e.g. a single larger app slot,
  since OTA isn't a requirement here) — see `board_build.partitions` in
  PlatformIO's docs if you hit "region overflowed" at link time.
- **8-second boot delay** (`delay(8000)` at the top of `setup()` in
  `main.cpp`) exists purely to give a USB serial monitor time to reattach
  after the native-USB re-enumeration on reset (see Serial console above).
  It doesn't affect correctness (relays stay in failsafe-off until the RTC
  is read regardless), but it can be shortened or removed once you're done
  with active debugging.
- **DST rule is a fixed EU calculation** (last Sunday of March/October), not
  looked up from a timezone database — correct for Switzerland indefinitely
  unless EU DST rules change. The local-date-based variant used for
  sunrise/sunset offset selection has one known, intentional simplification:
  during the one hour that gets repeated on the autumn transition night, it
  may pick the wrong side of the DST boundary — inconsequential here since
  it only affects which of two ~1h-apart sun times is used that one night a
  year.
- **Relay board polarity and RGB LED byte order** are per-board electrical
  characteristics, not something firmware can detect automatically — both
  are exposed as constants in `include/Config.h` (`RELAY_ACTIVE_LOW`,
  `RGB_LED_COLOR_ORDER`) in case you swap to different hardware.

## License

MIT — see [`LICENSE`](LICENSE).
