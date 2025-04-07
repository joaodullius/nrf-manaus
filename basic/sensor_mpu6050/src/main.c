/*
 * Projeto: Sensor IMU
 * Autor: João Dullius
 * Licença: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(sensor_imu, LOG_LEVEL_DBG);
 
 static const struct device *init_sensor(void) {
     const struct device *dev = DEVICE_DT_GET(DT_ALIAS(sensor_imu));
     
     if (!device_is_ready(dev)) {
         LOG_ERR("Sensor IMU não encontrado!");
         return NULL;
     }
     return dev;
 }
 
 static void read_sensor_data(const struct device *sensor) {
     struct sensor_value accel[3], gyro[3];
 
     if (sensor_sample_fetch(sensor) < 0) {
         LOG_ERR("Falha ao obter amostra do sensor");
         return;
     }
 
     sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_X, &accel[0]);
     sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_Y, &accel[1]);
     sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_Z, &accel[2]);
 
     sensor_channel_get(sensor, SENSOR_CHAN_GYRO_X, &gyro[0]);
     sensor_channel_get(sensor, SENSOR_CHAN_GYRO_Y, &gyro[1]);
     sensor_channel_get(sensor, SENSOR_CHAN_GYRO_Z, &gyro[2]);
 
     LOG_INF("Aceleração: X=%.3f, Y=%.3f, Z=%.3f", 
             sensor_value_to_double(&accel[0]), 
             sensor_value_to_double(&accel[1]), 
             sensor_value_to_double(&accel[2]));
 
     LOG_INF("Giroscópio: X=%.3f, Y=%.3f, Z=%.3f", 
             sensor_value_to_double(&gyro[0]), 
             sensor_value_to_double(&gyro[1]), 
             sensor_value_to_double(&gyro[2]));
 }
 
 int main(void) {
     const struct device *sensor = init_sensor();
     if (!sensor) {
         return -1;
     }
 
     while (1) {
         read_sensor_data(sensor);
         k_sleep(K_SECONDS(1));
     }
     return 0;
 }
 