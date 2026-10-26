"""Receive Delta Dore X3D lock detectors (K-LINE DVI) of windows and doors with an RFM69 / SX1231."""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation, pins
from esphome.components import spi
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_PLATFORM, CONF_TYPE

CODEOWNERS = ["@netixx"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]
MULTI_CONF = True
DOMAIN = "x3d_rfm69"

CONF_X3D_RFM69_ID = "x3d_rfm69_id"
CONF_X3D_ID = "x3d_id"
CONF_ENABLED = "enabled"
CONF_DIO0_PIN = "dio0_pin"
CONF_LOG_UNCLAIMED_DEVICES = "log_unclaimed_devices"
CONF_RESTORE_STATE = "restore_state"
CONF_ON_OPENING_EVENT = "on_opening_event"

x3d_rfm69_ns = cg.esphome_ns.namespace("x3d_rfm69")
X3DRFM69 = x3d_rfm69_ns.class_("X3DRFM69", cg.Component, spi.SPIDevice)


def x3d_id(value):
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


# Common keys of the `platform: x3d_rfm69` entities.
X3D_ENTITY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_X3D_RFM69_ID): cv.use_id(X3DRFM69),
        cv.Required(CONF_X3D_ID): x3d_id,
        # false: the entity is not created. Lets the profiles turn optional
        # entities on and off with a var, without conditional includes.
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
    }
)


def unique_entity_final_validate(domain, default_type):
    """FINAL_VALIDATE_SCHEMA of the platforms: one entity per X3D device and
    type, otherwise only one of them would be updated."""

    def key(conf):
        return (
            str(conf[CONF_X3D_RFM69_ID]),
            conf.get(CONF_X3D_ID),
            conf.get(CONF_TYPE, default_type),
        )

    def validator(config):
        if not config.get(CONF_ENABLED, True):
            return config
        same = [
            conf
            for conf in fv.full_config.get().get(domain, [])
            if conf.get(CONF_PLATFORM) == DOMAIN
            and conf.get(CONF_ENABLED, True)
            and key(conf) == key(config)
        ]
        if len(same) > 1:
            owner = (
                f"Device 0x{config[CONF_X3D_ID]:06X}"
                if CONF_X3D_ID in config
                else "The radio"
            )
            raise cv.Invalid(
                f"{owner} has {len(same)} '{key(config)[2]}' {domain} entities, keep one"
            )
        return config

    return validator


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(X3DRFM69),
            cv.Optional(CONF_DIO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_FREQUENCY, default="868.95MHz"): cv.All(
                cv.frequency, cv.float_range(min=290e6, max=1020e6)
            ),
            cv.Optional(CONF_LOG_UNCLAIMED_DEVICES, default=True): cv.boolean,
            # Publish the last saved handle position at boot.
            cv.Optional(CONF_RESTORE_STATE, default=True): cv.boolean,
            cv.Optional(CONF_ON_OPENING_EVENT): automation.validate_automation(
                single=True
            ),
            cv.Optional("log_raw_frames"): cv.invalid(
                "'log_raw_frames' was removed: raw frames are logged at VERBOSE level"
                " (logger: level: VERBOSE)"
            ),
            cv.Optional("openings"): cv.invalid(
                "'openings' was removed: use the profiles in esphome/profiles/ or "
                "'platform: x3d_rfm69' entities, see the README"
            ),
            cv.Optional("last_frame"): cv.invalid(
                "'last_frame' moved to 'text_sensor: - platform: x3d_rfm69, "
                "type: last_frame'"
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    # 1 MHz: at 8 MHz the ESP32 samples MISO too early in burst reads and the MSB
    # of each FIFO byte takes the previous byte's value (bad CRC on every frame).
    .extend(spi.spi_device_schema(cs_pin_required=True, default_data_rate=1e6))
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
    cg.add(var.set_restore_state(config[CONF_RESTORE_STATE]))

    if CONF_ON_OPENING_EVENT in config:
        await automation.build_automation(
            var.get_opening_event_trigger(),
            [
                (cg.std_string, "device_id"),
                (cg.std_string, "state"),
                (cg.bool_, "tilted"),
                (cg.std_string, "kind"),
                (cg.float_, "rssi"),
                (cg.std_string, "raw"),
            ],
            config[CONF_ON_OPENING_EVENT],
        )
