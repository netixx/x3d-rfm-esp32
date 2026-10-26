"""`binary_sensor: - platform: x3d_rfm69`: handle lock or tilt of one X3D device."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_TYPE, DEVICE_CLASS_LOCK

from .. import (
    CONF_ENABLED,
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    X3D_ENTITY_SCHEMA,
    unique_entity_final_validate,
)

DEPENDENCIES = ["x3d_rfm69"]

TYPE_LOCK = "lock"
TYPE_TILT = "tilt"

CONFIG_SCHEMA = cv.typed_schema(
    {
        # Home Assistant "lock" class: shown as Locked / Unlocked, ON = unlocked
        TYPE_LOCK: binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_LOCK,
        ).extend(X3D_ENTITY_SCHEMA),
        # Handle in tilt position (tilt / tilt-and-turn windows). ON = tilted
        TYPE_TILT: binary_sensor.binary_sensor_schema(
            icon="mdi:window-open-variant",
        ).extend(X3D_ENTITY_SCHEMA),
    },
    default_type=TYPE_LOCK,
)

FINAL_VALIDATE_SCHEMA = unique_entity_final_validate("binary_sensor", TYPE_LOCK)


async def to_code(config):
    if not config.get(CONF_ENABLED, True):
        return
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await binary_sensor.new_binary_sensor(config)
    if config[CONF_TYPE] == TYPE_LOCK:
        cg.add(hub.add_lock_sensor(config[CONF_X3D_ID], sens))
    else:
        cg.add(hub.add_tilt_sensor(config[CONF_X3D_ID], sens))
