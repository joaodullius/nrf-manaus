#ifndef SENSOR_BLE_SERVICE_H
#define SENSOR_BLE_SERVICE_H

#include <zephyr/bluetooth/bluetooth.h>
#include "../../../../sensor_common/src/sensor_common.h"

// Service UUID
#define SENSOR_BLE_SERVICE_UUID BT_UUID_128_ENCODE(0xe700afec, 0x3812, 0x4db9, 0xa1d9, 0x2c3273d2a411)

// Sensor Data characteristic
#define SENSOR_DATA_CHAR_UUID BT_UUID_128_ENCODE(0x9c330129, 0xb199, 0x41f3, 0xadf4, 0x2a51a76aad77)

// Sensor Interval characteristic
#define SENSOR_INTERVAL_CHAR_UUID BT_UUID_128_ENCODE(0x527c07fd, 0x600b, 0x409a, 0xad79, 0x1a885d7f9922)

int sensor_ble_service_init(void);
int send_sensor_data_notification(sensor_data_t sensor_data);


#endif /* SENSOR_BLE_SERVICE_H */




extern uint8_t sensor_interval_var; // Default sensor interval in seconds