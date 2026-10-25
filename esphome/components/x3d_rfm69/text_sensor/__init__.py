"""`text_sensor: - platform: x3d_rfm69`: handle state of one X3D device, or the
last frame received from any device (discovery)."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_DIAGNOSTIC

from .. import (
    CONF_X3D_ID,
    CONF_X3D_RFM69_ID,
    X3D_ENTITY_SCHEMA,
    X3DRFM69,
    unique_entity_final_validate,
)

DEPENDENCIES = ["x3d_rfm69"]

TYPE_STATE = "state"
TYPE_LAST_FRAME = "last_frame"

CONFIG_SCHEMA = cv.typed_schema(
    {
        # locked / unlocked
        TYPE_STATE: text_sensor.text_sensor_schema(
            icon="mdi:window-closed-variant",
        ).extend(X3D_ENTITY_SCHEMA),
        # "0xEFCC85 'Bathroom window': unlocked (window, -67.5 dBm)", any device
        TYPE_LAST_FRAME: text_sensor.text_sensor_schema(
            icon="mdi:radio-tower",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend({cv.GenerateID(CONF_X3D_RFM69_ID): cv.use_id(X3DRFM69)}),
    },
    default_type=TYPE_STATE,
)

FINAL_VALIDATE_SCHEMA = unique_entity_final_validate("text_sensor", TYPE_STATE)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_X3D_RFM69_ID])
    sens = await text_sensor.new_text_sensor(config)
    if config[CONF_TYPE] == TYPE_LAST_FRAME:
        cg.add(hub.set_last_frame_sensor(sens))
    else:
        cg.add(hub.add_state_sensor(config[CONF_X3D_ID], sens))
