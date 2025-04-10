// gateway_ble.h

#ifndef GATEWAY_BLE_H
#define GATEWAY_BLE_H

#include "sensor_common.h"
#include "worker_shadow_service.h"
#include "bt_simple_service_client.h"

#include <zephyr/types.h>

typedef enum {
    BLE_DISCONNECTED,
    BLE_CONNECTING,
    BLE_CONNECTED,
} ble_state_t;

extern ble_state_t ble_state;

extern struct k_sem ble_connected;

/* Declare the global scanning state (1 = active, 0 = stopped) */
extern uint8_t scanning_state;


struct bt_simple_service;
typedef uint8_t (*concentrator_shadow_handler_t)(struct bt_simple_service *simple_service,
    const uint8_t *data, uint16_t len);


/**
 * Inicializa BLE e conecta ao concentrador.
 */
int gateway_ble_init(concentrator_shadow_handler_t client_handler);

/**
 * Lê os dados do shadow por leitura GATT.
 */
int gateway_ble_poll_shadow(concentrator_shadow_t *shadow_out);

#endif // GATEWAY_BLE_H
