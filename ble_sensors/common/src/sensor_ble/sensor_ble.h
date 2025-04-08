#ifndef SENSOR_BLE_H
#define SENSOR_BLE_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include "sensor_common.h"

// BLE Advertising Data
extern const struct bt_data sensor_ad[];
extern const struct bt_data sd[];

// BLE Functions
void bt_ready(int err);
int init_ble(void);
void sensor_data_adv_update(const sensor_data_t *data);

#endif // SENSOR_BLE_H