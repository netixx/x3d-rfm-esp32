"""`lock: - platform: x3d_rfm69`: read-only lock following the handle of one
X3D device.

Workaround for Home Assistant's area dashboard, which only lists `lock`
entities in its Security section. Lock / unlock commands are refused: the
radio only receives. Prefer the `binary_sensor` (type lock) otherwise.
"""

import esphome.codegen as cg
from esphome.components import lock

from .. import (
    CONF_ENABLED,
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    X3D_ENTITY_SCHEMA,
    unique_entity_final_validate,
    x3d_rfm69_ns,
)

DEPENDENCIES = ["x3d_rfm69"]

X3DLock = x3d_rfm69_ns.class_("X3DLock", lock.Lock)

CONFIG_SCHEMA = lock.lock_schema(X3DLock).extend(X3D_ENTITY_SCHEMA)

FINAL_VALIDATE_SCHEMA = unique_entity_final_validate("lock", "lock")


async def to_code(config):
    if not config.get(CONF_ENABLED, True):
        return
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    var = await lock.new_lock(config)
    cg.add(hub.add_handle_listener(config[CONF_X3D_ID], var, var))
