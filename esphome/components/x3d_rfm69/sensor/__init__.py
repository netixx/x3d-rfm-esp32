"""`sensor: - platform: x3d_rfm69`: signal strength declared by hand."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_DEVICE_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import (
    CONF_RSSI,
    CONF_X3D_RFM69_ID,
    X3D_PLATFORM_SCHEMA,
    platform_final_validate,
)

DEPENDENCIES = ["x3d_rfm69"]

CONFIG_SCHEMA = X3D_PLATFORM_SCHEMA.extend(
    {
        # dBm, updated on every frame of the device: acts as "last seen"
        cv.Required(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


FINAL_VALIDATE_SCHEMA = platform_final_validate(CONF_RSSI)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await sensor.new_sensor(config[CONF_RSSI])
    cg.add(hub.add_rssi_sensor(config[CONF_DEVICE_ID], sens))
