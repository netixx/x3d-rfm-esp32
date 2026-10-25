#include "x3d_rfm69.h"

#include <cinttypes>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome::x3d_rfm69 {

static const char *const TAG = "x3d_rfm69";

// SX1231 registers used here.
static constexpr uint8_t REG_FIFO = 0x00;
static constexpr uint8_t REG_OP_MODE = 0x01;
static constexpr uint8_t REG_DATA_MODUL = 0x02;
static constexpr uint8_t REG_BITRATE_MSB = 0x03;
static constexpr uint8_t REG_BITRATE_LSB = 0x04;
static constexpr uint8_t REG_FDEV_MSB = 0x05;
static constexpr uint8_t REG_FDEV_LSB = 0x06;
static constexpr uint8_t REG_FRF_MSB = 0x07;
static constexpr uint8_t REG_FRF_MID = 0x08;
static constexpr uint8_t REG_FRF_LSB = 0x09;
static constexpr uint8_t REG_VERSION = 0x10;
static constexpr uint8_t REG_PA_LEVEL = 0x11;
static constexpr uint8_t REG_OCP = 0x13;
static constexpr uint8_t REG_RX_BW = 0x19;
static constexpr uint8_t REG_AFC_BW = 0x1A;
static constexpr uint8_t REG_AFC_FEI = 0x1E;
static constexpr uint8_t REG_RSSI_VALUE = 0x24;
static constexpr uint8_t REG_DIO_MAPPING_1 = 0x25;
static constexpr uint8_t REG_DIO_MAPPING_2 = 0x26;
static constexpr uint8_t REG_IRQ_FLAGS_1 = 0x27;
static constexpr uint8_t REG_IRQ_FLAGS_2 = 0x28;
static constexpr uint8_t REG_RSSI_THRESH = 0x29;
static constexpr uint8_t REG_PREAMBLE_MSB = 0x2C;
static constexpr uint8_t REG_PREAMBLE_LSB = 0x2D;
static constexpr uint8_t REG_SYNC_CONFIG = 0x2E;
static constexpr uint8_t REG_SYNC_VALUE_1 = 0x2F;
static constexpr uint8_t REG_PACKET_CONFIG_1 = 0x37;
static constexpr uint8_t REG_PAYLOAD_LENGTH = 0x38;
static constexpr uint8_t REG_FIFO_THRESH = 0x3C;
static constexpr uint8_t REG_PACKET_CONFIG_2 = 0x3D;
static constexpr uint8_t REG_TEST_LNA = 0x58;
static constexpr uint8_t REG_TEST_DAGC = 0x6F;

static constexpr uint8_t MODE_STANDBY = 0x04;
static constexpr uint8_t MODE_RECEIVER = 0x10;
static constexpr uint8_t IRQ1_MODE_READY = 0x80;
static constexpr uint8_t IRQ2_PAYLOAD_READY = 0x04;
static constexpr uint8_t SX1231_VERSION = 0x24;
static constexpr uint8_t MAX_PAYLOAD = 64;

static constexpr double FSTEP = 32000000.0 / 524288.0;  // FXOSC / 2^19

static std::string to_hex(const uint8_t *data, size_t len) {
  static const char HEX_CHARS[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 3);
  for (size_t i = 0; i < len; i++) {
    if (i > 0)
      out.push_back(' ');
    out.push_back(HEX_CHARS[data[i] >> 4]);
    out.push_back(HEX_CHARS[data[i] & 0x0f]);
  }
  return out;
}

uint8_t X3DRFM69::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg & 0x7F);
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void X3DRFM69::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg | 0x80);
  this->write_byte(value);
  this->disable();
}

void X3DRFM69::set_mode_(uint8_t mode) {
  if (this->mode_ == mode)
    return;
  this->mode_ = mode;
  this->write_register_(REG_OP_MODE, mode);
  // DIO0 = PayloadReady (mapping 01) in RX only, ClkOut off.
  this->write_register_(REG_DIO_MAPPING_1, mode == MODE_RECEIVER ? 0x40 : 0x00);
  this->write_register_(REG_DIO_MAPPING_2, 0x07);
}

void IRAM_ATTR X3DRFM69::gpio_intr(X3DRFM69 *arg) { arg->enable_loop_soon_any_context(); }

void X3DRFM69::setup() {
  if (this->dio0_pin_ != nullptr) {
    this->dio0_pin_->setup();
    this->dio0_pin_->attach_interrupt(&X3DRFM69::gpio_intr, this, gpio::INTERRUPT_RISING_EDGE);
  }
  this->spi_setup();

  uint8_t version = this->read_register_(REG_VERSION);
  if (version != SX1231_VERSION) {
    ESP_LOGE(TAG, "SX1231/RFM69 not found: RegVersion=0x%02X (expected 0x%02X). Check wiring and power.", version,
             SX1231_VERSION);
    this->mark_failed();
    return;
  }

  // SPI sanity check, as in sx1231_init() of x3d-raw-monitor.
  uint32_t start = millis();
  do {
    this->write_register_(REG_SYNC_VALUE_1, 0xAA);
  } while (this->read_register_(REG_SYNC_VALUE_1) != 0xAA && millis() - start < 50);
  start = millis();
  do {
    this->write_register_(REG_SYNC_VALUE_1, 0x55);
  } while (this->read_register_(REG_SYNC_VALUE_1) != 0x55 && millis() - start < 50);

  this->write_register_(REG_OCP, 0x0F);  // OCP off
  this->set_mode_(MODE_STANDBY);
  start = millis();
  while ((this->read_register_(REG_IRQ_FLAGS_1) & IRQ1_MODE_READY) == 0) {
    if (millis() - start > 50) {
      ESP_LOGE(TAG, "SX1231 did not reach standby mode");
      this->mark_failed();
      return;
    }
  }

  this->configure_radio_();
  this->receive_begin_();

  if (this->restore_state_) {
    for (auto *opening : this->openings_)
      opening->restore_state();
  }

  // Safety net: if a DIO0 edge was missed the pin stays high and no new edge comes.
  if (this->dio0_pin_ != nullptr) {
    this->set_interval(1000, [this]() {
      if (this->dio0_pin_->digital_read())
        this->enable_loop();
    });
  }
}

// Exact port of rfm_init() from x3d-raw-monitor (proven on real DVI captures).
void X3DRFM69::configure_radio_() {
  // Packet mode, FSK, no shaping
  this->write_register_(REG_DATA_MODUL, 0x00);
  // 40 kbps: 32 MHz / 40000 = 800
  this->write_register_(REG_BITRATE_MSB, 0x03);
  this->write_register_(REG_BITRATE_LSB, 0x20);
  // Fdev 80 kHz: 80000 / FSTEP = 1311
  this->write_register_(REG_FDEV_MSB, 0x05);
  this->write_register_(REG_FDEV_LSB, 0x1F);
  // Carrier frequency
  auto frf = static_cast<uint32_t>(std::lround(static_cast<double>(this->frequency_) / FSTEP));
  this->write_register_(REG_FRF_MSB, (frf >> 16) & 0xFF);
  this->write_register_(REG_FRF_MID, (frf >> 8) & 0xFF);
  this->write_register_(REG_FRF_LSB, frf & 0xFF);
  // RX BW 125 kHz, DCC 4 %
  this->write_register_(REG_RX_BW, 0x42);
  // AFC BW 41.7 kHz, DCC 1 %
  this->write_register_(REG_AFC_BW, 0x93);
  // The reference driver writes this register with a 16 bit transfer, so the
  // effective RegAfcFei value is 0x00 (the second byte lands in read-only RegAfcMsb).
  this->write_register_(REG_AFC_FEI, 0x00);
  // RSSI threshold -114 dBm
  this->write_register_(REG_RSSI_THRESH, 114 * 2);
  // Preamble 4 bytes
  this->write_register_(REG_PREAMBLE_MSB, 0x00);
  this->write_register_(REG_PREAMBLE_LSB, 0x04);
  // Sync on, 4 bytes, no tolerance: 81 69 96 7e
  this->write_register_(REG_SYNC_CONFIG, 0x98);
  static const uint8_t SYNC[] = {0x81, 0x69, 0x96, 0x7E};
  for (uint8_t i = 0; i < sizeof(SYNC); i++)
    this->write_register_(REG_SYNC_VALUE_1 + i, SYNC[i]);
  // Variable length, whitening, hardware CRC off, no address filtering
  this->write_register_(REG_PACKET_CONFIG_1, 0xC0);
  this->write_register_(REG_PAYLOAD_LENGTH, MAX_PAYLOAD);
  // Inter packet RX delay 32 bits, auto RX restart on
  this->write_register_(REG_PACKET_CONFIG_2, 0x52);
  // TX start on FIFO not empty, threshold 15
  this->write_register_(REG_FIFO_THRESH, 0x8F);
  // LNA high sensitivity
  this->write_register_(REG_TEST_LNA, 0x2D);
  // Continuous DAGC, improved margin, AfcLowBetaOn=0
  this->write_register_(REG_TEST_DAGC, 0x30);
  // PA1+PA2, power 23 (unused, receive only)
  this->write_register_(REG_PA_LEVEL, 0x77);
}

void X3DRFM69::receive_begin_() {
  this->set_mode_(MODE_STANDBY);
  this->set_mode_(MODE_RECEIVER);
}

bool X3DRFM69::payload_ready_() {
  if (this->dio0_pin_ != nullptr)
    return this->dio0_pin_->digital_read();
  return (this->read_register_(REG_IRQ_FLAGS_2) & IRQ2_PAYLOAD_READY) != 0;
}

void X3DRFM69::loop() {
  if (this->payload_ready_())
    this->read_and_handle_packet_();
  // With an IRQ pin, sleep until the next DIO0 edge. Without it, keep polling.
  if (this->dio0_pin_ != nullptr && !this->dio0_pin_->digital_read())
    this->disable_loop();
}

void X3DRFM69::read_and_handle_packet_() {
  if ((this->read_register_(REG_IRQ_FLAGS_2) & IRQ2_PAYLOAD_READY) == 0)
    return;

  // RSSI of the packet that just completed, read before leaving RX.
  float rssi = -static_cast<float>(this->read_register_(REG_RSSI_VALUE)) / 2.0f;

  uint8_t buf[X3D_MAX_FRAME_LEN];
  this->set_mode_(MODE_STANDBY);
  this->enable();
  this->write_byte(REG_FIFO);
  uint8_t len = this->read_byte();
  if (len > MAX_PAYLOAD)
    len = MAX_PAYLOAD;
  buf[0] = len;
  this->read_array(buf + 1, len);
  this->disable();
  this->receive_begin_();

  this->handle_frame_(buf, len + 1, rssi);
}

X3DOpening *X3DRFM69::find_opening_(uint32_t x3d_id) {
  for (auto *opening : this->openings_) {
    if (opening->get_x3d_id() == x3d_id)
      return opening;
  }
  return nullptr;
}

X3DOpening *X3DRFM69::opening_for_(uint32_t x3d_id, EntityBase *entity) {
  X3DOpening *opening = this->find_opening_(x3d_id);
  if (opening != nullptr)
    return opening;
  // Label used in the logs: the HA sub-device ("Bathroom window") when the
  // entity has one, else the entity name.
  const char *name = nullptr;
#ifdef USE_DEVICES
  if (entity->get_device() != nullptr)
    name = entity->get_device()->get_name();
#endif
  if (name == nullptr && !entity->get_name().empty())
    name = entity->get_name().c_str();
  // Lives as long as the firmware.
  opening = new X3DOpening(x3d_id, name != nullptr ? name : "?");  // NOLINT(cppcoreguidelines-owning-memory)
  this->openings_.push_back(opening);
  return opening;
}

bool X3DRFM69::first_sighting_(uint32_t x3d_id) {
  static constexpr size_t MAX_SIGHTED = 64;
  for (uint32_t id : this->sighted_) {
    if (id == x3d_id)
      return false;
  }
  if (this->sighted_.size() < MAX_SIGHTED)
    this->sighted_.push_back(x3d_id);
  return true;
}

void X3DRFM69::check_reversed_id_(uint32_t x3d_id, const char *id_str) {
  // The ID is little endian on air: raw bytes "85 cc ef" are device 0xEFCC85.
  const uint32_t reversed = (x3d_id & 0xFF) << 16 | (x3d_id & 0xFF00) | (x3d_id >> 16 & 0xFF);
  X3DOpening *opening = this->find_opening_(reversed);
  if (opening != nullptr && reversed != x3d_id && this->first_sighting_(x3d_id)) {
    ESP_LOGW(TAG, "'%s' is configured with x3d_id 0x%06" PRIX32 ", the raw byte order of device 0x%s. Use "
             "'x3d_id: 0x%s'", opening->get_name(), reversed, id_str, id_str);
  }
}

// Log levels: VERBOSE raw bytes and repeats, DEBUG one decoded line per frame,
// INFO what unclaimed devices send (to find their IDs), WARN unexpected bits.
void X3DRFM69::handle_frame_(const uint8_t *buf, size_t len, float rssi) {
  ESP_LOGV(TAG, "RX %.1f dBm: %s", rssi, to_hex(buf, len).c_str());

  X3DFrame common{};
  ParseResult res = parse_frame(buf, len, &common);
  if (res != ParseResult::OK) {
    ESP_LOGD(TAG, "Invalid frame (%s), length byte %u, %.1f dBm", parse_result_to_string(res), buf[0], rssi);
    return;
  }

  char id_str[7];
  snprintf(id_str, sizeof(id_str), "%06" PRIX32, common.device_id);

  if (!this->dedup_.is_new(common.device_id, common.msg_no, millis())) {
    ESP_LOGV(TAG, "0x%s: repeat of msg 0x%02X", id_str, common.msg_no);
    return;
  }

  X3DOpening *opening = this->find_opening_(common.device_id);
  if (opening == nullptr)
    this->check_reversed_id_(common.device_id, id_str);
  const std::string raw = to_hex(buf, common.length);

  if (common.msg_type != X3D_MSG_TYPE_SENSOR) {
    // Not a handle event (thermostats, Tydom, pairing, beacons...).
    const char *type_str = msg_type_to_string(common.msg_type);
    if (opening != nullptr) {
      ESP_LOGD(TAG, "'%s' (0x%s): %s message 0x%02X, msg 0x%02X, %.1f dBm", opening->get_name(), id_str, type_str,
               common.msg_type, common.msg_no, rssi);
      opening->publish_seen(rssi);
    } else if (this->log_unclaimed_devices_ && this->first_sighting_(common.device_id)) {
      ESP_LOGI(TAG, "Unclaimed device 0x%s: %s message 0x%02X (not a handle event), %.1f dBm. Logged once, "
               "next ones at DEBUG", id_str, type_str, common.msg_type, rssi);
    } else {
      ESP_LOGD(TAG, "Unclaimed device 0x%s: %s message 0x%02X, msg 0x%02X, %.1f dBm", id_str, type_str,
               common.msg_type, common.msg_no, rssi);
    }
    return;
  }

  SensorFrame frame{};
  res = parse_sensor_frame(buf, len, &frame);
  if (res != ParseResult::OK) {
    ESP_LOGD(TAG, "0x%s: sensor message rejected (%s), %.1f dBm", id_str, parse_result_to_string(res), rssi);
    ESP_LOGV(TAG, "0x%s: raw %s", id_str, raw.c_str());
    return;
  }

  const char *state_str = handle_state_to_string(frame.state);
  // For logs and last_frame only: the tilt position is shown next to the state.
  const char *shown_str = is_tilted(frame) ? "unlocked (tilt)" : state_str;
  const char *kind_str = sensor_kind_to_string(frame);

  if (opening == nullptr) {
    if (this->log_unclaimed_devices_) {
      this->first_sighting_(frame.device_id);
      ESP_LOGI(TAG, "Unclaimed device 0x%s (%s): %s, %.1f dBm -> claim it with 'x3d_id: 0x%s'",
               id_str, kind_str, shown_str, rssi, id_str);
    } else {
      ESP_LOGD(TAG, "Unclaimed device 0x%s (%s): %s, msg 0x%02X, %.1f dBm", id_str, kind_str, shown_str,
               frame.msg_no, rssi);
    }
  } else {
    ESP_LOGD(TAG, "'%s' (0x%s, %s): %s, msg 0x%02X, %.1f dBm", opening->get_name(), id_str, kind_str, shown_str,
             frame.msg_no, rssi);
  }
  ESP_LOGV(TAG, "0x%s: bytes 8-9: %02X %02X, raw %s", id_str, frame.flags, frame.state_byte, raw.c_str());

  if (has_unexpected_bits(frame)) {
    const bool named = opening != nullptr;
    ESP_LOGW(TAG, "Device 0x%s%s%s%s: unexpected bits in bytes 8-14, please report. raw: %s", id_str, named ? " '" : "",
             named ? opening->get_name() : "", named ? "'" : "", raw.c_str());
  }

  if (opening != nullptr)
    opening->publish(frame, rssi);
  if (this->last_frame_ != nullptr) {
    // "0xEFCC85 'Tilt window': unlocked (window, -67.5 dBm)" or "0xEFA00B unclaimed: locked (sliding, -71.0 dBm)".
    // The 0x prefix matters: copied without it, an all-digit ID would be read as decimal by YAML.
    char text[128];
    if (opening != nullptr) {
      snprintf(text, sizeof(text), "0x%s '%s': %s (%s, %.1f dBm)", id_str, opening->get_name(), shown_str, kind_str,
               rssi);
    } else {
      snprintf(text, sizeof(text), "0x%s unclaimed: %s (%s, %.1f dBm)", id_str, shown_str, kind_str, rssi);
    }
    this->last_frame_->publish_state(text);
  }

  this->event_trigger_.trigger(std::string(id_str), std::string(state_str), is_tilted(frame), std::string(kind_str),
                               rssi, raw);
}

void X3DRFM69::dump_config() {
  ESP_LOGCONFIG(TAG, "X3D RFM69 (SX1231) receiver:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DIO0 Pin: ", this->dio0_pin_);
  ESP_LOGCONFIG(TAG,
                "  Frequency: %" PRIu32 " Hz\n"
                "  Log unclaimed devices: %s\n"
                "  Restore state: %s\n"
                "  Claimed devices: %u",
                this->frequency_, YESNO(this->log_unclaimed_devices_), YESNO(this->restore_state_),
                static_cast<unsigned>(this->openings_.size()));
  if (this->is_failed())
    ESP_LOGE(TAG, "  Radio initialisation failed, see earlier log");
}

}  // namespace esphome::x3d_rfm69
