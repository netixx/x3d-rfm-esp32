// Host-side unit tests for the X3D frame parser. Run: make -C esphome/tests
#include <cstdio>
#include <cstring>

#include "../components/x3d_rfm69/x3d_frame.h"

using namespace esphome::x3d_rfm69;

static int failures = 0;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++; \
    } \
  } while (0)

// msg 0x42: window opened (handle unlocked)
static const uint8_t FRAME_OPEN[] = {0x13, 0xff, 0x42, 0x00, 0x2d, 0x85, 0xcc, 0xef, 0x08, 0x41,
                                     0x82, 0x00, 0x20, 0x00, 0x00, 0xfc, 0xd5, 0x43, 0x54};
// msg 0x43: window closed and locked
static const uint8_t FRAME_LOCKED[] = {0x13, 0xff, 0x43, 0x00, 0x2d, 0x85, 0xcc, 0xef, 0x00, 0x01,
                                       0x82, 0x00, 0x20, 0x00, 0x00, 0xfd, 0x1d, 0x59, 0xf2};

static void test_crc() {
  CHECK(crc16_xmodem(FRAME_OPEN, 17) == 0x4354);
  CHECK(crc16_xmodem(FRAME_LOCKED, 17) == 0x59f2);
}

static void test_parse_open() {
  SensorFrame f{};
  CHECK(parse_sensor_frame(FRAME_OPEN, sizeof(FRAME_OPEN), &f) == ParseResult::OK);
  CHECK(f.device_id == 0xEFCC85);
  CHECK(f.msg_no == 0x42);
  CHECK(f.lock == 0x08);
  CHECK(f.sash == 0x41);
  CHECK(f.extra_len == 5);
  CHECK(f.state == WindowState::OPEN);
  CHECK(!has_unexpected_bits(f));
}

static void test_parse_locked() {
  SensorFrame f{};
  CHECK(parse_sensor_frame(FRAME_LOCKED, sizeof(FRAME_LOCKED), &f) == ParseResult::OK);
  CHECK(f.device_id == 0xEFCC85);
  CHECK(f.msg_no == 0x43);
  CHECK(f.state == WindowState::CLOSED_LOCKED);
  CHECK(!has_unexpected_bits(f));
}

static void test_trailing_garbage() {
  uint8_t buf[X3D_MAX_FRAME_LEN];
  std::memset(buf, 0xA5, sizeof(buf));
  std::memcpy(buf, FRAME_OPEN, sizeof(FRAME_OPEN));
  SensorFrame f{};
  CHECK(parse_sensor_frame(buf, sizeof(buf), &f) == ParseResult::OK);
  CHECK(f.state == WindowState::OPEN);
}

static void test_corruption() {
  uint8_t buf[sizeof(FRAME_OPEN)];
  SensorFrame f{};

  std::memcpy(buf, FRAME_OPEN, sizeof(buf));
  buf[9] ^= 0x40;  // flip sash bit
  CHECK(parse_sensor_frame(buf, sizeof(buf), &f) == ParseResult::BAD_CRC);

  CHECK(parse_sensor_frame(FRAME_OPEN, 10, &f) == ParseResult::BAD_LENGTH);
  CHECK(parse_sensor_frame(FRAME_OPEN, 3, &f) == ParseResult::TOO_SHORT);

  // Non sensor message with a valid CRC is ignored.
  std::memcpy(buf, FRAME_OPEN, sizeof(buf));
  buf[3] = 0x01;
  uint16_t crc = crc16_xmodem(buf, 17);
  buf[17] = crc >> 8;
  buf[18] = crc & 0xff;
  CHECK(parse_sensor_frame(buf, sizeof(buf), &f) == ParseResult::NOT_SENSOR);

  // Valid CRC but broken header checksum.
  std::memcpy(buf, FRAME_OPEN, sizeof(buf));
  buf[16] ^= 0x01;
  crc = crc16_xmodem(buf, 17);
  buf[17] = crc >> 8;
  buf[18] = crc & 0xff;
  CHECK(parse_sensor_frame(buf, sizeof(buf), &f) == ParseResult::BAD_HEADER_CHECKSUM);
}

static void test_decode_state() {
  CHECK(decode_window_state(0x08, 0x41) == WindowState::OPEN);
  CHECK(decode_window_state(0x00, 0x41) == WindowState::OPEN);
  CHECK(decode_window_state(0x08, 0x01) == WindowState::CLOSED_UNLOCKED);
  CHECK(decode_window_state(0x00, 0x01) == WindowState::CLOSED_LOCKED);
  CHECK(decode_window_state(0x00, 0x00) == WindowState::UNKNOWN);
  CHECK(std::strcmp(window_state_to_string(WindowState::CLOSED_UNLOCKED), "closed_unlocked") == 0);
}

static void test_dedup() {
  Deduplicator d(5000);
  // 3 repeats ~20 ms apart -> only the first is new.
  CHECK(d.is_new(0xEFCC85, 0x42, 24075));
  CHECK(!d.is_new(0xEFCC85, 0x42, 24095));
  CHECK(!d.is_new(0xEFCC85, 0x42, 24115));
  // Other device with the same msg# is independent.
  CHECK(d.is_new(0x123456, 0x42, 24120));
  // Next event.
  CHECK(d.is_new(0xEFCC85, 0x43, 30000));
  // Counter going backwards (battery change) still counts as new.
  CHECK(d.is_new(0xEFCC85, 0x01, 31000));
  // Same msg# long after the last one (counter wrapped / reset) is new again.
  CHECK(d.is_new(0xEFCC85, 0x01, 100000));
  // Eviction keeps working with more devices than slots.
  for (uint32_t i = 0; i < Deduplicator::MAX_DEVICES * 2; i++)
    CHECK(d.is_new(0x100000 + i, 0x10, 200000 + i));
  CHECK(!d.is_new(0x100000 + Deduplicator::MAX_DEVICES * 2 - 1, 0x10, 200100));
}

int main() {
  test_crc();
  test_parse_open();
  test_parse_locked();
  test_trailing_garbage();
  test_corruption();
  test_decode_state();
  test_dedup();
  if (failures == 0)
    std::printf("All tests passed\n");
  return failures == 0 ? 0 : 1;
}
