#ifndef SENSOR_COMMON_H
#define SENSOR_COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <stdint.h>

#define SCALING_FACTOR 10000000  // Converts lat/lon to fixed-point (1e-7 degrees)
#define TEMP_SCALING_FACTOR 100  // Temperature stored in centi-degrees
#define PRESSURE_SCALING_FACTOR 10  // Pressure stored in 0.1 hPa
#define LIGHT_SCALING_FACTOR 1  // Light stored in milli-lux
#define ACCEL_SCALING_FACTOR 1000  // Acceleration stored in milli-g

#define COMPANY_ID 0x0059  // Company ID for the sensor manufacturer

// Sensor type definitions
typedef enum {
    SENSOR_TYPE_ERROR = 0,
    SENSOR_TYPE_LIGHT = 1,
    SENSOR_TYPE_TEMP = 2,
    SENSOR_TYPE_PRESSURE = 3,
    SENSOR_TYPE_ENVIRONMENTAL = 4,
    SENSOR_TYPE_ACCEL = 5,
    SENSOR_TYPE_GYRO = 6,
    SENSOR_TYPE_GNSS = 7,
    SENSOR_TYPE_MOTION = 8
} sensor_type_t;


// Structure for sensor data messages
typedef struct sensor_data_t {
    uint16_t company_id;
    uint8_t type;
    uint8_t padding_byte;   // Padding to align timestamp to 4 bytes
    uint32_t timestamp;
    union {
        int16_t values[8];  // Used for single or multiple values depending on the sensor type
    };
} sensor_data_t;

void sensor_data_print(const sensor_data_t *data);

#endif // SENSOR_COMMON_H