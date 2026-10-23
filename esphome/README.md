# ESPHome component: Delta Dore X3D lock detectors (K•LINE DVI)

`x3d_rfm69` is an ESPHome external component that listens to Delta Dore **X3D**
868 MHz radio traffic with an **RFM69HW (SX1231)** on an **ESP32**, and decodes
the messages of K•LINE DVI sensors (Détecteur de Verrouillage Intégré) built
into windows and doors, called *openings* here ("ouvrants"). The DVI reads the
handle and reports **locked** or **unlocked**, plus the **tilt** position on
tilt windows. Opening the sash doesn't send anything.

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

## Declaring openings

List each window or door once, with its ID and a name:

```yaml
x3d_rfm69:
  cs_pin: GPIO5
  dio0_pin: GPIO4
  openings:
    - device_id: 0xEFCC85
      name: "Tilt window"
      tilt: true
    - device_id: 0xEFA00B
      name: "Sliding door"
```

Each entry is expanded into regular ESPHome entities, so filters, automations
(`on_state`, ...), `internal` and the other entity options work as usual:

| Key     | Entity                         | Default | Content                                   |
|---------|--------------------------------|---------|-------------------------------------------|
| `lock`  | `<name> lock` binary sensor    | on      | Shown as *Locked* / *Unlocked* in HA      |
| `tilt`  | `<name> tilted` binary sensor  | off     | ON when the handle is in tilt position    |
| `state` | `<name> state` text sensor     | off     | `locked` / `unlocked`                     |
| `rssi`  | `<name> RSSI` sensor           | off     | dBm, updated on each event: acts as "last seen" |

Set a key to `true` to enable it, `false` to disable it, or give it entity
options:

```yaml
  openings:
    - device_id: 0xEFA00B
      name: "Sliding door"
      rssi: true
      lock:
        name: "Sliding door handle"
```

The `lock` binary sensor uses Home Assistant's `lock` device class. HA displays
it as *Locked* / *Unlocked*; its raw value is ON when unlocked (HA's
convention for this class).

`tilt` is only useful on tilt windows (soufflet, tilt-and-turn). The radio ID
doesn't say what kind of opening a detector is fitted to, so you enable it per
opening. The tilt position itself is read from the frames.

`device_id` accepts `0xEFCC85` or `"EFCC85"` (as printed in the logs).

To show each opening as its own device in Home Assistant (with its own area),
declare a [sub-device](https://esphome.io/components/esphome/#sub-devices) and
reference it with `device:`. Its entities are then named `Lock`, `Tilted`,
`State` and `RSSI` inside that device:

```yaml
esphome:
  devices:
    - id: tilt_window
      name: "Tilt window"
      area_id: bathroom      # optional, from esphome: areas:

x3d_rfm69:
  openings:
    - device_id: 0xEFCC85
      name: "Tilt window"
      device: tilt_window
      tilt: true
```

## Finding device IDs

Flash with `log_unclaimed_devices: true` (the default), then turn each handle
one opening at a time. Devices not listed under `openings:` (*unclaimed*) are
logged at INFO, so they stand out:

```
[I][x3d_rfm69]: Unclaimed device 0xEFCC85 (window): unlocked (tilt), -67.5 dBm -> add it under 'openings:' with 'device_id: 0xEFCC85'
```

Once claimed, the device is logged at DEBUG with its name:

```
[D][x3d_rfm69]: 'Tilt window' (0xEFCC85, window): unlocked (tilt), -67.5 dBm
[D][x3d_rfm69]: 'Sliding door' (0xEFA00B, sliding): state unchanged (no handle position in this frame), keeping current value, -71.0 dBm
```

Wording used everywhere: a device is *claimed* or *unclaimed* (listed under
`openings:` or not); its state is `locked`, `unlocked`, or `unchanged` (a frame
with no handle position, for example `00 00` in bytes 8-9); its kind is
`window`, `sliding`, or `other`.

The hub's `last_frame` text sensor shows the last event of any device in Home
Assistant, for example `EFCC85 'Tilt window': unlocked (tilt) (window, -67.5 dBm)`
or `EFA055 unclaimed: locked (sliding, -70.0 dBm)`:

```yaml
x3d_rfm69:
  last_frame:
    name: "Last X3D frame"
```

Log levels:

| Level   | What                                                                 |
|---------|----------------------------------------------------------------------|
| INFO    | Events of unclaimed devices (with `log_unclaimed_devices: true`)     |
| WARN    | Unexpected values in bytes 8-14, with the raw frame                  |
| DEBUG   | Events of claimed devices, every raw packet with `log_raw_frames`    |
| VERBOSE | Raw bytes of each event, radio repeats, ignored non-sensor X3D traffic |

## Mapping IDs in Home Assistant instead (no reflash per opening)

`on_opening_event` fires once per real event of **any** device (the radio
repeats are dropped). The demo forwards it as a Home Assistant event:

```yaml
x3d_rfm69:
  on_opening_event:
    - homeassistant.event:
        event: esphome.x3d_opening
        data:
          device_id: !lambda "return device_id;"
          state: !lambda "return state;"
          tilted: !lambda 'return tilted ? "true" : "false";'
          kind: !lambda "return kind;"
          rssi: !lambda 'return str_sprintf("%.1f", rssi);'
```

Watch the events in Home Assistant under *Developer tools → Events → Listen to
`esphome.x3d_opening`*.
[`homeassistant/x3d_openings.yaml`](homeassistant/x3d_openings.yaml) turns
them into trigger-based template entities, one block per opening. Paste its
`template:` block into `configuration.yaml`, or drop the file in
`<config>/packages/` if you use
[packages](https://www.home-assistant.io/docs/configuration/packages/). Those
entities keep their state across Home Assistant restarts. While Home Assistant
is disconnected from the ESP, events are lost; the opening catches up on its
next change.

Events whose name starts with `esphome.` are accepted from any ESPHome device.
You don't need to enable "Allow the device to perform Home Assistant actions".

## Hub options

```yaml
x3d_rfm69:
  id: x3d_radio            # optional
  cs_pin: GPIO5            # required, SPI chip select (NSS)
  dio0_pin: GPIO4          # optional, PayloadReady interrupt
  frequency: 868.95MHz     # default
  log_unclaimed_devices: true
  log_raw_frames: false    # log every packet received, including other X3D traffic
  last_frame: ...          # optional text sensor, last event of any device
  openings: ...            # claimed devices and their entities
  on_opening_event: ...    # variables: device_id, state, tilted, kind, rssi, raw
```

## Decoding notes

- RF: FSK 40 kbps, 80 kHz deviation, sync `81 69 96 7e`, hardware whitening.
  The SX1231 register setup is a straight port of `x3d-raw-monitor`.
- CRC: CRC-16/XMODEM (poly `0x1021`, init `0x0000`) over the length byte and
  payload, stored big endian.
- Header checksum: `0x10000 - sum(device id .. byte before checksum)`, big
  endian, checked as well.
- Frame bytes 8-14, from captures of a tilt (soufflet) window and a sliding
  door:

  | Device            | Locked                 | Unlocked               |
  |-------------------|------------------------|------------------------|
  | Tilt window       | `00 01 82 00 20 00 00` | `08 41 82 00 20 00 00` |
  | Sliding door      | `00 01 82 00 40 00 00` | `04 41 82 00 40 00 00` |

  State comes from byte 9: bit `0x40` = unlocked, `0x01` = locked, `0x00` =
  "not changed" (entities keep their value). Byte 8 bit `0x08` is read as the
  tilt position (Tydom's `OPEN_HOPPER`), `0x04` as a plain unlock. Byte 12 gives
  the `kind`: `0x20` window, `0x40` sliding, matching Tydom's `windowFrench` /
  `windowSliding` device types.
- Not captured yet: a tilt-and-turn window with the handle horizontal (turn
  position, Tydom's `OPEN_FRENCH`). If it also sets `0x08`, the tilt sensor
  would be wrong on those windows. Captures welcome.
- Each event is sent 3 times ~20 ms apart, then the same burst again
  ~3.5-5.3 s later. Both are dropped by message number for 30 s.
- Values in bytes 8-14 that don't match the table (possibly battery low or
  tamper) are logged at WARN level with the raw frame. Please report them.

## Tests

Host-side unit tests for the parser (CRC, header checksum, state and tilt
decoding, de-duplication, replay of real captures), with no hardware needed:

```sh
make -C esphome/tests
```
