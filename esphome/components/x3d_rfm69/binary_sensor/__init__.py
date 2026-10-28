"""`binary_sensor: - platform: x3d_rfm69`: handle lock or tilt of one X3D device."""

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_LOCK,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import (
    CONF_ENABLED,
    CONF_TIMEOUT,
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    DOMAIN,
    X3D_ENTITY_SCHEMA,
    unique_entity_final_validate,
    x3d_ids,
)

DEPENDENCIES = ["x3d_rfm69"]

TYPE_LOCK = "lock"
TYPE_TILT = "tilt"
TYPE_CONNECTIVITY = "connectivity"
TYPE_BATTERY = "battery"

CONFIG_SCHEMA = cv.typed_schema(
    {
        # Home Assistant "lock" class: shown as Locked / Unlocked, ON = unlocked
        # With several IDs (sashes of one opening): combined, unlocked as soon
        # as one sash is, locked when all are.
        TYPE_LOCK: binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_LOCK,
        )
        .extend(X3D_ENTITY_SCHEMA)
        .extend({cv.Required(CONF_X3D_ID): x3d_ids}),
        # Handle in tilt position (tilt / tilt-and-turn windows). ON = tilted
        TYPE_TILT: binary_sensor.binary_sensor_schema(
            icon="mdi:window-open-variant",
        ).extend(X3D_ENTITY_SCHEMA),
        # Battery class, ON = low: suspected low battery flag (byte 8 bit 0x10,
        # unconfirmed) or silent for longer than the hub `timeout`.
        TYPE_BATTERY: binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(X3D_ENTITY_SCHEMA),
        # ON while frames arrive, OFF after the hub `timeout` (battery dead?)
        TYPE_CONNECTIVITY: binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(X3D_ENTITY_SCHEMA),
    },
    default_type=TYPE_LOCK,
)


def _connectivity_needs_timeout(config):
    if config[CONF_TYPE] != TYPE_CONNECTIVITY or not config.get(CONF_ENABLED, True):
        return config
    for hub in fv.full_config.get().get(DOMAIN, []):
        if (
            hub[CONF_ID] == config[CONF_X3D_RFM69_ID]
            and hub[CONF_TIMEOUT].total_milliseconds == 0
        ):
            raise cv.Invalid(
                "A connectivity entity needs 'timeout' on x3d_rfm69, otherwise it "
                "never turns off"
            )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    unique_entity_final_validate("binary_sensor", TYPE_LOCK),
    _connectivity_needs_timeout,
)


async def to_code(config):
    if not config.get(CONF_ENABLED, True):
        return
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await binary_sensor.new_binary_sensor(config)
    if config[CONF_TYPE] == TYPE_LOCK:
        ids = config[CONF_X3D_ID]
        if len(ids) == 1:
            cg.add(hub.add_lock_sensor(ids[0], sens))
        else:
            cg.add(hub.add_combined_lock_sensor(sens, ids))
    elif config[CONF_TYPE] == TYPE_TILT:
        cg.add(hub.add_tilt_sensor(config[CONF_X3D_ID], sens))
    elif config[CONF_TYPE] == TYPE_BATTERY:
        cg.add(hub.add_battery_sensor(config[CONF_X3D_ID], sens))
    else:
        cg.add(hub.add_connectivity_sensor(config[CONF_X3D_ID], sens))
