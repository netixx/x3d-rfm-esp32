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

Tested with ESPHome 2026.9 and 2026.10. ESPHome caches GitHub sources for a
day: add `refresh: 0s` to the `external_components` source and to the package
to pick up changes right away.

## Declaring windows and doors

Each window or door is one **profile** from [`profiles/`](profiles/), loaded
straight from GitHub. One package lists them all, so the URL appears once: add
one `files` entry per window or door, with the profile matching it, an `id`, a
`name` and its radio ID (see [Finding device IDs](#finding-device-ids)):

```yaml
packages:
  x3d_openings:
    url: https://github.com/netixx/x3d-rfm-esp32
    ref: esphome-x3d-dvi
    files:
      - path: esphome/profiles/tilt_window.yaml
        vars:
          id: bathroom_window
          name: "Bathroom window"
          x3d_id: "0xEFCC85"
      - path: esphome/profiles/double_sliding_door.yaml
        vars:
          id: living_room_door
          name: "Living room door"
          left_x3d_id: "0xEFA00B"
          right_x3d_id: "0xEFACF4"
          rssi: true
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
  is, *Locked* when both are, and unknown otherwise. Set `sash_entities: false`
  to keep only that combined entity. Home Assistant can't show per-sash
  details inside one entity (ESPHome entities have no attributes); with
  `sash_entities: true` the device page lists `Left lock` / `Right lock`
  next to it.
- `window`, `door` and `sliding_door` create the same entities; the name only
  documents your config.

Optional vars, on every profile:

| Var     | Adds                                               |
|---------|----------------------------------------------------|
| `sash_entities: false` | double profiles only: keep the combined `Lock`, drop `Left lock` / `Right lock` |
| `rssi: true`  | `RSSI` sensor in dBm, updated on every frame: acts as "last seen" |
| `state: true` | `State` text sensor: `locked` / `unlocked`  |
| `battery: true` | `Battery` binary sensor (low = ON): suspected low battery flag, or silent past the hub `timeout`, see [Silent sensors](#silent-sensors-and-dead-batteries) |
| `connectivity: true` | `Connectivity` binary sensor: OFF when the DVI sent nothing for the hub `timeout` (needs `timeout`, see [Silent sensors](#silent-sensors-and-dead-batteries)) |
| `lock_entity: true` | read-only `Handle` lock entity (combined state on double profiles), a workaround, see [below](#workaround-security-section-of-the-area-dashboard) |

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

### Workaround: Security section of the area dashboard

Home Assistant's area (rooms) dashboard only lists `lock`, `alarm_control_panel`
and `camera` entities in its **Security** section. The `Lock` binary sensor
(device class `lock`) shows in the global Security dashboard, but not there.

As a workaround, `lock_entity: true` adds one read-only **lock** entity per
opening (`Handle`), which the area dashboard picks up. It follows the handle
like the binary sensor; on double profiles it has the combined state. The radio only receives, so the Lock / Unlock buttons Home
Assistant shows can't work: commands are refused (logged as a warning) and the
real state is published again. Prefer the binary sensor everywhere else.

The proper fix belongs in Home Assistant's frontend (list `binary_sensor` with
device class `lock` in the area Security section, as the Security dashboard
already does).

### Entities without profiles

The profiles only generate `platform: x3d_rfm69` entities, which you can also
write yourself. `x3d_id` says which radio device an entity follows:

```yaml
binary_sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    type: lock              # lock (default), tilt, connectivity or battery
    name: "Bathroom window lock"
  - platform: x3d_rfm69
    x3d_id: [0xEFA00B, 0xEFACF4]   # several sashes: combined lock
    name: "Living room door lock"

text_sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    type: state             # state (default) or last_frame (no x3d_id)
    name: "Bathroom window state"

sensor:
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85
    name: "Bathroom window RSSI"

lock:                       # read-only workaround, see above
  - platform: x3d_rfm69
    x3d_id: 0xEFCC85        # or a list: combined state
    name: "Bathroom window handle"
```

Each X3D device can have at most one entity of each type. Every entity also
takes `enabled: false`, which leaves it out of the firmware; the profiles use
it for their optional entities (`enabled: ${rssi}`), as conditional includes
need ESPHome 2026.10.

`x3d_id` accepts `0xEFCC85` (as printed in the logs) or the quoted hex string
`"EFCC85"`. Don't write an all-digit ID without `0x` or quotes: YAML reads
`123456` as a decimal number.

The ID is sent little endian: raw bytes `85 cc ef` (bytes 5-7 of the frame)
are device `0xEFCC85`. Copy IDs from the logs or the `last_frame` sensor,
which print them in the right order. If an entity is configured with the raw
byte order, a warning tells you which value to use.

## Finding device IDs

Flash with `log_unclaimed_devices: true` (the default), then turn each handle
one opening at a time. A DVI that stays silent can be woken up by passing a
strong magnet in front of it a few times (see
[Decoding notes](#decoding-notes)). Devices without entities (*unclaimed*) are logged at
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

### Tracing some devices

With many openings, DEBUG gets hard to read. List the IDs to follow under
`trace:`: every frame they send (new ones and radio repeats, raw bytes and
decoded state) is logged at INFO under its own tag, `x3d_rfm69.trace`. ESPHome
filters logs by tag, so quiet the main tag to see only the traced devices:

```yaml
x3d_rfm69:
  trace: [0xEFCC85, 0xEFA214]

logger:
  level: DEBUG
  logs:
    x3d_rfm69: WARN       # hide the per-frame lines of all the other devices
```

```
[I][x3d_rfm69.trace]: 0xEFCC85 new sensor message, msg 0x55, -67.5 dBm: 13 ff 55 00 2d 85 cc ef 08 41 82 00 20 00 00 fc d5 75 49
[I][x3d_rfm69.trace]: 0xEFCC85 decoded: unlocked (tilt) (window), bytes 8-9: 08 41
[I][x3d_rfm69.trace]: 0xEFCC85 repeat sensor message, msg 0x55, -68.0 dBm: 13 ff 55 00 2d 85 cc ef 08 41 82 00 20 00 00 fc d5 75 49
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

## Silent sensors and dead batteries

DVIs run on batteries. When a battery dies the sensor just stops sending, and
its entities would keep their last state forever. With a hub `timeout`, a
claimed device that sent no frame at all for that long gets its entities
cleared (*unknown* in Home Assistant: ESPHome can't make a single entity
*unavailable*) and a warning is logged:

```
[W][x3d_rfm69]: 'Bathroom window' (0xEFCC85): no frame for 1560 min, entities set to unknown. Battery?
```

The next frame of any kind brings it back: a "not changed" heartbeat
publishes the last known position again. The `connectivity` entity (profile var
`connectivity: true`, or `type: connectivity`) shows the same as a binary
sensor you can alert on.

The `battery` entity (profile var `battery: true`, or `type: battery`, Home
Assistant's battery class: ON = low) turns on when a frame carries the
suspected low battery flag, or when the device went silent past the
`timeout`. The flag is a guess: standard X3D messages use bit `0x10` of their
first payload byte for "battery low" ([X3D-Protocol.md](../X3D-Protocol.md)),
and the DVI's byte 8 looks like the same flags byte (the door sends `05`
there, like their `05 98` header), so a low DVI should send `1x` in byte 8.
Not seen yet: frames with that bit are still reported with a warning, please
send them.

The timeout is **off by default**: how often a DVI sends a heartbeat when
nothing happens isn't known yet (Tydom shows a `supervisionMode` setting,
`SHORT` / `LONG`). Measure it first: with `rssi: true`, the RSSI sensor updates
on every frame, so its history shows the gaps; or watch the `unchanged` lines
at DEBUG. Then set the timeout to 2-3 times the longest gap:

```yaml
x3d_rfm69:
  timeout: 26h
```

Too short a timeout blanks working sensors between heartbeats.

With MQTT, numeric sensors also accept ESPHome's `expire_after`: Home
Assistant then shows them *unavailable* when no value arrives in time. The RSSI
sensor publishes on every frame, so it fits (there's no equivalent for binary
sensors or for the native API):

```yaml
sensor:
  - id: !extend bathroom_window_rssi
    expire_after: 26h
```

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
  timeout: 0s              # off; clear entities of devices silent this long
  trace: []                # IDs whose frames are all logged, tag x3d_rfm69.trace
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
- Frame bytes 8-14, from captures of a tilt (soufflet) window, a sliding door
  and a door:

  | Device            | Locked                 | Unlocked               |
  |-------------------|------------------------|------------------------|
  | Tilt window       | `00 01 82 00 20 00 00` | `08 41 82 00 20 00 00` |
  | Sliding door      | `00 01 82 00 40 00 00` | `04 41 82 00 40 00 00` |
  | Door              | not captured yet       | `05 41 82 00 50 00 04` |

  State comes from byte 9: bit `0x40` = unlocked, `0x01` = locked, `0x00` =
  "not changed" (entities keep their value). Byte 8 bit `0x08` is read as the
  tilt position (Tydom's `OPEN_HOPPER`), `0x04` as a plain unlock. Byte 12 gives
  the `kind`: `0x20` window, `0x40` sliding, `0x50` door, matching Tydom's
  `windowFrench` / `windowSliding` / door device types. On the door, byte 8 bit
  `0x01` and byte 14 `0x04` are not understood yet (see the activation notes
  below, they may be a test mode).
- Not captured yet: a tilt-and-turn window with the handle horizontal (turn
  position, Tydom's `OPEN_FRENCH`). If it also sets `0x08`, the tilt sensor
  would be wrong on those windows. Captures welcome.
- Each event is sent 3 times ~20 ms apart, then the same burst again
  ~3.5-5.3 s later. Both are dropped by message number for 30 s.
- Values in bytes 8-14 that don't match the table (possibly battery low or
  tamper) are logged at WARN level with the raw frame, once per device and
  pattern (repeats at DEBUG). Please report them.
- **Activation and announce frames** (observed, not documented anywhere we
  found: the repo's [X3D-Protocol.md](../X3D-Protocol.md) only covers Tydom
  pairing messages, and Delta Dore / K•LINE describe pairing through the Tydom
  app). Passing a strong magnet in front of a DVI a few times wakes it up:
  - it sends an **announce** frame: a sensor message with a 16-byte header,
    flags `0x00` (no "no response required" bit, so it probably expects an
    answer, e.g. from a Tydom in pairing mode) and state byte `0x80`:
    ```
    16 ff 00 00 10 02 a0 ef 00 80 82 01 01 40 00 00 1e 3f fc ce 24 e5
    ```
    It is logged as `announce` and never changes the entities.
  - a door sent its state about once per second, with a new message number
    each time, for at least 10 s (instead of the usual 3 + 3 repeats).

  Raw captures: [`tests/unsupported_frames.txt`](tests/unsupported_frames.txt).
- **Door DVI calibration.** Doors use a different DVI (no reed switch, no
  magnet). Per the K•LINE POD installation manual (page 9), a door DVI must be
  recalibrated as soon as it has been removed from the frame (*dormant*): put
  it back in the frame, close the door, then do **3 lock / unlock cycles with
  the key**. Observed before calibration: the door only ever reports
  `05 41 82 00 50 00 04` (unlocked), about once per second, whatever is in
  front of the sensor. Byte 8 bit `0x01` / byte 14 `0x04` may flag the missing
  calibration (Tydom exposes a `calibrationDefect` field for doors only); not
  confirmed yet.

## Tests

Host-side unit tests for the parser (CRC, header checksum, state and tilt
decoding, de-duplication, replay of real captures), with no hardware needed:

```sh
make -C esphome/tests
```
