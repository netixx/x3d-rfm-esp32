"""`text_sensor: - platform: x3d_rfm69`: handle state declared by hand."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_DEVICE_ID, CONF_STATE

from .. import CONF_X3D_RFM69_ID, X3D_PLATFORM_SCHEMA, platform_final_validate

DEPENDENCIES = ["x3d_rfm69"]

CONFIG_SCHEMA = X3D_PLATFORM_SCHEMA.extend(
    {
        # locked / unlocked
        cv.Required(CONF_STATE): text_sensor.text_sensor_schema(
            icon="mdi:window-closed-variant",
        ),
    }
)


FINAL_VALIDATE_SCHEMA = platform_final_validate(CONF_STATE)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await text_sensor.new_text_sensor(config[CONF_STATE])
    cg.add(hub.add_state_sensor(config[CONF_DEVICE_ID], sens))
