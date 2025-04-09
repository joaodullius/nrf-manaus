#include "worker_shadow_service.h"
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(worker_shadow_service, CONFIG_WORKER_SHADOW_SERVICE_LOG_LEVEL);

// Global variable for the read characteristic "Worker Shadow".
static concentrator_shadow_t worker_shadow_var;

// Global flag for Worker Shadow notifications.
bool worker_shadow_notify_enabled;

static void worker_shadow_ccc_change(const struct bt_gatt_attr *attr, uint16_t value)
{
    worker_shadow_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Worker Shadow notifications %s", worker_shadow_notify_enabled ? "enabled" : "disabled");
}

static ssize_t on_worker_shadow_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset) {
    // TODO: Implement read callback for "Worker Shadow"
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &worker_shadow_var, sizeof(worker_shadow_var));
}

BT_GATT_SERVICE_DEFINE(worker_shadow_service_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(WORKER_SHADOW_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(WORKER_SHADOW_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           on_worker_shadow_read, NULL, &worker_shadow_var),
    BT_GATT_CCC(worker_shadow_ccc_change, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

/* Send notification for worker_shadow characteristic */
int send_worker_shadow_notification(concentrator_shadow_t worker_shadow)
{
    worker_shadow_var = worker_shadow;
    if (!worker_shadow_notify_enabled) {
        return -EACCES;
    }
    /* The value attribute is assumed to be at index 2 in the service structure. */
    return bt_gatt_notify(NULL, &worker_shadow_service_svc.attrs[2],
                            &worker_shadow_var, sizeof(worker_shadow_var));
}


int worker_shadow_service_service_init(void) {
    LOG_INF("Worker Shadow Service service initialized");
    return 0;
}