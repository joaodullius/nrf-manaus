#ifndef ACCEPT_LIST_SERVICE_H
#define ACCEPT_LIST_SERVICE_H

#include <zephyr/bluetooth/bluetooth.h>

// Define UUIDs for the service and characteristics
#define ACCEPT_LIST_SERVICE_UUID BT_UUID_128_ENCODE(0x227a5db3, 0xcae4, 0x435f, 0x8ed1, 0x2db3fbe438c9)
#define ADD_DEVICE_CHAR_UUID     BT_UUID_128_ENCODE(0x22f64492, 0xbb02, 0x4ccf, 0x9612, 0xc749be0c897d)
#define REMOVE_DEVICE_CHAR_UUID  BT_UUID_128_ENCODE(0xaebd5484, 0x453a, 0x4bb5, 0xa8ea, 0x1593405a4a36)
#define CLEAR_LIST_CHAR_UUID     BT_UUID_128_ENCODE(0x67e19398, 0x8879, 0x40e8, 0xb513, 0xb75f0268278c)
#define SCAN_CONTROL_CHAR_UUID   BT_UUID_128_ENCODE(0x4f3b5a2c, 0x8d1e, 0x4b6c, 0x9f7d, 0x5a2c8d1e4b6c)

// Track devices in the accept list

/**
 * @brief Initialize the accept list service
 *
 * @return 0 on success, negative error code otherwise
 */
int accept_list_service_init(void);

/**
 * @brief Add a device to the accept list
 *
 * @param addr Bluetooth LE address to add
 * @return 0 on success, negative error code otherwise
 */
int accept_list_add_device(const bt_addr_le_t *addr);

/**
 * @brief Remove a device from the accept list
 *
 * @param addr Bluetooth LE address to remove
 * @return 0 on success, negative error code otherwise
 */
int accept_list_remove_device(const bt_addr_le_t *addr);

/**
 * @brief Clear the entire accept list
 *
 * @return 0 on success, negative error code otherwise
 */
int accept_list_clear(void);

#endif /* aACCEPT_LIST_SERVICE_H */