/*
 * Projeto: Sensor MPU6050 com Interrupção
 * Autor: João Dullius
 * Licença: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(sensor_mpu6050_int, LOG_LEVEL_DBG);
 
 static void mpu6050_data_ready_handler(const struct device *sensor, const struct sensor_trigger *trig) {
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

 static const struct device *init_sensor(void) {
     const struct device *dev = DEVICE_DT_GET_ANY(invensense_mpu6050);
     
     if (!device_is_ready(dev)) {
         LOG_ERR("MPU6050 não encontrado!");
         return NULL;
     }
  
     LOG_INF("Configurando interrupção de dados prontos do MPU6050...");
     static struct sensor_trigger trigger;
     trigger.type = SENSOR_TRIG_DATA_READY;
     trigger.chan = SENSOR_CHAN_ALL;
 
     if (sensor_trigger_set(dev, &trigger, mpu6050_data_ready_handler) < 0) {
         LOG_ERR("Falha ao configurar interrupção de dados prontos do MPU6050");
         return NULL;
     }
 
     return dev;
 }
 
 int main(void) {
     const struct device *sensor = init_sensor();
     if (!sensor) {
         return -1;
     }
 
     LOG_INF("MPU6050 configurado! Aguardando eventos de dados prontos...");
 
     while (1) {
         k_sleep(K_FOREVER);
     }
 
     return 0;
 }
 