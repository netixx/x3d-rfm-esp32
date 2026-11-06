# DVI captures

Frames received from K•LINE DVI sensors, de-whitened, as printed by the
receiver (byte 0 = length). Each event is sent several times (3 frames ~20 ms
apart, then the same burst ~3.5-5.3 s later) with the same message number:
only one line per event is kept. The unit tests in
[`test_x3d_frame.cpp`](test_x3d_frame.cpp) use these frames.

Frame layout (13-byte header): `len ff msg type hdr | id id id | b8 b9 82 b11
kind 00 b14 | header checksum | CRC`. Byte 8: handle position when unlocked
(`00` locked, `08` tilt, `0c` turn, `04` sliding). Byte 9: state (`01`
locked, `41` unlocked, `00` not changed; bits `10` and `80` seen, meaning
unknown). Byte 12: kind (`20` window, `40` sliding, `50` door). See the
decoding notes in the [README](../README.md#decoding-notes).

## Devices

| ID | Opening | Kind |
|---|---|---|
| `0xEFCC85` | tilt-only (soufflet) window | `20` |
| `0xEF56A1` | tilt-and-turn window | `20` |
| `0xEFCC75` | window | `20` |
| `0xEFA00B` | sliding door | `40` |
| `0xEFACF4` | sliding door | `40` |
| `0xEFA055` | sliding door (probably) | `40` |
| `0xEFA002` | sliding (power-on frame only) | `40` |
| `0xEFA214` | door | `50` |

## First captures (x3d-raw-monitor, unannotated)

Windows `0xEFCC75` and `0xEFA055`. Several "unlocked" events in a row on
`0xEFCC75`: the DVI reports the handle, not the sash.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `f2` | `08 41` | `13 ff f2 00 2d 75 cc ef 08 41 82 00 20 00 00 fc e5 dc 9a` | unlocked (tilt) |
| `f3` | `08 41` | `13 ff f3 00 2d 75 cc ef 08 41 82 00 20 00 00 fc e5 72 66` | unlocked (tilt) |
| `37` | `00 00` | `13 ff 37 00 2d 55 a0 ef 00 00 82 00 40 00 00 fd 5a 00 2c` | not changed |
| `f4` | `08 41` | `13 ff f4 00 2d 75 cc ef 08 41 82 00 20 00 00 fc e5 0a f1` | unlocked (tilt) |
| `f5` | `00 01` | `13 ff f5 00 2d 75 cc ef 00 01 82 00 20 00 00 fd 2d 10 57` | locked |
| `38` | `04 41` | `13 ff 38 00 2d 55 a0 ef 04 41 82 00 40 00 00 fd 15 cb 39` | unlocked |
| `39` | `00 01` | `13 ff 39 00 2d 55 a0 ef 00 01 82 00 40 00 00 fd 59 86 b2` | locked |

## Soufflet window and sliding door (x3d-raw-monitor)

Annotated: the soufflet `0xEFCC85` and the sliding door `0xEFA00B`, handles
operated by hand. Opening the sash sends nothing.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `53` | `08 41` | `13 ff 53 00 2d 85 cc ef 08 41 82 00 20 00 00 fc d5 a3 22` | soufflet, unlocked |
| `54` | `00 01` | `13 ff 54 00 2d 85 cc ef 00 01 82 00 20 00 00 fd 1d 6f ef` | soufflet, closed and locked |
| `55` | `08 41` | `13 ff 55 00 2d 85 cc ef 08 41 82 00 20 00 00 fc d5 75 49` | soufflet, closed and unlocked |
| `06` | `00 01` | `13 ff 06 00 2d 0b a0 ef 00 01 82 00 40 00 00 fd a3 be 2d` | sliding door, closed and locked |
| `07` | `04 41` | `13 ff 07 00 2d 0b a0 ef 04 41 82 00 40 00 00 fd 5f 54 7d` | sliding door, closed and unlocked |
| `08` | `00 01` | `13 ff 08 00 2d 0b a0 ef 00 01 82 00 40 00 00 fd a3 4f 03` | sliding door, closed and locked |

## SPI misread at 8 MHz

Every frame failed the CRC with the SPI bus at 8 MHz: in the FIFO burst read,
the MSB of each byte copied the previous byte's MSB. With the MSBs fixed, the
frame is valid (sliding door `0xEFACF4`, unlocked). Fixed by reading at 1 MHz.

| | frame |
|---|---|
| read | `13 ff 9e 00 2d 74 ac ef 84 41 02 80 40 00 00 7c ea 08 32 99` |
| real | `13 ff 1e 00 2d f4 ac ef 04 41 82 00 40 00 00 fc 6a 08 b2` |

## Door before calibration

Door `0xEFA214`, DVI taken out of the frame. It sent the same unlocked frame
about once per second, with a new message number each time, whatever was in
front of the sensor. A door DVI must be recalibrated (3 lock / unlock cycles
with the key) once removed from the frame. Byte 8 bit `01` and byte 14 `04`
are not understood.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `1a` | `05 41` | `13 ff 1a 00 2d 14 a2 ef 05 41 82 00 50 00 04 fd 3f d3 e3` | first of 11 identical events, msg `1a` to `24`, ~1 s apart |

## Power-on frame after a magnet wake-up

Sliding DVI `0xEFA002`, woken by passing a strong magnet in front of it a few
times. 16-byte header, message number `00`, flags `00`; the last two header
bytes are the encrypted message id (decrypts to 2). State `80`: no handle
position.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `00` | `00 80` | `16 ff 00 00 10 02 a0 ef 00 80 82 01 01 40 00 00 1e 3f fc ce 24 e5` | power-on, no position |

## Tilt-and-turn window on a bench supply

Window `0xEF56A1` out of the frame, powered from a bench supply instead of its
2x CR2032 in series (6 V). The magnet was moved by hand, so the "positions"
are not real handle positions. Bit `10` of byte 9 appeared after running at
low voltage and only cleared after a power cycle; its meaning is unknown.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `06` | `08 51` | `13 ff 06 00 2d a1 56 ef 08 51 82 00 20 00 00 fd 1f 39 b2` | after low-voltage tests |
| `07` | `00 11` | `13 ff 07 00 2d a1 56 ef 00 11 82 00 20 00 00 fd 67 b7 fe` | after low-voltage tests |
| `00` | `00 11` | `16 ff 00 00 10 a1 56 ef 00 11 82 00 00 20 00 00 c6 79 fc 28 26 49` | 3.2 V, power-on (reset), lowest reliable voltage |
| `1a` | `0c 51` | `13 ff 1a 00 2d a1 56 ef 0c 51 82 00 20 00 00 fd 1b 17 a4` | 4.0 V |
| `2a` | `08 d0` | `13 ff 2a 00 2d a1 56 ef 08 d0 82 01 20 00 00 fc 9f 45 1a` | conditions unknown; byte 9 bit `80` and byte 11 `01` |
| `37` | `00 11` | `13 ff 37 00 2d a1 56 ef 00 11 82 00 20 00 00 fd 67 64 60` | 4.0 V |
| `38` | `08 51` | `13 ff 38 00 2d a1 56 ef 08 51 82 00 20 00 00 fd 1f 1b 02` | 4.0 V |
| `39` | `00 11` | `13 ff 39 00 2d a1 56 ef 00 11 82 00 20 00 00 fd 67 95 4e` | back to 6 V without power cycle: `10` still set |
| `3a` | `08 51` | `13 ff 3a 00 2d a1 56 ef 08 51 82 00 20 00 00 fd 1f 56 db` | 6 V, no power cycle |

After a power cycle, at 6 V, then at 4 V, magnet held on each reed switch
(messages `03` and `07` not received):

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `00` | `00 01` | `16 ff 00 00 10 a1 56 ef 00 01 82 00 00 20 00 00 c6 79 fc 38 61 45` | 6 V, power-on: `10` cleared |
| `01` | `08 41` | `13 ff 01 00 2d a1 56 ef 08 41 82 00 20 00 00 fd 2f 4a c2` | 6 V, top reed |
| `02` | `0c 41` | `13 ff 02 00 2d a1 56 ef 0c 41 82 00 20 00 00 fd 2b 75 8c` | 6 V, middle reed |
| `04` | `08 41` | `13 ff 04 00 2d a1 56 ef 08 41 82 00 20 00 00 fd 2f 7f 8c` | 6 V, bottom reed |
| `05` | `08 41` | `13 ff 05 00 2d a1 56 ef 08 41 82 00 20 00 00 fd 2f d1 70` | 4 V, top reed |
| `06` | `08 41` | `13 ff 06 00 2d a1 56 ef 08 41 82 00 20 00 00 fd 2f 32 55` | 4 V, middle reed |
| `08` | `08 51` | `13 ff 08 00 2d a1 56 ef 08 51 82 00 20 00 00 fd 1f c8 9c` | 4 V, bottom reed: `10` set again |

## Tilt-and-turn window in its frame

Window `0xEF56A1` back in the window, on its batteries, handle operated by
hand. In the window, the handle's magnet stops at the bottom reed when locked;
in tilt and turn positions it passes the middle / top reed, then moves away
when the sash opens. Moving the sash sends nothing. Message `02` not received.

| msg | bytes 8-9 | frame | notes |
|---|---|---|---|
| `00` | `0c 41` | `16 ff 00 00 10 a1 56 ef 0c 41 82 00 00 20 00 00 c6 79 fb ec 21 82` | batteries inserted, window open in turn position (power-on) |
| `00` | `0c 41` | `13 ff 00 00 2d a1 56 ef 0c 41 82 00 20 00 00 fd 2b 38 55` | same message `00` resent as a normal frame; sash closed, handle still horizontal |
| `01` | `08 41` | `13 ff 01 00 2d a1 56 ef 08 41 82 00 20 00 00 fd 2f 4a c2` | handle up (tilt), sash still closed; tilting the sash then sent nothing |
| `03` | `00 01` | `13 ff 03 00 2d a1 56 ef 00 01 82 00 20 00 00 fd 77 03 c9` | sash closed, handle down (locked) |
