/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/settings/settings.h>

#include <math.h>

#include "sensor_common.h"
#include "sensor_ble.h"
#ifdef CONFIG_SENSOR_BLE_SERVICE
#include "sensor_ble_service.h"
#endif // CONFIG_SENSOR_BLE_SERVICE


// Thresholds for detection
#define ACCEL_THRESHOLD 0.9f      // Acceleration threshold in m/s²
#define GYRO_THRESHOLD 0.9f       // Gyroscope threshold in rad/s
#define STANDING_THRESHOLD 8.0f   // Z-axis threshold to consider "standing"
#define SAMPLE_INTERVAL_MS 500    // Sampling interval in milliseconds
 
LOG_MODULE_REGISTER(sensor_mov_adv, LOG_LEVEL_INF);
 
static const struct device *init_sensor(void)
 {
    const struct device *dev = DEVICE_DT_GET(DT_ALIAS(sensor_imu));
    if (!device_is_ready(dev)) {
        LOG_ERR("IMU sensor not found!");
        return NULL;
    }
    return dev;
 }
 
 static int read_sensor_data(const struct device *sensor, sensor_data_t *data) {
    struct sensor_value accel[3], gyro[3];
    float accel_magnitude, gyro_magnitude;
    uint32_t timestamp = k_uptime_get_32();

    if (sensor_sample_fetch(sensor) < 0) {
        LOG_ERR("Failed to fetch sensor data");
        return -1;
    }

    sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
    sensor_channel_get(sensor, SENSOR_CHAN_GYRO_XYZ, gyro);

    accel_magnitude = sqrtf(
        powf(sensor_value_to_float(&accel[0]), 2) +
        powf(sensor_value_to_float(&accel[1]), 2) +
        powf(sensor_value_to_float(&accel[2]), 2)
    ) - 9.81f;

    gyro_magnitude = sqrtf(
        powf(sensor_value_to_float(&gyro[0]), 2) +
        powf(sensor_value_to_float(&gyro[1]), 2) +
        powf(sensor_value_to_float(&gyro[2]), 2)
    );

    bool is_moving = (fabsf(accel_magnitude) > ACCEL_THRESHOLD) || 
                     (gyro_magnitude > GYRO_THRESHOLD);

    float z_axis = sensor_value_to_float(&accel[2]);
    bool is_standing = (z_axis > STANDING_THRESHOLD);

    // Fill sensor_data_t structure
    data->company_id = COMPANY_ID;
    data->type = SENSOR_TYPE_MOTION;
    data->timestamp = timestamp;
    data->values[0] = is_moving ? 1 : 0;
    data->values[1] = is_standing ? 1 : 0;

    LOG_DBG("Accel magnitude: %.3f | Gyro magnitude: %.3f", 
            (double)accel_magnitude, (double)gyro_magnitude);
    LOG_DBG("Z-axis: %.3f", (double)z_axis);

    return 0;
}

 
/* Main Function */
int main(void) {
	sensor_data_t data, last;
    
    // Zero-initialize the entire struct to avoid garbage values
    memset(&data, 0, sizeof(sensor_data_t));

    LOG_INF("Sensor Temp BLE\n");

	int err = init_ble();
    if (err) {
		LOG_ERR("Failed to Init BLE");
		return -1;
	}
#ifdef CONFIG_BT_SMP
    settings_load();
    bt_ready(err);
#endif // CONFIG_BT_SMP

	// Initialize Sensor
    const struct device *sensor = init_sensor();
    if (!sensor) {
        LOG_ERR("Failed to init sensor");
        return -1;  // Exit if the sensor is not found
    }

	while (1) {
		// Read sensor data
        read_sensor_data(sensor, &data);

        // Check if MOVING and STANDING values changed from last sample
        if (data.values[0] != last.values[0] || data.values[1] != last.values[1]) {
            // Update the last values
            last.values[0] = data.values[0];
            last.values[1] = data.values[1];
            sensor_data_print(&data);
			LOG_HEXDUMP_DBG(&data, sizeof(sensor_data_t), "Sensor Data");
			sensor_data_adv_update(&data);
#ifdef CONFIG_SENSOR_BLE_SERVICE
            send_sensor_data_notification(data);
#endif // CONFIG_SENSOR_BLE_SERVICE

		} else {
		  LOG_ERR("Failed to read sensor data");
		}
        k_sleep(K_MSEC(SAMPLE_INTERVAL_MS)); 
    }
    return 0;
}