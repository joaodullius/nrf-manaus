/*
 * Projeto: Sensor BH1750
 * Autor: João Dullius
 * Licença: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(sensor_bh1750, LOG_LEVEL_DBG);
 
 static const struct device *init_sensor(void) {
     const struct device *dev = DEVICE_DT_GET_ANY(rohm_bh1750);
     
     if (!device_is_ready(dev)) {
         LOG_ERR("BH1750 não encontrado!");
         return NULL;
     }
     return dev;
 }
 
 static void read_sensor_data(const struct device *sensor) {
     struct sensor_value light;
 
     if (sensor_sample_fetch(sensor) < 0) {
         LOG_ERR("Falha ao obter amostra do sensor");
         return;
     }
 
     if (sensor_channel_get(sensor, SENSOR_CHAN_LIGHT, &light) < 0) {
         LOG_ERR("Falha ao obter dados do sensor");
         return;
     }
 
     LOG_INF("Luminosidade: %.3f lx", sensor_value_to_double(&light));
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
 