#include "sensor_common.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_common, CONFIG_SENSOR_COMMON_LOG_LEVEL);

void sensor_data_print(const sensor_data_t *data) {
    LOG_DBG("Sensor Data: Type: %d | Timestamp: %u", data->type, data->timestamp);
    LOG_HEXDUMP_DBG(data, sizeof(sensor_data_t), "Sensor Data:");

    switch (data->type) {
        case SENSOR_TYPE_TEMP:
            LOG_INF("Temperature: %.2f °C", (double)(data->values[0] / 100.0f));
            break;
        case SENSOR_TYPE_PRESSURE:
            LOG_INF("Pressure: %.1f hPa", (double)(data->values[0] / 10.0f));
            break;
        case SENSOR_TYPE_LIGHT:
            LOG_INF("Light Intensity: %d lx", data->values[0]);
            break;
        case SENSOR_TYPE_ENVIRONMENTAL:
            LOG_INF("Environmental: Temperature: %.2f °C | Pressure: %.1f hPa",
                    (double)(data->values[0] / 100.0f),  // Temperature in °C
                    (double)(data->values[1] / 10.0f));   // Pressure in hPa
            break;
        case SENSOR_TYPE_ACCEL:
            LOG_INF("Accel: X: %.3f g | Y: %.3f g | Z: %.3f g",
                    (double)(data->values[0] / 1000.0f),
                    (double)(data->values[1] / 1000.0f),
                    (double)(data->values[2] / 1000.0f));
            break;
        case SENSOR_TYPE_GYRO:
            LOG_INF("Gyro: X: %.3f °/s | Y: %.3f °/s | Z: %.3f °/s",
                    (double)(data->values[0] / 100.0f),
                    (double)(data->values[1] / 100.0f),
                    (double)(data->values[2] / 100.0f));
            break;
        case SENSOR_TYPE_GNSS: {
            LOG_HEXDUMP_DBG(data->values, sizeof(data->values), "GNSS Data:");
            LOG_DBG("Values: %xh | %xh | %xh | %xh | %xh | %xh | %xh",
                    data->values[0], data->values[1], data->values[2],
                    data->values[3], data->values[4], data->values[5], data->values[6]);
            int32_t latitude = ((int32_t)data->values[1] << 16) | (data->values[2] & 0xFFFF);
            int32_t longitude = ((int32_t)data->values[3] << 16) | (data->values[4] & 0xFFFF);
            int32_t altitude = ((int32_t)data->values[5] << 16) | (data->values[6] & 0xFFFF);
            LOG_INF("GNSS Fix Type: %d | Lat: %.7f° | Lon: %.7f° | Alt: %.3f m",
                    data->values[0],
                    (double)(latitude / 1e7),
                    (double)(longitude / 1e7),
                    (double)(altitude / 1000.0f));
            break;
        }

        case SENSOR_TYPE_MOTION:
            LOG_INF("Motion: %s | Posture: %s",
            data->values[0] ? "MOVING" : "STILL",
            data->values[1] ? "STANDING" : "NON_STANDING");
            break;

        default:
            LOG_WRN("Unknown sensor type received.");
            break;
    }

    LOG_HEXDUMP_DBG(data, sizeof(sensor_data_t), "Sensor Data:");
}