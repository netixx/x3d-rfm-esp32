"""`lock: - platform: x3d_rfm69`: read-only lock following the handle of one
X3D device.

Workaround for Home Assistant's area dashboard, which only lists `lock`
entities in its Security section. Lock / unlock commands are refused: the
radio only receives. Prefer the `binary_sensor` (type lock) otherwise.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lock

from .. import (
    CONF_ENABLED,
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    X3D_ENTITY_SCHEMA,
    unique_entity_final_validate,
    x3d_ids,
    x3d_rfm69_ns,
)

DEPENDENCIES = ["x3d_rfm69"]

X3DLock = x3d_rfm69_ns.class_("X3DLock", lock.Lock)

# One ID, or several (sashes of one opening): combined state.
CONFIG_SCHEMA = (
    lock.lock_schema(X3DLock)
    .extend(X3D_ENTITY_SCHEMA)
    .extend({cv.Required(CONF_X3D_ID): x3d_ids})
)

FINAL_VALIDATE_SCHEMA = unique_entity_final_validate("lock", "lock")


async def to_code(config):
    if not config.get(CONF_ENABLED, True):
        return
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    var = await lock.new_lock(config)
    for x3d_id in config[CONF_X3D_ID]:
        cg.add(var.add_x3d_id(x3d_id))
        cg.add(hub.add_handle_listener(x3d_id, var, var))
