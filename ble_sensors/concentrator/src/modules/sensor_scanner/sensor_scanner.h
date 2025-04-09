#ifndef SENSOR_SCANNER_H
#define SENSOR_SCANNER_H

#include "sensor_common.h"
#include <zephyr/bluetooth/addr.h>

// New structure to encapsulate additional packet information
typedef struct sensor_packet {
    bt_addr_le_t addr;         // MAC address of the sender
    uint32_t timestamp;        // Local timestamp when data was received
    sensor_data_t sensor_data; // The sensor data from the advertisement
} sensor_packet_t;

/* Declare the global scanning state (1 = active, 0 = stopped) */
extern uint8_t scanning_state;

typedef void (*sensor_packet_handler_t)(const sensor_packet_t *parsed);

int sensor_scanner_init(sensor_packet_handler_t handler);
void sensor_packet_print(const sensor_packet_t *pkt);
int sensor_scanner_is_new_data(const sensor_packet_t *pkt);
int sensor_scanner_stop(void);
int sensor_scanner_start(void);

#endif // SENSOR_SCANNER_H
