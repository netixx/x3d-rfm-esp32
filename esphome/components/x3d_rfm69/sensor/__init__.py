"""`sensor: - platform: x3d_rfm69`: signal strength of one X3D device."""

import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import (
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    X3D_ENTITY_SCHEMA,
    unique_entity_final_validate,
)

DEPENDENCIES = ["x3d_rfm69"]

# dBm, updated on every frame of the device: acts as "last seen"
CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(X3D_ENTITY_SCHEMA)

FINAL_VALIDATE_SCHEMA = unique_entity_final_validate("sensor", "rssi")


async def to_code(config):
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await sensor.new_sensor(config)
    cg.add(hub.add_rssi_sensor(config[CONF_X3D_ID], sens))
