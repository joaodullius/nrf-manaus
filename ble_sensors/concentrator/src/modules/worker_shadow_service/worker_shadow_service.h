#ifndef WORKER_SHADOW_SERVICE_H
#define WORKER_SHADOW_SERVICE_H

#include <zephyr/bluetooth/bluetooth.h>

typedef struct {
    uint32_t concentrator_timestamp;
    int16_t temperature;
    uint16_t pressure;
    int32_t latitude;
    int32_t longitude;
    uint8_t fix_type;
    uint8_t movement;
    uint8_t posture;
    int16_t light;
} concentrator_shadow_t;

// Service UUID
#define WORKER_SHADOW_SERVICE_UUID BT_UUID_128_ENCODE(0x5facdc62, 0xdf9e, 0x4403, 0xa258, 0x6f590aea3440)

// Worker Shadow characteristic
#define WORKER_SHADOW_CHAR_UUID BT_UUID_128_ENCODE(0x485ec8f4, 0xc56c, 0x4534, 0x9ba3, 0xd850bf804877)

int worker_shadow_service_init(void);
int send_worker_shadow_notification(concentrator_shadow_t worker_shadow);

#endif /* WORKER_SHADOW_SERVICE_H */