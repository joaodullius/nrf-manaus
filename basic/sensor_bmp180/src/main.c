/*
 * Projeto: Sensor BMP180
 * Autor: João Dullius
 * Licença: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/logging/log.h>
 
 LOG_MODULE_REGISTER(sensor_bmp180, LOG_LEVEL_DBG);
 
 static const struct device *init_sensor(void) {
	 const struct device *dev = DEVICE_DT_GET_ONE(bosch_bmp180);
	 
	 if (!device_is_ready(dev)) {
		 LOG_ERR("BMP180 não encontrado!");
		 return NULL;
	 }
	 return dev;
 }
 
 static void read_sensor_data(const struct device *sensor) {
	 struct sensor_value pressure, temp;
 
	 if (sensor_sample_fetch(sensor) < 0) {
		 LOG_ERR("Falha ao obter amostra do sensor");
		 return;
	 }
 
	 sensor_channel_get(sensor, SENSOR_CHAN_PRESS, &pressure);
	 sensor_channel_get(sensor, SENSOR_CHAN_DIE_TEMP, &temp);
 
	 LOG_INF("Pressão: %.3f kPa", sensor_value_to_double(&pressure));
	 LOG_INF("Temperatura: %.3f °C", sensor_value_to_double(&temp));
 }
 
 int main(void) {
	 const struct device *sensor = init_sensor();
	 if (!sensor) {
		 return -1;
	 }
 
	 while (1) {
		 read_sensor_data(sensor);
		 k_sleep(K_SECONDS(2));
	 }
	 return 0;
 }
 