# X3D Message Log

To focus more on relevant data some random or known values are replaced by some chars.

The following log omits the crc sum.

The device id and the message number are replaced by `*`.

The random message id in header is replaced by `#`.

The Header-Cross-Sum is replaced by `q`.

## Type 0 messages send from Tydom 1.0

At startup with its administrative device id:
```
1F FF 01 00 34 ****** 80 01 8A 0000FFFFFF000000002ACD qqqq B201FFFFFF
1F FF 01 00 34 ****** 80 01 8A 0000FFFFFF000000002ACD qqqq B101FFFFFF
1F FF 01 00 34 ****** 80 01 8A 0000FFFFFF000000002ACD qqqq B001FFFFFF

1C FF 02 00 31 ****** 00 00 82 0000000000000000 qqqq B201FFFFFF
1C FF 02 00 31 ****** 00 00 82 0000000000000000 qqqq B101FFFFFF
1C FF 02 00 31 ****** 00 00 82 0000000000000000 qqqq B001FFFFFF

24 FF 03 00 39 ****** 40 02 82 00030812040032000000000000010421 qqqq B201FFFFFF
24 FF 03 00 39 ****** 40 02 82 00030812040032000000000000010421 qqqq B101FFFFFF
24 FF 03 00 39 ****** 40 02 82 00030812040032000000000000010421 qqqq B001FFFFFF
```

Cyclic with its administrative device id:
```
1C FF ** 00 31 ****** 00 00 82 0000000000000000 qqqq B201FFFFFF
1C FF ** 00 31 ****** 00 00 82 0000000000000000 qqqq B101FFFFFF
1C FF ** 00 31 ****** 00 00 82 0000000000000000 qqqq B001FFFFFF
```

## Type 0 messages send from Window Sensor

```
13 FF ** 00 2D ****** 00 41 8201000000 qqqq   Contact opened
13 FF ** 00 2D ****** 00 01 8201000000 qqqq   Contact closed
13 FF ** 00 2D ****** 00 00 8201000000 qqqq   Contact not changed
```

## Type 0 messages send from K•LINE DVI (lock detector)

Byte 8 flags, byte 9 state, byte 12 kind (`20` window, `40` sliding, `50` door):
```
13 FF ** 00 2D ****** 00 01 8200200000 cccc qqqq   Window, locked
13 FF ** 00 2D ****** 08 41 8200200000 cccc qqqq   Window, unlocked (tilt)
13 FF ** 00 2D ****** 00 01 8200400000 cccc qqqq   Sliding door, locked
13 FF ** 00 2D ****** 04 41 8200400000 cccc qqqq   Sliding door, unlocked
13 FF ** 00 2D ****** 05 41 8200500004 cccc qqqq   Door, unlocked (sent ~1/s after magnet activation)
13 FF ** 00 2D ****** 00 00 8200400000 cccc qqqq   Not changed
```

Announce frame, sent when a DVI is woken up by passing a magnet in front of it
(16 byte header, flags `0x00`):
```
16 FF 00 00 10 ****** 00 80 8201014000001E3F cccc qqqq
```

`cccc` header checksum, `qqqq` CRC. Decoder: [esphome/](esphome/README.md).

## Type 1 messages send from Tydom 1.0

Unpair device No. 3 (0b1000)
```
1E FF ** 01 0C ****** 84 059800 #### qqqq 04 0700 0000 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 03 0700 0000 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 02 0700 0000 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 01 0700 0000 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 00 0700 0000 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 10 0700 0100 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 12 0700 0400 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 22 0700 0400 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 20 0700 0500 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 30 0700 0500 0400 00 E000 0000
1E FF ** 01 0C ****** 84 059800 #### qqqq 32 0700 0400 0400 00 E000 0000
```

## Type 1 messages send from Thermostat

Cyclic message from Thermostat containing the current room temperature
```
req: 21 FF ** 01 0F ****** 00 059808 00 D708 #### qqqq 01 0100 0000 0100 08 0000 0000
res: 21 FF ** 01 0F ****** 00 059808 00 D708 #### qqqq 10 0100 0100 0100 08 0000 0100
D708 = 0x08D7 = 2263 / 100 = 22.63 °C
```

## Type 2 messages send from Tydom 1.0

Register Network 4 with no devices
```
req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0000 0000 1FFF 0000 0000 E0 000001FFFFFF1CDE
```

Register Network 4 with one new device. The new device puts a random value in first response marked with `r`. This value is used in the second request.
```
req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0000 0000 1FFF 0000 0000 E0 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 40 0000 0100 1FFF 0000 rrrr E0 000001FFFFFF1CDE

req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0000 0000 1FFF 0000 rrrr E5 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 40 0000 0100 1FFF 0000 rrrr E5 000001FFFFFF1CDE

req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 40 0100 0100 1FFF 0100 0000 E0 000001FFFFFF1CDE
```

Register Network 4, with one device already assigned
```
req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 20 0100 0100 1FFF 0100 0000 E0 000001FFFFFF1CDE

req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 20 0100 0100 1FFF 0100 0000 E0 000001FFFFFF1CDE

req: 26 FF ** 02 0C ****** 84 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0 000001FFFFFF1CDE
res: 26 FF ** 02 0C ****** 84 859800 #### qqqq 20 0100 0100 1FFF 0100 0000 E0 000001FFFFFF1CDE
```

Register Network 5 with no devices
```
req: 26 FF ** 02 0C ****** 85 859800 #### qqqq 04 0000 0000 1FFF 0000 0000 E0 000001FFFFFF1CDE
```

## Type 2 messages send from Thermostat

Register one new device. The new device puts a random value in first response marked with `r`. This value is used in the second request.
```
req: 1F FF ** 02 0C ****** 00 859800 #### qqqq 04 0000 0000 1FFF 0000 0000 E0
res: 1F FF ** 02 0C ****** 00 859800 #### qqqq 40 0000 0100 1FFF 0000 rrrr E0

req: 1F FF ** 02 0C ****** 00 859800 #### qqqq 04 0000 0000 1FFF 0000 rrrr E5
res: 1F FF ** 02 0C ****** 00 859800 #### qqqq 40 0000 0100 1FFF 0000 rrrr E5

req: 1F FF ** 02 0C ****** 00 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0
res: 1F FF ** 02 0C ****** 00 859800 #### qqqq 40 0100 0100 1FFF 0100 0000 E0

req: 1F FF ** 02 0C ****** 00 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0
res: 1F FF ** 02 0C ****** 00 859800 #### qqqq 40 0100 0100 1FFF 0100 0000 E0

req: 1F FF ** 02 0C ****** 00 859800 #### qqqq 04 0100 0000 1FFF 0100 0000 E0
res: 1F FF ** 02 0C ****** 00 859800 #### qqqq 40 0100 0100 1FFF 0100 0000 E0
```

## Type 3 messages send to Identify actor

The following message is send from Tydom in Network 4 to device slot 0
```
req: 1C FF ** 03 0C ****** 84 059800 #### qqqq 04 0100 0000 FF 00 04E0FF
res: 1C FF ** 03 0C ****** 84 059800 #### qqqq 30 0100 0100 FF 00 04E0FF
```

The following message is send from Tydom in Network 4 to device slot 1
```
req: 1C FF ** 03 0C ****** 84 059800 #### qqqq 04 0300 0000 FF 01 04E0FF
res: 1C FF ** 03 0C ****** 84 059800 #### qqqq 10 0300 0100 FF 01 04E0FF
     1C FF ** 03 0C ****** 84 059800 #### qqqq 11 0300 0300 FF 01 04E0FF
```

The following message is send from Thermostat to device slot 0
```
req: 1C FF ** 03 0C ****** 00 059800 #### qqqq 04 0100 0000 FF 00 00E0FF
res: 1C FF ** 03 0C ****** 00 059800 #### qqqq 30 0100 0100 FF 00 00E0FF
```
