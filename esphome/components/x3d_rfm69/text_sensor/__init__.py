import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_ID,
    CONF_STATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_X3D_RFM69_ID, X3DRFM69, X3DListener, x3d_device_id, x3d_rfm69_ns

DEPENDENCIES = ["x3d_rfm69"]

CONF_LAST_FRAME = "last_frame"

X3DWindowTextSensor = x3d_rfm69_ns.class_("X3DWindowTextSensor", X3DListener)
ANY_DEVICE = 0xFFFFFFFF


def _validate(config):
    if CONF_STATE in config and CONF_DEVICE_ID not in config:
        raise cv.Invalid(f"'{CONF_STATE}' requires '{CONF_DEVICE_ID}'")
    if CONF_LAST_FRAME in config and CONF_DEVICE_ID in config:
        raise cv.Invalid(
            f"'{CONF_LAST_FRAME}' reports every device, remove '{CONF_DEVICE_ID}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(X3DWindowTextSensor),
            cv.GenerateID(CONF_X3D_RFM69_ID): cv.use_id(X3DRFM69),
            cv.Optional(CONF_DEVICE_ID): x3d_device_id,
            # locked / unlocked
            cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
                icon="mdi:window-closed-variant",
            ),
            # "EFCC85 unlocked (window, msg 0x55, -71.5 dBm)" for any device: discovery from HA
            cv.Optional(CONF_LAST_FRAME): text_sensor.text_sensor_schema(
                icon="mdi:radio-tower",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ),
    cv.has_exactly_one_key(CONF_STATE, CONF_LAST_FRAME),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config.get(CONF_DEVICE_ID, ANY_DEVICE))
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    cg.add(hub.register_listener(var))

    if state_config := config.get(CONF_STATE):
        sens = await text_sensor.new_text_sensor(state_config)
        cg.add(var.set_state_sensor(sens))
    if last_frame_config := config.get(CONF_LAST_FRAME):
        sens = await text_sensor.new_text_sensor(last_frame_config)
        cg.add(var.set_last_frame_sensor(sens))
