#ifndef PARSER_GNSS_H
#define PARSER_GNSS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "sensor_common.h"

#define NMEA_BUFFER_SIZE 256  // Buffer for UBX messages
#define GNSS_FIX_TIMEOUT K_SECONDS(60)  // GNSS timeout period

// Function declarations
int parser_gnss_init(void);
int acquire_gnss_fix(sensor_data_t *data);

#endif // PARSER_GNSS_H