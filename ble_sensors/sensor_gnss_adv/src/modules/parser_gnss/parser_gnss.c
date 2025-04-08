#include "parser_gnss.h"
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(sensor_gnss, CONFIG_PARSER_GNSS_LOG_LEVEL);

// Semaphores
K_SEM_DEFINE(gnss_fix_done, 0, 1);
K_SEM_DEFINE(gnss_fix_timeout, 0, 1);
K_SEM_DEFINE(gnss_stop_uart_rx, 0, 1);

// UART Device and Buffer
#define UART_NODE DT_NODELABEL(arduino_serial)
static const struct device *uart = DEVICE_DT_GET(UART_NODE);

static sensor_data_t *global_sensor_data = {0};

double encode_nmea_to_double(const char *nmea_coord, char direction) {
    if (nmea_coord == NULL) {
        return 0.0;
    }
    double raw_value = atof(nmea_coord);
    int degrees;
    double minutes;
    double decimal_degrees;
    if (strlen(nmea_coord) > 9) {
        degrees = (int)(raw_value / 100);
        minutes = raw_value - (degrees * 100);
    } else {
        degrees = (int)(raw_value / 100);
        minutes = raw_value - (degrees * 100);
    }
    decimal_degrees = degrees + (minutes / 100.0);
    if (direction == 'S' || direction == 'W') {
        decimal_degrees = -decimal_degrees;
    }
    return decimal_degrees;
}

static void process_nmea_sentence(char *nmea, sensor_data_t *sensor_data) {
    if (strstr(nmea, "$GNGGA") == NULL) {
        return;
    }

    char *token;
    int field = 0;
    char *utc_time = NULL;
    char *lat_str = NULL, *lat_dir = NULL;
    char *lon_str = NULL, *lon_dir = NULL;
    char *alt_str = NULL;
    int fix_quality = 0;

    token = strtok(nmea, ",");
    while (token != NULL) {
        switch (field) {
        case 1:
            utc_time = token;
            break;
        case 2:
            lat_str = token;
            break;
        case 3:
            lat_dir = token;
            break;
        case 4:
            lon_str = token;
            break;
        case 5:
            lon_dir = token;
            break;
        case 6:
            fix_quality = atoi(token);
            break;
        case 9:
            alt_str = token;
            break;
        default:
            break;
        }
        token = strtok(NULL, ",");
        field++;
    }

    if (fix_quality > 0 && lat_str && lat_dir && lon_str && lon_dir && alt_str) {
        double latitude = encode_nmea_to_double(lat_str, lat_dir[0]);
        double longitude = encode_nmea_to_double(lon_str, lon_dir[0]);
        double altitude = atof(alt_str);

        sensor_data->company_id = COMPANY_ID;
        sensor_data->type = SENSOR_TYPE_GNSS;
        sensor_data->timestamp = k_uptime_get_32();
        sensor_data->values[0] = (int16_t)fix_quality;

        int32_t lat_fixed = (int32_t)(latitude * 1e7);
        sensor_data->values[1] = (int16_t)(lat_fixed >> 16);
        sensor_data->values[2] = (int16_t)(lat_fixed & 0xFFFF);

        int32_t lon_fixed = (int32_t)(longitude * 1e7);
        sensor_data->values[3] = (int16_t)(lon_fixed >> 16);
        sensor_data->values[4] = (int16_t)(lon_fixed & 0xFFFF);

        int32_t alt_fixed = (int32_t)(altitude * 1000.0);
        sensor_data->values[5] = (int16_t)(alt_fixed >> 16);
        sensor_data->values[6] = (int16_t)(alt_fixed & 0xFFFF);

        k_sem_give(&gnss_fix_done);
    } else {
        LOG_WRN("Sentença NMEA incompleta ou fix inválido.");
    }
}

K_MSGQ_DEFINE(nmea_msgq, sizeof(char[NMEA_BUFFER_SIZE]), 10, 4);

static void gnss_thread(void) {
    char nmea_buffer[NMEA_BUFFER_SIZE];

    while (1) {
        if (k_msgq_get(&nmea_msgq, nmea_buffer, K_FOREVER) == 0) {
            process_nmea_sentence(nmea_buffer, global_sensor_data);
        }
    }
}

K_THREAD_DEFINE(gnss_thread_id, 1024, gnss_thread, NULL, NULL, NULL, 7, 0, 0);

static void uart_cb(const struct device *dev, void *user_data) {
    uart_irq_update(dev);
    if (uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t byte;
            static char rx_buf[NMEA_BUFFER_SIZE];
            static int rx_buf_pos = 0;

            while (uart_irq_rx_ready(dev)) {
                uart_fifo_read(dev, &byte, 1);

                if (rx_buf_pos < sizeof(rx_buf) - 1) {
                    rx_buf[rx_buf_pos++] = byte;
                }

                if (byte == '\n' && rx_buf_pos > 0) {
                    rx_buf[rx_buf_pos] = '\0';
                    k_msgq_put(&nmea_msgq, rx_buf, K_NO_WAIT);
                    rx_buf_pos = 0;
                }
            }
        }
    }
}

int parser_gnss_init(void) {
    if (!device_is_ready(uart)) {
        LOG_ERR("UART device not ready");
        return -1;
    }
    uart_irq_callback_set(uart, uart_cb);

    return 0;
}

int acquire_gnss_fix(sensor_data_t *data) {
    LOG_INF("Requesting GNSS Fix...");

    global_sensor_data = data;

    k_sem_reset(&gnss_fix_done);
    k_sem_reset(&gnss_fix_timeout);
    k_sem_reset(&gnss_stop_uart_rx);

    uart_irq_rx_enable(uart);

    int ret = k_sem_take(&gnss_fix_done, GNSS_FIX_TIMEOUT);

    k_sem_give(&gnss_stop_uart_rx);
    uart_irq_rx_disable(uart);

    return ret;
}