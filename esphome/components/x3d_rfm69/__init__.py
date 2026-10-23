"""Receive Delta Dore X3D window sensors (K-LINE DVI) with an RFM69 / SX1231."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import spi
from esphome.const import CONF_DEVICE_ID, CONF_FREQUENCY, CONF_ID

CODEOWNERS = ["@netixx"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_X3D_RFM69_ID = "x3d_rfm69_id"
CONF_DIO0_PIN = "dio0_pin"
CONF_LOG_UNKNOWN_DEVICES = "log_unknown_devices"
CONF_LOG_RAW_FRAMES = "log_raw_frames"
CONF_ON_WINDOW_EVENT = "on_window_event"

x3d_rfm69_ns = cg.esphome_ns.namespace("x3d_rfm69")
X3DRFM69 = x3d_rfm69_ns.class_("X3DRFM69", cg.Component, spi.SPIDevice)
X3DListener = x3d_rfm69_ns.class_("X3DListener")


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


X3D_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_X3D_RFM69_ID): cv.use_id(X3DRFM69),
        cv.Required(CONF_DEVICE_ID): x3d_device_id,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(X3DRFM69),
            cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_FREQUENCY, default="868.95MHz"): cv.All(
                cv.frequency, cv.float_range(min=290e6, max=1020e6)
            ),
            cv.Optional(CONF_LOG_UNKNOWN_DEVICES, default=True): cv.boolean,
            cv.Optional(CONF_LOG_RAW_FRAMES, default=False): cv.boolean,
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
    cg.add(var.set_log_unknown_devices(config[CONF_LOG_UNKNOWN_DEVICES]))
    cg.add(var.set_log_raw_frames(config[CONF_LOG_RAW_FRAMES]))

    if CONF_ON_WINDOW_EVENT in config:
        await automation.build_automation(
            var.get_window_event_trigger(),
            [
                (cg.std_string, "device_id"),
                (cg.std_string, "state"),
                (cg.float_, "rssi"),
                (cg.std_string, "raw"),
            ],
            config[CONF_ON_WINDOW_EVENT],
        )
