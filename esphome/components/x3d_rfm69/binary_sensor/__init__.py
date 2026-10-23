import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_ID,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_WINDOW,
)

from .. import CONF_X3D_RFM69_ID, X3D_DEVICE_SCHEMA, X3DListener, x3d_rfm69_ns

DEPENDENCIES = ["x3d_rfm69"]

CONF_OPENING = "opening"
CONF_LOCK = "lock"

X3DWindowBinarySensor = x3d_rfm69_ns.class_("X3DWindowBinarySensor", X3DListener)

CONFIG_SCHEMA = cv.All(
    X3D_DEVICE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(X3DWindowBinarySensor),
            # ON = open
            cv.Optional(CONF_OPENING): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_WINDOW,
            ),
            # Home Assistant "lock" class: ON = unlocked
            cv.Optional(CONF_LOCK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_LOCK,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_OPENING, CONF_LOCK),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_DEVICE_ID])
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    cg.add(hub.register_listener(var))

    if opening_config := config.get(CONF_OPENING):
        sens = await binary_sensor.new_binary_sensor(opening_config)
        cg.add(var.set_opening_sensor(sens))
    if lock_config := config.get(CONF_LOCK):
        sens = await binary_sensor.new_binary_sensor(lock_config)
        cg.add(var.set_lock_sensor(sens))
