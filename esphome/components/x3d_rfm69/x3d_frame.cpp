#include "x3d_frame.h"

namespace esphome::x3d_rfm69 {

// Frame layout (idx 0 = length byte), verified on K-LINE DVI captures:
//   0 len | 1 0xff | 2 msg# | 3 type | 4 flags|hdr_len | 5..7 device id (LE)
//   8 lock | 9 sash | 10.. extra | header checksum (BE) | CRC16 (BE)
static constexpr size_t IDX_LEN = 0;
static constexpr size_t IDX_MSG_NO = 2;
static constexpr size_t IDX_TYPE = 3;
static constexpr size_t IDX_HEADER = 4;
static constexpr size_t IDX_DEVICE_ID = 5;
static constexpr size_t IDX_LOCK = 8;
static constexpr size_t IDX_SASH = 9;
static constexpr size_t IDX_EXTRA = 10;
static constexpr uint8_t HEADER_LEN_MASK = 0x1f;
// device id (3) + lock + sash + checksum (2)
static constexpr uint8_t MIN_HEADER_LEN = 1 + 3 + 1 + 1 + 2;

static const uint8_t KNOWN_EXTRA[] = {0x82, 0x00, 0x20, 0x00, 0x00};

uint16_t crc16_xmodem(const uint8_t *data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

ParseResult parse_sensor_frame(const uint8_t *buf, size_t buf_len, SensorFrame *out) {
  if (buf_len < IDX_HEADER + 1)
    return ParseResult::TOO_SHORT;

  const size_t len = buf[IDX_LEN];
  if (len < IDX_HEADER + 1 + 2 || len > buf_len || len > X3D_MAX_FRAME_LEN)
    return ParseResult::BAD_LENGTH;

  const uint16_t rx_crc = static_cast<uint16_t>(buf[len - 2] << 8 | buf[len - 1]);
  if (crc16_xmodem(buf, len - 2) != rx_crc)
    return ParseResult::BAD_CRC;

  if (buf[IDX_TYPE] != X3D_MSG_TYPE_SENSOR)
    return ParseResult::NOT_SENSOR;

  // Header runs from idx 4 for hdr_len bytes; its last 2 bytes are the checksum
  // and the CRC must follow right after it.
  const size_t hdr_len = buf[IDX_HEADER] & HEADER_LEN_MASK;
  if (hdr_len < MIN_HEADER_LEN || IDX_HEADER + hdr_len + 2 != len)
    return ParseResult::BAD_HEADER;

  const size_t chk_idx = IDX_HEADER + hdr_len - 2;
  // Sum excludes the flags/header-length byte (verified on captures).
  uint32_t sum = 0;
  for (size_t i = IDX_DEVICE_ID; i < chk_idx; i++)
    sum += buf[i];
  const uint16_t expected_chk = static_cast<uint16_t>((0x10000 - (sum & 0xffff)) & 0xffff);
  const uint16_t rx_chk = static_cast<uint16_t>(buf[chk_idx] << 8 | buf[chk_idx + 1]);
  if (expected_chk != rx_chk)
    return ParseResult::BAD_HEADER_CHECKSUM;

  out->length = static_cast<uint8_t>(len);
  out->msg_no = buf[IDX_MSG_NO];
  out->device_id = static_cast<uint32_t>(buf[IDX_DEVICE_ID]) | static_cast<uint32_t>(buf[IDX_DEVICE_ID + 1]) << 8 |
                   static_cast<uint32_t>(buf[IDX_DEVICE_ID + 2]) << 16;
  out->lock = buf[IDX_LOCK];
  out->sash = buf[IDX_SASH];
  out->extra_len = 0;
  for (size_t i = IDX_EXTRA; i < chk_idx && out->extra_len < sizeof(out->extra); i++)
    out->extra[out->extra_len++] = buf[i];
  out->state = decode_window_state(out->lock, out->sash);
  return ParseResult::OK;
}

WindowState decode_window_state(uint8_t lock, uint8_t sash) {
  if (sash & DVI_SASH_OPEN_BIT)
    return WindowState::OPEN;
  if (!(sash & DVI_SASH_VALID_BIT))
    return WindowState::UNKNOWN;  // e.g. "no change" message of plain detectors
  if (lock & DVI_LOCK_UNLOCKED_BIT)
    return WindowState::CLOSED_UNLOCKED;
  return WindowState::CLOSED_LOCKED;
}

const char *window_state_to_string(WindowState state) {
  switch (state) {
    case WindowState::OPEN:
      return "open";
    case WindowState::CLOSED_UNLOCKED:
      return "closed_unlocked";
    case WindowState::CLOSED_LOCKED:
      return "closed_locked";
    default:
      return "unknown";
  }
}

const char *parse_result_to_string(ParseResult result) {
  switch (result) {
    case ParseResult::OK:
      return "ok";
    case ParseResult::TOO_SHORT:
      return "too short";
    case ParseResult::BAD_LENGTH:
      return "bad length";
    case ParseResult::BAD_CRC:
      return "bad CRC";
    case ParseResult::NOT_SENSOR:
      return "not a sensor message";
    case ParseResult::BAD_HEADER:
      return "bad header length";
    case ParseResult::BAD_HEADER_CHECKSUM:
      return "bad header checksum";
    default:
      return "?";
  }
}

bool has_unexpected_bits(const SensorFrame &frame) {
  if (frame.lock != 0x00 && frame.lock != DVI_LOCK_UNLOCKED_BIT)
    return true;
  if (frame.sash != DVI_SASH_VALID_BIT && frame.sash != (DVI_SASH_VALID_BIT | DVI_SASH_OPEN_BIT))
    return true;
  if (frame.extra_len != sizeof(KNOWN_EXTRA))
    return true;
  for (size_t i = 0; i < sizeof(KNOWN_EXTRA); i++) {
    if (frame.extra[i] != KNOWN_EXTRA[i])
      return true;
  }
  return false;
}

bool Deduplicator::is_new(uint32_t device_id, uint8_t msg_no, uint32_t now_ms) {
  Entry *slot = nullptr;
  for (auto &e : this->entries_) {
    if (e.used && e.device_id == device_id) {
      slot = &e;
      break;
    }
  }
  if (slot != nullptr) {
    const bool duplicate = slot->msg_no == msg_no && (now_ms - slot->last_ms) < this->window_ms_;
    slot->msg_no = msg_no;
    slot->last_ms = now_ms;
    return !duplicate;
  }
  // New device: take a free slot, or evict the least recently seen one.
  slot = &this->entries_[0];
  for (auto &e : this->entries_) {
    if (!e.used) {
      slot = &e;
      break;
    }
    if (now_ms - e.last_ms > now_ms - slot->last_ms)
      slot = &e;
  }
  *slot = Entry{device_id, now_ms, msg_no, true};
  return true;
}

}  // namespace esphome::x3d_rfm69
