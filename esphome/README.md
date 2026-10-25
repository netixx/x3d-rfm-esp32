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

The SPI clock defaults to 1 MHz. Faster clocks (`data_rate: 8MHz`) can make
the ESP32 sample MISO too early during FIFO reads: the MSB of each byte then
copies the previous byte's MSB, and every frame fails with "bad CRC".

## Install

Add the component straight from GitHub in your device YAML:

```yaml
external_components:
  - source: github://netixx/x3d-rfm-esp32@esphome-x3d-dvi
    components: [x3d_rfm69]
```

[`x3d-dvi.yaml`](x3d-dvi.yaml) is a complete demo configuration, with one
window and one double sliding door. Copy it, add a
`secrets.yaml` (`wifi_ssid`, `wifi_password`, `api_encryption_key`), then:

```sh
esphome run x3d-dvi.yaml
```

On NixOS, `nix-shell -p esphome --run "esphome run x3d-dvi.yaml"` is enough.

## Declaring windows and doors

Each window or door is one include of a **profile** package from
[`profiles/`](profiles/), straight from GitHub. Pick the profile matching the
opening and give it an `id`, a `name` and its radio ID (see
[Finding device IDs](#finding-device-ids)):

```yaml
packages:
  bathroom_window:
    url: https://github.com/netixx/x3d-rfm-esp32
    ref: esphome-x3d-dvi
    files:
      - path: esphome/profiles/tilt_window.yaml
        vars:
          id: bathroom_window
          name: "Bathroom window"
          x3d_id: "0xEFCC85"
```

Each opening becomes its own device in Home Assistant (an ESPHome
[sub-device](https://esphome.io/components/esphome/#sub-devices)), so you can
give it an area there.

| Profile                    | Entities                                         | Radio ID vars                  |
|----------------------------|--------------------------------------------------|--------------------------------|
| `window`, `door`, `sliding_door` | `Lock`                                     | `x3d_id`                       |
| `tilt_window`              | `Lock`, `Tilted`                                 | `x3d_id`                       |
| `double_window`, `double_sliding_door` | `Left lock`, `Right lock`, combined `Lock` | `left_x3d_id`, `right_x3d_id` |

- `Lock` uses Home Assistant's `lock` device class: shown as *Locked* /
  *Unlocked* (its raw value is ON when unlocked, HA's convention).
- `Tilted` is ON when the handle is in the tilt position.
- The combined `Lock` of the double profiles is *Unlocked* as soon as one sash
  is, *Locked* when both are, and unknown otherwise.
- `window`, `door` and `sliding_door` create the same entities; the name only
  documents your config.

Optional vars, on every profile:

| Var     | Adds                                               |
|---------|----------------------------------------------------|
| `rssi: true`  | `RSSI` sensor in dBm, updated on every frame: acts as "last seen" |
| `state: true` | `State` text sensor: `locked` / `unlocked`  |

Entity ids are `<id>_lock`, `<id>_tilted`, `<id>_rssi`, `<id>_state`, and
`<id>_left_lock`, `<id>_right_lock`... for the double profiles. Customize any
of them with ESPHome's `!extend` (or drop one with `!remove`):

```yaml
binary_sensor:
  - id: !extend bathroom_window_lock
    icon: mdi:window-closed-variant
    filters:
      - delayed_on: 1s
```

### Entities without profiles

The profiles only generate `platform: x3d_rfm69` entities, which you can also
write yourself. `x3d_id` says which radio device an entity follows:

```yaml
binary_sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    type: lock              # lock (default) or tilt
    name: "Bathroom window lock"

text_sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    type: state             # state (default) or last_frame (no x3d_id)
    name: "Bathroom window state"

sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    name: "Bathroom window RSSI"
```

Each X3D device can have at most one entity of each type.

`x3d_id` accepts `0xEFCC85` (as printed in the logs) or the quoted hex string
`"EFCC85"`. Don't write an all-digit ID without `0x` or quotes: YAML reads
`123456` as a decimal number.

The ID is sent little endian: raw bytes `85 cc ef` (bytes 5-7 of the frame)
are device `0xEFCC85`. Copy IDs from the logs or the `last_frame` sensor,
which print them in the right order. If an entity is configured with the raw
byte order, a warning tells you which value to use.

## Finding device IDs

Flash with `log_unclaimed_devices: true` (the default), then turn each handle
one opening at a time. Devices without entities (*unclaimed*) are logged at
INFO, so they stand out: every handle event, and the first message of any
other type (thermostats, Tydom box, ...):

```
[I][x3d_rfm69]: Unclaimed device 0xEFCC85 (window): unlocked (tilt), -67.5 dBm -> claim it with 'x3d_id: 0xEFCC85'
[I][x3d_rfm69]: Unclaimed device 0x5A1B2C: standard message 0x01 (not a handle event), -80.0 dBm. Logged once, next ones at DEBUG
```

At DEBUG every valid X3D frame gets one decoded line, claimed devices with the
name of their Home Assistant device:

```
[D][x3d_rfm69]: 'Bathroom window' (0xEFCC85, window): unlocked (tilt), msg 0x55, -67.5 dBm
[D][x3d_rfm69]: 'Living room door' (0xEFA00B, sliding): unchanged, msg 0x37, -71.0 dBm
[D][x3d_rfm69]: Unclaimed device 0x5A1B2C: standard message 0x01, msg 0x12, -80.0 dBm
[D][x3d_rfm69]: Invalid frame (bad CRC), length byte 19, -95.0 dBm
```

Wording used everywhere: a device is *claimed* or *unclaimed* (has entities or
not); its state is `locked`, `unlocked`, or `unchanged` (a frame with no handle
position, for example `00 00` in bytes 8-9); its kind is `window`, `sliding`,
or `other`.

The `last_frame` text sensor shows the last event of any device in Home
Assistant, for example `0xEFCC85 'Bathroom window': unlocked (tilt) (window, -67.5 dBm)`
or `0xEFA055 unclaimed: locked (sliding, -70.0 dBm)`:

```yaml
text_sensor:
  - platform: x3d_rfm69
    type: last_frame
    name: "Last X3D frame"
```

Log levels:

| Level   | What                                                                 |
|---------|----------------------------------------------------------------------|
| INFO    | Unclaimed devices: handle events, first message of other types       |
| WARN    | Unexpected values in bytes 8-14, with the raw frame                  |
| DEBUG   | One decoded line per frame: device ID, name, message type, state; invalid frames with the reason |
| VERBOSE | Raw bytes of every packet received, radio repeats                    |

To see the raw bytes, set the logger to VERBOSE. Other components get verbose
too; quiet the noisy ones per tag:

```yaml
logger:
  level: VERBOSE          # highest level compiled in
  logs:
    api: DEBUG            # example: keep other tags at DEBUG
    wifi: DEBUG
```

## Entity states after a reboot

The DVI only transmits when the handle moves, so after a reboot nothing is
known until then. With `restore_state: true` (the default), the last handle
position is saved to flash and published at boot. If a handle moved while the
ESP was off, the entity shows the old position until the next event.

An entity Home Assistant shows as **unavailable** while the ESP is online is
usually one the firmware no longer provides, for example after renaming an
opening or changing its profile. Remove it from Home Assistant
(*Settings → Entities*).

## Hub options

```yaml
x3d_rfm69:
  id: x3d_radio            # optional
  cs_pin: GPIO5            # required, SPI chip select (NSS)
  dio0_pin: GPIO4          # optional, PayloadReady interrupt
  frequency: 868.95MHz     # default
  data_rate: 1MHz          # default, see Hardware
  log_unclaimed_devices: true
  restore_state: true      # publish the last saved handle position at boot
  on_opening_event: ...    # automation, see below
```

`on_opening_event` runs once per real event of any device, claimed or not
(radio repeats are dropped), with the variables `device_id` (`"EFCC85"`),
`state`, `tilted`, `kind`, `rssi` and `raw`. For example, to forward every
event to Home Assistant:

```yaml
x3d_rfm69:
  on_opening_event:
    - homeassistant.event:
        event: esphome.x3d_opening
        data:
          device_id: !lambda "return device_id;"
          state: !lambda "return state;"
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
