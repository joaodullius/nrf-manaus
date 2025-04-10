#include "accept_list_service.h"
#include "sensor_scanner.h"
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <bluetooth/scan.h>

LOG_MODULE_REGISTER(accept_list_service, LOG_LEVEL_INF);


// Track devices in the accept list
#define MAX_ACCEPT_LIST_DEVICES 10
static struct {
    bt_addr_le_t addr;
    bool in_accept_list;
} accept_list_devices[MAX_ACCEPT_LIST_DEVICES];

static int accept_list_count = 0;

// Implementation of the public functions
int accept_list_add_device(const bt_addr_le_t *addr)
{
    int err;
    char addr_str[BT_ADDR_LE_STR_LEN];
    
    // Convert the address to string for logging
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    
    // Check if device is already in our tracking list
    for (int i = 0; i < accept_list_count; i++) {
        if (bt_addr_le_cmp(&accept_list_devices[i].addr, addr) == 0) {
            if (accept_list_devices[i].in_accept_list) {
                LOG_INF("Device %s already in accept list", addr_str);
                return 0;
            }
            break;
        }
    }
    
    // Add the address to the accept list
    err = bt_le_filter_accept_list_add(addr);
    if (err) {
        LOG_ERR("Failed to add device %s to accept list (err %d)", addr_str, err);
        return err;
    }
    
    // Update our tracking
    if (accept_list_count < MAX_ACCEPT_LIST_DEVICES) {
        bt_addr_le_copy(&accept_list_devices[accept_list_count].addr, addr);
        accept_list_devices[accept_list_count].in_accept_list = true;
        accept_list_count++;
    }
    
    LOG_INF("Added device %s to accept list", addr_str);
    return 0;
}

int accept_list_remove_device(const bt_addr_le_t *addr)
{
    int err;
    char addr_str[BT_ADDR_LE_STR_LEN];
    
    // Convert the address to string for logging
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    
    // Remove the address from the accept list
    err = bt_le_filter_accept_list_remove(addr);
    if (err) {
        LOG_ERR("Failed to remove device %s from accept list (err %d)", addr_str, err);
        return err;
    }
    
    // Update our tracking
    for (int i = 0; i < accept_list_count; i++) {
        if (bt_addr_le_cmp(&accept_list_devices[i].addr, addr) == 0) {
            accept_list_devices[i].in_accept_list = false;
            break;
        }
    }
    
    LOG_INF("Removed device %s from accept list", addr_str);
    return 0;
}

int accept_list_clear(void)
{

    int err = bt_le_filter_accept_list_clear();
    if (err) {
        LOG_ERR("Failed to clear accept list (err %d)", err);
        return err;
    }
     
    // Update our tracking
    for (int i = 0; i < accept_list_count; i++) {
        accept_list_devices[i].in_accept_list = false;
    }
    
    LOG_INF("Cleared accept list");
    return 0;
}

// GATT write callbacks
static ssize_t on_add_device_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
    const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset > 0 || len != 7) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const uint8_t *raw = buf;

    bt_addr_le_t addr;
    memcpy(addr.a.val, raw, 6);
    addr.type = raw[6];  // 7th byte indicates the type (0x00 or 0x01)

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    LOG_INF("Add device %s to accept list", addr_str);

    accept_list_add_device(&addr);
    return len;
}

static ssize_t on_remove_device_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset > 0 || len != 7) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const uint8_t *raw = buf;

    bt_addr_le_t addr;
    memcpy(addr.a.val, raw, 6);
    addr.type = raw[6];

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
    LOG_INF("Remove device %s from accept list", addr_str);

    accept_list_remove_device(&addr);
    return len;
}


static ssize_t on_clear_list_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    accept_list_clear();
    return len;
}

// Read callback for the scanning control characteristic
static ssize_t scan_ctrl_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                              void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &scanning_state, sizeof(scanning_state));
}

// Write callback for the scanning control characteristic
static ssize_t scan_ctrl_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    if (offset != 0 || len != sizeof(uint8_t)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t new_state = *((uint8_t *)buf);
    if (new_state == scanning_state) {
        return len; // No change requested
    }

    if (new_state == 0) {
        int err = sensor_scanner_stop();
        if (err) {
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }
    } else if (new_state == 1) {
        int err = sensor_scanner_start();
        if (err) {
            return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
        }
    } else {
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
    }

    scanning_state = new_state;
    return len;
}


// Define the GATT service
BT_GATT_SERVICE_DEFINE(accept_list_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(ACCEPT_LIST_SERVICE_UUID)),
    
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(ADD_DEVICE_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, on_add_device_write, NULL),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(REMOVE_DEVICE_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, on_remove_device_write, NULL),
                          
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(CLEAR_LIST_CHAR_UUID),
                          BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_WRITE,
                          NULL, on_clear_list_write, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(SCAN_CONTROL_CHAR_UUID),
                            BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                            scan_ctrl_read, scan_ctrl_write, &scanning_state),

                          
);

int accept_list_service_init(void)
{
    // Initialize the accept list device tracking
    accept_list_count = 0;
    
    LOG_INF("Accept list service initialized");
    return 0;
}