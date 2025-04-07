/*
 * Project: IMU Sensor with Motion and Posture Detection
 * Author: João Dullius
 * Modified by: [Your Name]
 * License: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/logging/log.h>
 #include <math.h>
 
 LOG_MODULE_REGISTER(sensor_imu, LOG_LEVEL_INF);
 
 // Thresholds for detection
 #define ACCEL_THRESHOLD 0.9f      // Acceleration threshold in m/s²
 #define GYRO_THRESHOLD 0.9f       // Gyroscope threshold in rad/s
 #define STANDING_THRESHOLD 8.0f   // Z-axis threshold to consider "standing"
 #define SAMPLE_INTERVAL_MS 500    // Sampling interval in milliseconds
 
 // Posture states
 typedef enum {
     POSTURE_STANDING,      // Vertical posture (standing up)
     POSTURE_NON_STANDING   // Other postures (lying down, inclined, etc.)
 } posture_state_t;
 
 // Sensor initialization
 static const struct device *init_sensor(void) {
     const struct device *dev = DEVICE_DT_GET(DT_ALIAS(sensor_imu));
 
     if (!device_is_ready(dev)) {
         LOG_ERR("IMU sensor not found!");
         return NULL;
     }
     return dev;
 }
 
 // State detection function
 static void detect_state(const struct device *sensor, bool *is_moving, posture_state_t *posture) {
     struct sensor_value accel[3], gyro[3];
     float accel_magnitude, gyro_magnitude;
 
     // Fetch sensor data
     if (sensor_sample_fetch(sensor) < 0) {
         LOG_ERR("Failed to fetch sensor data");
         return;
     }
 
     sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
     sensor_channel_get(sensor, SENSOR_CHAN_GYRO_XYZ, gyro);
 
     // Calculate acceleration magnitude (subtracting gravity)
     accel_magnitude = sqrtf(
         powf(sensor_value_to_float(&accel[0]), 2) +
         powf(sensor_value_to_float(&accel[1]), 2) +
         powf(sensor_value_to_float(&accel[2]), 2)
     ) - 9.81f;
 
     // Calculate gyroscope magnitude
     gyro_magnitude = sqrtf(
         powf(sensor_value_to_float(&gyro[0]), 2) +
         powf(sensor_value_to_float(&gyro[1]), 2) +
         powf(sensor_value_to_float(&gyro[2]), 2)
     );
 
     // Determine if the device is moving
     *is_moving = (fabsf(accel_magnitude) > ACCEL_THRESHOLD) || 
                  (gyro_magnitude > GYRO_THRESHOLD);
 
     // Determine posture based on Z-axis acceleration
     float z_axis = sensor_value_to_float(&accel[2]);
     *posture = (z_axis > STANDING_THRESHOLD) ? POSTURE_STANDING : POSTURE_NON_STANDING;
 
     // Detailed debug logging
     LOG_DBG("Acceleration: X=%.3f, Y=%.3f, Z=%.3f", 
            (double)sensor_value_to_float(&accel[0]),
            (double)sensor_value_to_float(&accel[1]),
            (double)z_axis);
 
     LOG_DBG("Acceleration Magnitude: %.3f, Gyroscope Magnitude: %.3f",
            (double)accel_magnitude, (double)gyro_magnitude);
 }
 
 // Main application entry point
 int main(void) {
     const struct device *sensor = init_sensor();
     if (!sensor) {
         return -1;
     }
 
     LOG_INF("Starting motion and posture monitoring...");
 
     bool is_moving = false;
     posture_state_t posture = POSTURE_NON_STANDING;
 
     while (1) {
         detect_state(sensor, &is_moving, &posture);
 
         // Output formatted status
         LOG_INF("State: %s | %s", 
                is_moving ? "MOVING" : "STILL",
                posture == POSTURE_STANDING ? "STANDING" : "NON_STANDING");
 
         k_msleep(SAMPLE_INTERVAL_MS);
     }
 
     return 0;
 }
 