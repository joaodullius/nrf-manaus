#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "parser_gnss.h"
#include "sensor_common.h"
#include "sensor_ble.h"

#ifdef CONFIG_SENSOR_BLE_SERVICE
#include "sensor_ble_service.h"
#endif // CONFIG_SENSOR_BLE_SERVICE

#define SENSOR_READ_INTERVAL K_SECONDS(10)

LOG_MODULE_REGISTER(sensor_gnss_adv, LOG_LEVEL_INF);

int main(void) {
    sensor_data_t data;

    LOG_INF("Sensor GNSS BLE Initialized.");


    int err = init_ble();
    if (err) {
		LOG_ERR("Failed to Init BLE");
		return -1;
	}

#ifdef CONFIG_BT_SMP
    settings_load();
    bt_ready(err);
#endif // CONFIG_BT_SMP
	
    if (parser_gnss_init() != 0) {
        LOG_ERR("GNSS Initialization Failed!");
        return -1;
    }

    while (1) {
        if (acquire_gnss_fix(&data) == 0) {
            sensor_data_print(&data);
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