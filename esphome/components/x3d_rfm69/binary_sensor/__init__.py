"""`binary_sensor: - platform: x3d_rfm69`: lock / tilt entities declared by hand."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_DEVICE_ID, DEVICE_CLASS_LOCK

from .. import (
    CONF_LOCK,
    CONF_TILT,
    CONF_X3D_RFM69_ID,
    X3D_PLATFORM_SCHEMA,
    platform_final_validate,
)

DEPENDENCIES = ["x3d_rfm69"]

CONFIG_SCHEMA = cv.All(
    X3D_PLATFORM_SCHEMA.extend(
        {
            # Handle position. Home Assistant "lock" class: ON = unlocked
            cv.Optional(CONF_LOCK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_LOCK,
            ),
            # Handle in tilt position. ON = tilted
            cv.Optional(CONF_TILT): binary_sensor.binary_sensor_schema(
                icon="mdi:window-open-variant",
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_LOCK, CONF_TILT),
)


FINAL_VALIDATE_SCHEMA = platform_final_validate(CONF_LOCK, CONF_TILT)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    if lock_config := config.get(CONF_LOCK):
        sens = await binary_sensor.new_binary_sensor(lock_config)
        cg.add(hub.add_lock_sensor(config[CONF_DEVICE_ID], sens))
    if tilt_config := config.get(CONF_TILT):
        sens = await binary_sensor.new_binary_sensor(tilt_config)
        cg.add(hub.add_tilt_sensor(config[CONF_DEVICE_ID], sens))
