# ESPHome component: Delta Dore X3D window sensors (K•LINE DVI)

`x3d_rfm69` is an ESPHome external component that listens to Delta Dore **X3D**
868 MHz radio traffic with an **RFM69HW (SX1231)** on an **ESP32**, and decodes
the messages of K•LINE DVI sensors (Détecteur de Verrouillage Intégré). The DVI
reads the window or door handle and reports **locked** or **unlocked**. Opening
the sash doesn't send anything.

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
          kind: !lambda "return kind;"
          rssi: !lambda 'return str_sprintf("%.1f", rssi);'
```

Event data: `device_id` (for example `EFCC85`), `state` (`locked`,
`unlocked`, or `unknown`), `kind` (`window`, `sliding`, or `unknown`), and
`rssi`. Watch them live
in Home Assistant under *Developer tools → Events → Listen to
`esphome.x3d_window`*.

[`homeassistant/x3d_windows.yaml`](homeassistant/x3d_windows.yaml) turns these
events into trigger-based template entities: one block per window, with the ID
and names set in Home Assistant. Paste its `template:` block into
`configuration.yaml`, or drop the file in `<config>/packages/` if you use
[packages](https://www.home-assistant.io/docs/configuration/packages/). Those
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
    lock:
      name: "Tilt window lock"         # ON = unlocked (device_class lock)

text_sensor:
  - platform: x3d_rfm69
    device_id: 0xEFCC85
    state:
      name: "Tilt window state"        # locked / unlocked

sensor:
  - platform: x3d_rfm69
    device_id: 0xEFCC85
    rssi:
      name: "Tilt window RSSI"         # updated on each event: acts as "last seen"
```

`device_id` accepts `0xEFCC85` or `"EFCC85"` (as printed in the logs).

## Finding device IDs

Flash with `log_unknown_devices: true` (the default), then turn each handle
one window at a time. Devices not claimed by a `device_id` in your YAML are
logged at INFO, so they stand out:

```
[I][x3d_rfm69]: Unclaimed device 0xEFCC85 (window): unlocked, -67.5 dBm -> add 'device_id: 0xEFCC85' to claim it
```

The hub-wide `last_frame` text sensor shows every device in Home Assistant
(`EFCC85 unlocked (window, msg 0x55, -67.5 dBm)`):

```yaml
text_sensor:
  - platform: x3d_rfm69
    last_frame:
      name: "Last X3D frame"
```

Log levels:

| Level   | What                                                                 |
|---------|----------------------------------------------------------------------|
| INFO    | Events of unclaimed devices (with `log_unknown_devices: true`)       |
| WARN    | Unexpected values in bytes 8-14, with the raw frame                  |
| DEBUG   | Events of claimed devices, every raw packet with `log_raw_frames`     |
| VERBOSE | Raw bytes of each event, radio repeats, ignored non-sensor X3D traffic |

## Hub options

```yaml
x3d_rfm69:
  id: x3d_radio            # needed only with several radios (x3d_rfm69_id on platforms)
  cs_pin: GPIO5            # required, SPI chip select (NSS)
  dio0_pin: GPIO4          # optional, PayloadReady interrupt
  frequency: 868.95MHz     # default
  log_unknown_devices: true
  log_raw_frames: false    # log every packet received, including other X3D traffic
  on_window_event: ...     # variables: device_id, state, kind, rssi, raw
```

## Decoding notes

- RF: FSK 40 kbps, 80 kHz deviation, sync `81 69 96 7e`, hardware whitening.
  The SX1231 register setup is a straight port of `x3d-raw-monitor`.
- CRC: CRC-16/XMODEM (poly `0x1021`, init `0x0000`) over the length byte and
  payload, stored big endian. It matches both reference captures.
- Header checksum: `0x10000 - sum(device id .. byte before checksum)`, big
  endian, checked as well.
- Frame bytes 8-14, from captures of a tilt (soufflet) window and a sliding
  door:

  | Device            | Locked                 | Unlocked               |
  |-------------------|------------------------|------------------------|
  | Tilt window       | `00 01 82 00 20 00 00` | `08 41 82 00 20 00 00` |
  | Sliding door      | `00 01 82 00 40 00 00` | `04 41 82 00 40 00 00` |

  State comes from byte 9: bit `0x40` = unlocked, `0x01` = locked, `0x00` =
  "not changed" (entities keep their value). Byte 12 gives the `kind`:
  `0x20` window, `0x40` sliding. That matches Tydom's `windowFrench` /
  `windowSliding` device types. Tydom reports `LOCKED` / `OPEN_FRENCH` /
  `OPEN_HOPPER` for French windows, so a tilt-and-turn window may use another
  byte 8 value in the tilt position. Captures welcome.
- Each event is sent 3 times ~20 ms apart, then the same burst again
  ~3.5-5.3 s later. Both are dropped by message number for 30 s.
- Values in bytes 8-14 that don't match the table (possibly battery low or
  tamper) are logged at WARN level with the raw frame. Please report them.

## Tests

Host-side unit tests for the parser (CRC, header checksum, state decoding,
de-duplication), with no hardware needed:

```sh
make -C esphome/tests
```
