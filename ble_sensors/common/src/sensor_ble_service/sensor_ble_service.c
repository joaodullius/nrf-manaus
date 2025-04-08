#include "sensor_ble_service.h"
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor_ble_service, CONFIG_SENSOR_BLE_SERVICE_LOG_LEVEL);

// Global variable for the read characteristic "Sensor Data".
static sensor_data_t sensor_data_var;

// Global flag for Sensor Data notifications.
bool sensor_data_notify_enabled;

static void sensor_data_ccc_change(const struct bt_gatt_attr *attr, uint16_t value)
{
    sensor_data_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Sensor Data notifications %s", sensor_data_notify_enabled ? "enabled" : "disabled");
}

uint8_t sensor_interval_var = 10;
static ssize_t on_sensor_data_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset) {
    // TODO: Implement read callback for "Sensor Data"
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &sensor_data_var, sizeof(sensor_data_var));
}

static ssize_t on_sensor_interval_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    void *buf, uint16_t len, uint16_t offset) {
    // TODO: Implementar callback de leitura para "Sensor Interval"
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &sensor_interval_var, sizeof(sensor_interval_var));
}
static ssize_t on_sensor_interval_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    sensor_interval_var = *(uint16_t *)buf;
    LOG_INF("Sensor Interval set to %u seconds", sensor_interval_var);
    return len;
}

#ifdef CONFIG_BT_SMP
BT_GATT_SERVICE_DEFINE(sensor_ble_service_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(SENSOR_BLE_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SENSOR_DATA_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_AUTHEN,
                           on_sensor_data_read, NULL, &sensor_data_var),
    BT_GATT_CCC(sensor_data_ccc_change, BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SENSOR_INTERVAL_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE,
                           on_sensor_interval_read, on_sensor_interval_write, &sensor_interval_var)
);
#else
BT_GATT_SERVICE_DEFINE(sensor_ble_service_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(SENSOR_BLE_SERVICE_UUID)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SENSOR_DATA_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           on_sensor_data_read, NULL, &sensor_data_var),
    BT_GATT_CCC(sensor_data_ccc_change, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SENSOR_INTERVAL_CHAR_UUID),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           on_sensor_interval_read, on_sensor_interval_write, &sensor_interval_var)
);
#endif


/* Send notification for sensor_data characteristic */
int send_sensor_data_notification(sensor_data_t sensor_data)
{
    sensor_data_var = sensor_data;
    if (!sensor_data_notify_enabled) {
        return -EACCES;
    }
    /* The value attribute is assumed to be at index 2 in the service structure. */
    return bt_gatt_notify(NULL, &sensor_ble_service_svc.attrs[2],
                            &sensor_data_var, sizeof(sensor_data_var));
}


int sensor_ble_service_init(void) {
    LOG_INF("BLE Sensor Service service initialized");
    return 0;
}