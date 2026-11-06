#pragma once

// Pure X3D frame parsing helpers (no ESPHome / hardware dependency) so they can
// be unit tested on the host, see esphome/tests/test_x3d_frame.cpp.

#include <cstddef>
#include <cstdint>

namespace esphome::x3d_rfm69 {

static constexpr uint8_t X3D_MSG_TYPE_SENSOR = 0x00;
static constexpr size_t X3D_MAX_FRAME_LEN = 65;  // length byte + 64 byte payload

// State byte (idx 9) bits seen on K-LINE DVI captures. The DVI reads the
// handle: it reports locked / unlocked, not open / closed.
static constexpr uint8_t DVI_STATE_UNLOCKED_BIT = 0x40;
static constexpr uint8_t DVI_STATE_VALID_BIT = 0x01;
// State byte bit 0x10: meaning unknown (0x11 / 0x51). Seen on a window fed
// from a bench supply after low-voltage tests, until a power cycle; not
// enough to call it battery low.
static constexpr uint8_t DVI_STATE_UNKNOWN_0X10 = 0x10;
// State byte bit 0x80: meaning unknown. Alone (0x80) it carries no handle
// position (seen in a power-on frame); it was also seen with a position
// (0xD0 = 0x80 | unlocked | 0x10, with byte 11 = 0x01).
static constexpr uint8_t DVI_STATE_ANNOUNCE_BIT = 0x80;
// Byte 8 = handle position when unlocked, from a tilt-and-turn window:
// 0x08 tilt (as on tilt-only "soufflet" windows), 0x0c turn (handle
// horizontal). 0x04 alone on sliding doors, 0x05 on a door.
static constexpr uint8_t DVI_POSITION_MASK = 0x0c;
static constexpr uint8_t DVI_POSITION_TILT = 0x08;
static constexpr uint8_t DVI_POSITION_TURN = 0x0c;

enum class HandleState : uint8_t {
  UNCHANGED = 0,  // "state not changed" message (state byte 0x00), maybe a heartbeat
  LOCKED,
  UNLOCKED,
  ANNOUNCE,  // power-on frame without handle position (state byte 0x80)
};

/// LOCKED or UNLOCKED: the frame says where the handle is.
inline bool has_handle_position(HandleState state) {
  return state == HandleState::LOCKED || state == HandleState::UNLOCKED;
}

enum class ParseResult : uint8_t {
  OK = 0,
  TOO_SHORT,
  BAD_LENGTH,
  BAD_CRC,
  NOT_SENSOR,
  BAD_HEADER,
  BAD_HEADER_CHECKSUM,
};

/// Fields common to every X3D frame (any message type).
struct X3DFrame {
  uint8_t length;      // idx 0, total frame length including length byte and CRC
  uint8_t msg_no;      // idx 2, rolling message number
  uint8_t msg_type;    // idx 3, 0 sensor, 1 standard, 2 pairing, 3 beacon
  uint8_t header_len;  // idx 4, low 5 bits
  uint32_t device_id;  // idx 5..7, little endian
};

struct SensorFrame {
  uint8_t length;      // idx 0, total frame length including length byte and CRC
  uint8_t msg_no;      // idx 2, rolling message number
  uint32_t device_id;  // idx 5..7, little endian
  uint8_t flags;       // idx 8, 0x00 locked, 0x08 tilt / 0x04 unlocked
  uint8_t state_byte;  // idx 9, 0x41 unlocked / 0x01 locked / 0x00 not changed
  uint8_t extra[8];    // idx 10 up to header checksum (82 00 20 00 00 or 82 00 40 00 00)
  uint8_t extra_len;
  HandleState state;
};

/// CRC-16/XMODEM: poly 0x1021, init 0x0000, not reflected, no final xor.
/// Verified against the DVI captures (over length byte + payload, stored big endian).
uint16_t crc16_xmodem(const uint8_t *data, size_t len);

/// Check length and CRC of any X3D frame and read its common fields.
/// buf[0] is the length byte as read from the SX1231 FIFO; buf_len is the number
/// of bytes available in buf; trailing garbage is ignored.
ParseResult parse_frame(const uint8_t *buf, size_t buf_len, X3DFrame *out);

/// Parse a sensor (DVI) message: parse_frame() plus header checks and handle state.
ParseResult parse_sensor_frame(const uint8_t *buf, size_t buf_len, SensorFrame *out);

const char *msg_type_to_string(uint8_t msg_type);

HandleState decode_handle_state(uint8_t state_byte);

const char *handle_state_to_string(HandleState state);

/// Handle in tilt position (byte 8 = 0x08).
bool is_tilted(const SensorFrame &frame);

/// Handle in turn position (byte 8 = 0x0c, tilt-and-turn windows).
bool is_turned(const SensorFrame &frame);

/// Power-on frame: 16-byte header (13 normally), message number 0x00, sent
/// when the DVI starts (batteries inserted, magnet wake-up, brown-out). Its
/// state byte is valid (or 0x80) and the sensor kind is one byte later. The
/// header ends with the encrypted message id (X3D message id encryption, see
/// github.com/psolyca/X2D-X3D), which decrypts to 2 on the two devices seen.
bool is_power_on_frame(const SensorFrame &frame);

/// Sensor kind from byte 12: 0x20 on a tilt (soufflet) window, 0x40 on a
/// sliding door, 0x50 on a door. Same split as Tydom's windowFrench /
/// windowSliding / door device types.
const char *sensor_kind_to_string(const SensorFrame &frame);
const char *parse_result_to_string(ParseResult result);

/// True when bytes 8-14 differ from every value captured so far
/// (candidates: battery low, tamper, ...).
bool has_unexpected_bits(const SensorFrame &frame);

/// Combined handle state of several sashes (one window or door): UNLOCKED as
/// soon as one sash is, LOCKED when all are, UNCHANGED (unknown) otherwise.
class HandleCombiner {
 public:
  static constexpr size_t MAX_SASHES = 4;

  void add(uint32_t x3d_id) {
    if (this->count_ < MAX_SASHES)
      this->ids_[this->count_++] = x3d_id;
  }
  /// Record one sash (UNCHANGED = unknown, e.g. after a timeout); returns the combined state.
  HandleState update(uint32_t x3d_id, HandleState state);

 protected:
  uint32_t ids_[MAX_SASHES]{};
  HandleState states_[MAX_SASHES]{};
  size_t count_{0};
};

/// Drops the X3D repeats: each event is sent 3 times ~20 ms apart, and the burst
/// is usually sent again ~3.5-4 s later, all with the same message number.
/// A (device, msg#) pair is a duplicate only if seen within `window_ms`, so
/// rolling / reset counters don't swallow real events.
class Deduplicator {
 public:
  static constexpr size_t MAX_DEVICES = 32;

  explicit Deduplicator(uint32_t window_ms = 30000) : window_ms_(window_ms) {}

  /// Returns true if this frame is a new event and records it.
  bool is_new(uint32_t device_id, uint8_t msg_no, uint32_t now_ms);

 protected:
  struct Entry {
    uint32_t device_id;
    uint32_t last_ms;
    uint8_t msg_no;
    bool used;
  };
  Entry entries_[MAX_DEVICES]{};
  uint32_t window_ms_;
};

}  // namespace esphome::x3d_rfm69
