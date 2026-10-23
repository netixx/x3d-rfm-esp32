# ESPHome component: Delta Dore X3D window sensors (K•LINE DVI)

`x3d_rfm69` is an ESPHome external component that listens to Delta Dore **X3D**
868 MHz radio traffic with an **RFM69HW (SX1231)** on an **ESP32**, and decodes
the window sensor messages of K•LINE DVI window sensors: open, closed and
unlocked, closed and locked.

It only receives. It doesn't pair or transmit, so it doesn't interfere with an
existing Delta Dore / Tydom installation.

## Hardware

DOIT ESP32 DEVKIT V1 (`esp32dev`) and a HopeRF RFM69HW 868 MHz with an ~8.2 cm
wire antenna on the ANT pad.

| ESP32  | RFM69 |
|--------|-------|
| 3V3    | 3.3V  |
| GND    | GND   |
| GPIO4  | DIO0  |
| GPIO5  | NSS   |
| GPIO18 | SCK   |
| GPIO19 | MISO  |
| GPIO23 | MOSI  |

`dio0_pin` is optional. Without it the component polls the radio over SPI in
every loop iteration.

## Install

Add the component straight from GitHub in your device YAML:

```yaml
external_components:
  - source: github://netixx/x3d-rfm-esp32@esphome-x3d-dvi
    components: [x3d_rfm69]
```

[`x3d-dvi.yaml`](x3d-dvi.yaml) is a complete demo configuration. Copy it, add a
`secrets.yaml` (`wifi_ssid`, `wifi_password`, `api_encryption_key`), then:

```sh
esphome run x3d-dvi.yaml
```

On NixOS, `nix-shell -p esphome --run "esphome run x3d-dvi.yaml"` is enough.

## Getting windows into Home Assistant

ESPHome entities are created at compile time, so the firmware can't add a new
entity on its own when an unknown window shows up. There are two ways to get
your windows into Home Assistant. You can use both at the same time.

### A. Map the IDs in Home Assistant (no reflash per window)

`on_window_event` fires once per real event of **any** device (the 3 radio
repeats are dropped). The demo forwards it as a Home Assistant event:

```yaml
x3d_rfm69:
  on_window_event:
    - homeassistant.event:
        event: esphome.x3d_window
        data:
          device_id: !lambda "return device_id;"
          state: !lambda "return state;"
          rssi: !lambda 'return str_sprintf("%.1f", rssi);'
```

Event data: `device_id` (for example `EFCC85`), `state` (`open`,
`closed_unlocked`, `closed_locked`, or `unknown`), and `rssi`. Watch them live
in Home Assistant under *Developer tools → Events → Listen to
`esphome.x3d_window`*.

[`homeassistant/x3d_windows.yaml`](homeassistant/x3d_windows.yaml) is a Home
Assistant package that turns these events into trigger-based template entities:
one block per window, with the ID and names set in Home Assistant. Those
entities keep their state across Home Assistant restarts. While Home Assistant
is disconnected from the ESP, events are lost; the window catches up on its
next change.

Events whose name starts with `esphome.` are accepted from any ESPHome
device. You don't need to enable "Allow the device to perform Home Assistant
actions".

### B. Map the IDs in ESPHome (native entities)

Declare each window with its ID:

```yaml
binary_sensor:
  - platform: x3d_rfm69
    device_id: 0xEFCC85
    opening:
      name: "Living room window"       # ON = open (device_class window)
    lock:
      name: "Living room window lock"  # ON = unlocked (device_class lock)

text_sensor:
  - platform: x3d_rfm69
    device_id: 0xEFCC85
    state:
      name: "Living room window state" # open / closed_unlocked / closed_locked

sensor:
  - platform: x3d_rfm69
    device_id: 0xEFCC85
    rssi:
      name: "Living room window RSSI"  # updated on each event: acts as "last seen"
```

`device_id` accepts `0xEFCC85` or `"EFCC85"` (as printed in the logs).

## Finding device IDs

Flash with `log_unknown_devices: true` (the default), then open and close each
window one at a time:

```
[I][x3d_rfm69]: Unconfigured device 0xEFCC85: open (msg 0x42, lock 0x08, sash 0x41, -67.5 dBm) raw: 13 ff 42 00 2d 85 cc ef 08 41 82 00 20 00 00 fc d5 43 54
```

The hub-wide `last_frame` text sensor shows the same thing in Home Assistant
(`EFCC85 open (msg 0x42, -67.5 dBm)`):

```yaml
text_sensor:
  - platform: x3d_rfm69
    last_frame:
      name: "Last X3D frame"
```

Once a window is declared (option B) its log line reads `Window 0xEFCC85: ...`.

## Hub options

```yaml
x3d_rfm69:
  id: x3d_radio            # needed only with several radios (x3d_rfm69_id on platforms)
  cs_pin: GPIO5            # required, SPI chip select (NSS)
  dio0_pin: GPIO4          # optional, PayloadReady interrupt
  frequency: 868.95MHz     # default
  log_unknown_devices: true
  log_raw_frames: false    # log every packet received, including other X3D traffic
  on_window_event: ...     # variables: device_id, state, rssi, raw
```

## Decoding notes

- RF: FSK 40 kbps, 80 kHz deviation, sync `81 69 96 7e`, hardware whitening.
  The SX1231 register setup is a straight port of `x3d-raw-monitor`.
- CRC: CRC-16/XMODEM (poly `0x1021`, init `0x0000`) over the length byte and
  payload, stored big endian. It matches both reference captures.
- Header checksum: `0x10000 - sum(device id .. byte before checksum)`, big
  endian, checked as well.
- State: sash byte bit `0x40` = open. Otherwise lock byte bit `0x08` = unlocked,
  else locked. If the sash byte has neither `0x40` nor `0x01` set, the state is
  `unknown` and entities keep their value.
- Unexpected values in bytes 8-14 (possibly battery low or tamper) are logged
  at WARN level with the raw frame. Please report them.
- The `closed_unlocked` state (`08 01`) is inferred and hasn't been captured
  yet. Check the logs the first time you close a window without turning the
  handle.

## Tests

Host-side unit tests for the parser (CRC, header checksum, state decoding,
de-duplication), with no hardware needed:

```sh
make -C esphome/tests
```
