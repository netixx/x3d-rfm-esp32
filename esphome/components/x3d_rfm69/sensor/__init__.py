import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import CONF_X3D_RFM69_ID, X3D_DEVICE_SCHEMA, X3DListener, x3d_rfm69_ns

DEPENDENCIES = ["x3d_rfm69"]

CONF_RSSI = "rssi"

X3DWindowSensor = x3d_rfm69_ns.class_("X3DWindowSensor", X3DListener)

CONFIG_SCHEMA = X3D_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(X3DWindowSensor),
        # Published on every new event, so HA "last reported" doubles as "last seen".
        cv.Required(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_DEVICE_ID])
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    cg.add(hub.register_listener(var))

    sens = await sensor.new_sensor(config[CONF_RSSI])
    cg.add(var.set_rssi_sensor(sens))
