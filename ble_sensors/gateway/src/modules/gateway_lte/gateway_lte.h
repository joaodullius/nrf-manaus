// gateway_lte.h

#ifndef GATEWAY_LTE_H
#define GATEWAY_LTE_H

typedef enum {
    RRC_DISCONNECTED,
    RRC_CONNECTING,
    RRC_CONNECTED,
    RRC_IDLE,
    RRC_PSM
} rrc_state_t;

extern rrc_state_t rrc_state;

int gateway_lte_init(void);

#endif // GATEWAY_LTE_H
