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

#include "sensor_common.h"
#include "sensor_ble.h"
#ifdef CONFIG_SENSOR_BLE_SERVICE
#include "sensor_ble_service.h"
#endif // CONFIG_SENSOR_BLE_SERVICE


#define SENSOR_READ_INTERVAL K_SECONDS(10)
 
LOG_MODULE_REGISTER(sensor_env_adv, LOG_LEVEL_INF);
 
static const struct device *init_sensor(void)
 {
	 const struct device *dev = DEVICE_DT_GET_ONE(bosch_bmp180);
	 if (!device_is_ready(dev)) {
		 LOG_ERR("Sensor not found or not ready");
		 return NULL;
	 }
	 return dev;
 }
 
static int read_sensor_data(const struct device *sensor, sensor_data_t *data) {
    struct sensor_value pressure, temp;

    // Get Timestamp
    uint32_t timestamp = k_uptime_get_32();

    // Fetch sensor data
    if (sensor_sample_fetch(sensor) < 0) {
        LOG_ERR("Failed to fetch sensor sample");
        return -1;  // Return error code
    }

    // Read pressure and temperature values
    if (sensor_channel_get(sensor, SENSOR_CHAN_PRESS, &pressure) < 0 ||
        sensor_channel_get(sensor, SENSOR_CHAN_DIE_TEMP, &temp) < 0) {
		LOG_ERR("Failed to read sensor channels");
        return -1;  // Return error code
    }

    // Log sensor values
    LOG_DBG("Pressure: %.3f kPa", sensor_value_to_double(&pressure));
    LOG_DBG("Temperature: %.3f °C", sensor_value_to_double(&temp));

    // Populate sensor_data_t structure
    data->company_id = COMPANY_ID;
    data->type = SENSOR_TYPE_ENVIRONMENTAL;
    data->timestamp = timestamp;
    data->values[0] = (int16_t)(sensor_value_to_double(&temp) * TEMP_SCALING_FACTOR);
    data->values[1] = (int16_t)(sensor_value_to_double(&pressure) * PRESSURE_SCALING_FACTOR);

    return 0;  // Return success
}
 
 /* Main Function */
int main(void) {
	sensor_data_t data;
    
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
        if (read_sensor_data(sensor, &data) == 0) {
            // Process the sensor data
			sensor_data_print(&data);
			LOG_HEXDUMP_DBG(&data, sizeof(sensor_data_t), "Sensor Data");
			sensor_data_adv_update(&data);
#ifdef CONFIG_SENSOR_BLE_SERVICE
            send_sensor_data_notification(data);
#endif // CONFIG_SENSOR_BLE_SERVICE

		} else {
		  LOG_ERR("Failed to read sensor data");
		}
#ifdef CONFIG_SENSOR_BLE_SERVICE
        k_sleep(K_SECONDS(sensor_interval_var)); 
#else
        k_sleep(SENSOR_READ_INTERVAL);
#endif // CONFIG_SENSOR_BLE_SERVICE
    }
    return 0;
}