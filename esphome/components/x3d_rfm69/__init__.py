"""Receive Delta Dore X3D window sensors (K-LINE DVI) with an RFM69 / SX1231."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import binary_sensor, sensor, spi, text_sensor
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_NAME,
    CONF_STATE,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

CODEOWNERS = ["@netixx"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = True

CONF_DIO0_PIN = "dio0_pin"
CONF_LOG_UNCLAIMED_DEVICES = "log_unclaimed_devices"
CONF_LOG_RAW_FRAMES = "log_raw_frames"
CONF_ON_WINDOW_EVENT = "on_window_event"
CONF_LAST_FRAME = "last_frame"
CONF_WINDOWS = "windows"
CONF_DEVICE = "device"
CONF_LOCK = "lock"
CONF_RSSI = "rssi"

x3d_rfm69_ns = cg.esphome_ns.namespace("x3d_rfm69")
X3DRFM69 = x3d_rfm69_ns.class_("X3DRFM69", cg.Component, spi.SPIDevice)
X3DWindow = x3d_rfm69_ns.class_("X3DWindow")

# Entities of each window: config key -> (name suffix, name inside a sub-device, on by default)
WINDOW_ENTITIES = {
    CONF_LOCK: ("lock", "Lock", True),
    CONF_STATE: ("state", "State", False),
    CONF_RSSI: ("RSSI", "RSSI", False),
}


def x3d_device_id(value):
    """Accept 0xEFCC85, 15715461 or "EFCC85" (as printed in the logs)."""
    if isinstance(value, str):
        text = value.strip()
        if text.lower().startswith("0x"):
            text = text[2:]
        try:
            value = int(text, 16)
        except ValueError as err:
            raise cv.Invalid(f"Invalid X3D device id '{value}'") from err
    value = cv.int_(value)
    if not 0 <= value <= 0xFFFFFF:
        raise cv.Invalid("X3D device id must be 3 bytes (0x000000 - 0xFFFFFF)")
    return value


def _fill_window_entities(value):
    """Create the window entity configs, named after the window.

    `lock` is on by default, `state` and `rssi` with `true`. Each one takes entity
    options, or `false` to disable it. With `device:`, entities go to that
    sub-device and are named "Lock", "State", "RSSI".
    """
    if not isinstance(value, dict):
        raise cv.Invalid("Expected a mapping with 'device_id' and 'name'")
    value = dict(value)
    name = value.get(CONF_NAME)
    device = value.get(CONF_DEVICE)
    for key, (suffix, sub_device_name, default_on) in WINDOW_ENTITIES.items():
        entity = value.get(key, default_on)
        if entity is False:
            value.pop(key, None)
            continue
        if entity is True or entity is None:
            entity = {}
        if not isinstance(entity, dict):
            raise cv.Invalid(f"'{key}' must be entity options or false")
        entity = dict(entity)
        if device is not None:
            entity.setdefault(CONF_DEVICE_ID, device)
            entity.setdefault(CONF_NAME, sub_device_name)
        elif name is not None:
            entity.setdefault(CONF_NAME, f"{name} {suffix}")
        value[key] = entity
    return value


WINDOW_SCHEMA = cv.All(
    _fill_window_entities,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(X3DWindow),
            cv.Required(CONF_DEVICE_ID): x3d_device_id,
            cv.Required(CONF_NAME): cv.string_strict,
            # Optional sub-device declared under `esphome: devices:`
            cv.Optional(CONF_DEVICE): cv.sub_device_id,
            # Handle position. Home Assistant "lock" class: ON = unlocked
            cv.Optional(CONF_LOCK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_LOCK,
            ),
            # locked / unlocked
            cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
                icon="mdi:window-closed-variant",
            ),
            # Published on every event, so HA "last reported" doubles as "last seen".
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ),
)


def _unique_device_ids(windows):
    seen = set()
    for window in windows:
        device_id = window[CONF_DEVICE_ID]
        if device_id in seen:
            raise cv.Invalid(f"Device 0x{device_id:06X} is declared twice")
        seen.add(device_id)
    return windows


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(X3DRFM69),
            cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_FREQUENCY, default="868.95MHz"): cv.All(
                cv.frequency, cv.float_range(min=290e6, max=1020e6)
            ),
            cv.Optional(CONF_LOG_UNCLAIMED_DEVICES, default=True): cv.boolean,
            cv.Optional(CONF_LOG_RAW_FRAMES, default=False): cv.boolean,
            cv.Optional(CONF_WINDOWS, default=[]): cv.All(
                cv.ensure_list(WINDOW_SCHEMA), _unique_device_ids
            ),
            # Last event of any device, claimed or not: "EFCC85 'Tilt window': unlocked (...)"
            cv.Optional(CONF_LAST_FRAME): text_sensor.text_sensor_schema(
                icon="mdi:radio-tower",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ON_WINDOW_EVENT): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_data_rate=8e6))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_DIO0_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_DIO0_PIN])
        cg.add(var.set_dio0_pin(pin))
    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_log_unclaimed_devices(config[CONF_LOG_UNCLAIMED_DEVICES]))
    cg.add(var.set_log_raw_frames(config[CONF_LOG_RAW_FRAMES]))

    for window_config in config[CONF_WINDOWS]:
        window = cg.new_Pvariable(
            window_config[CONF_ID],
            window_config[CONF_DEVICE_ID],
            window_config[CONF_NAME],
        )
        cg.add(var.add_window(window))
        if lock_config := window_config.get(CONF_LOCK):
            sens = await binary_sensor.new_binary_sensor(lock_config)
            cg.add(window.set_lock_sensor(sens))
        if state_config := window_config.get(CONF_STATE):
            sens = await text_sensor.new_text_sensor(state_config)
            cg.add(window.set_state_sensor(sens))
        if rssi_config := window_config.get(CONF_RSSI):
            sens = await sensor.new_sensor(rssi_config)
            cg.add(window.set_rssi_sensor(sens))

    if last_frame_config := config.get(CONF_LAST_FRAME):
        sens = await text_sensor.new_text_sensor(last_frame_config)
        cg.add(var.set_last_frame_sensor(sens))

    if CONF_ON_WINDOW_EVENT in config:
        await automation.build_automation(
            var.get_window_event_trigger(),
            [
                (cg.std_string, "device_id"),
                (cg.std_string, "state"),
                (cg.std_string, "kind"),
                (cg.float_, "rssi"),
                (cg.std_string, "raw"),
            ],
            config[CONF_ON_WINDOW_EVENT],
        )
