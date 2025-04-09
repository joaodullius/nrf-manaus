#include "sensor_scanner.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/scan.h>

LOG_MODULE_REGISTER(sensor_scanner, CONFIG_SENSOR_SCANNER_LOG_LEVEL);

static sensor_packet_handler_t sensor_handler = NULL;

// Remove duplicate packets based on MAC address and timestamp
typedef struct {
    bt_addr_le_t addr;
    uint32_t last_sensor_timestamp;
} sensor_record_t;

#define MAX_SENSOR_RECORDS 10
static sensor_record_t sensor_records[MAX_SENSOR_RECORDS];
static int sensor_records_count = 0;

int sensor_scanner_is_new_data(const sensor_packet_t *pkt)
{
    // Check if a record for this MAC already exists.
    for (int i = 0; i < sensor_records_count; i++) {
        if (bt_addr_le_cmp(&sensor_records[i].addr, &pkt->addr) == 0) {
            // If the timestamp is the same, it's a duplicate.
            if (sensor_records[i].last_sensor_timestamp == pkt->sensor_data.timestamp) {
                return 0;
            }
            // Update the stored timestamp if the data is new.
            sensor_records[i].last_sensor_timestamp = pkt->sensor_data.timestamp;
            return 1;
        }
    }
    // If this MAC address hasn't been seen before, add a new record.
    if (sensor_records_count < MAX_SENSOR_RECORDS) {
        bt_addr_le_copy(&sensor_records[sensor_records_count].addr, &pkt->addr);
        sensor_records[sensor_records_count].last_sensor_timestamp = pkt->sensor_data.timestamp;
        sensor_records_count++;
        return 1;
    }
    // If there's no room for a new record, decide how to handle it.
    return 1;
}

void sensor_packet_print(const sensor_packet_t *pkt)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(&pkt->addr, addr_str, sizeof(addr_str));

    char sensor_info[256] = {0};

    switch (pkt->sensor_data.type) {
    case SENSOR_TYPE_TEMP:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Temperature: %.2f °C",
                 pkt->sensor_data.values[0] / (double)TEMP_SCALING_FACTOR);
        break;
    case SENSOR_TYPE_PRESSURE:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Pressure: %.1f hPa",
                 pkt->sensor_data.values[0] / (double)PRESSURE_SCALING_FACTOR);
        break;
    case SENSOR_TYPE_LIGHT:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Light Intensity: %d lx",
                 pkt->sensor_data.values[0]);
        break;
    case SENSOR_TYPE_ENVIRONMENTAL:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Temperature: %.2f °C | Pressure: %.1f hPa",
                 pkt->sensor_data.values[0] / (double)TEMP_SCALING_FACTOR,
                 pkt->sensor_data.values[1] / (double)PRESSURE_SCALING_FACTOR);
        break;
    case SENSOR_TYPE_ACCEL:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Accel: X: %.3f g | Y: %.3f g | Z: %.3f g",
                 pkt->sensor_data.values[0] / (double)ACCEL_SCALING_FACTOR,
                 pkt->sensor_data.values[1] / (double)ACCEL_SCALING_FACTOR,
                 pkt->sensor_data.values[2] / (double)ACCEL_SCALING_FACTOR);
        break;
    case SENSOR_TYPE_GYRO:
        snprintf(sensor_info, sizeof(sensor_info),
                 "Gyro: X: %.3f °/s | Y: %.3f °/s | Z: %.3f °/s",
                 pkt->sensor_data.values[0] / 100.0,  // No dedicated scaling macro provided for gyro
                 pkt->sensor_data.values[1] / 100.0,
                 pkt->sensor_data.values[2] / 100.0);
        break;
    case SENSOR_TYPE_GNSS: {
        int32_t latitude = ((int32_t)pkt->sensor_data.values[1] << 16) |
                           (pkt->sensor_data.values[2] & 0xFFFF);
        int32_t longitude = ((int32_t)pkt->sensor_data.values[3] << 16) |
                            (pkt->sensor_data.values[4] & 0xFFFF);
        int32_t altitude = ((int32_t)pkt->sensor_data.values[5] << 16) |
                           (pkt->sensor_data.values[6] & 0xFFFF);
        snprintf(sensor_info, sizeof(sensor_info),
                 "GNSS Fix Type: %d | Lat: %.7f° | Lon: %.7f° | Alt: %.3f m",
                 pkt->sensor_data.values[0],
                 latitude / (double)SCALING_FACTOR,
                 longitude / (double)SCALING_FACTOR,
                 altitude / 1000.0);
        break;
    }

    case SENSOR_TYPE_MOTION:
        snprintf(sensor_info, sizeof(sensor_info),
                "Motion: %s | Posture: %s",
                pkt->sensor_data.values[0] ? "MOVING" : "STILL",
                pkt->sensor_data.values[1] ? "STANDING" : "NON_STANDING");
        break;

    default:
        snprintf(sensor_info, sizeof(sensor_info), "Unknown sensor type");
        break;
    }

    LOG_INF("%s | %u | %s", addr_str, pkt->timestamp, sensor_info);
}
static bool parse_adv_data(struct bt_data *data, void *user_data)
{
    sensor_packet_t *parsed = (sensor_packet_t *)user_data;
    if (data->type == BT_DATA_MANUFACTURER_DATA && data->data_len >= 8) {
        uint16_t company_id = data->data[0] | (data->data[1] << 8);
        if (company_id != COMPANY_ID) return true;

        memcpy(&parsed->sensor_data, data->data, sizeof(sensor_data_t));
    }
    return true;
}

static void scan_filter_match(struct bt_scan_device_info *info,
    struct bt_scan_filter_match *match, bool connectable)
{
    if (!sensor_handler || !match->manufacturer_data.match) return;

    sensor_packet_t parsed = {0};
    bt_addr_le_copy(&parsed.addr, info->recv_info->addr);
    parsed.timestamp = k_uptime_get();

    bt_data_parse(info->adv_data, parse_adv_data, &parsed);

    if (parsed.sensor_data.type != SENSOR_TYPE_ERROR) {
        sensor_handler(&parsed);
    }
}

uint8_t scanning_state = 1;

int sensor_scanner_stop(void) {
    int err = bt_scan_stop();
    if (err == 0) {
        scanning_state = 0;
        LOG_INF("Scanning stopped");
    } else {
        LOG_ERR("Failed to stop scanning (err %d)", err);
    }
    return err;
}

int sensor_scanner_start(void) {
    int err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err == 0) {
        scanning_state = 1;
        LOG_INF("Scanning started");
    } else {
        LOG_ERR("Failed to start scanning (err %d)", err);
    }
    return err;
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

static struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_ACTIVE,
#ifndef CONFIG_ACCEPT_LIST
    .options = BT_LE_SCAN_OPT_NONE,
#else
    .options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,
#endif
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_WINDOW,
};

static int scan_init(void)
{
    struct bt_scan_init_param init = {
        .connect_if_match = 0,
        .scan_param = &scan_param,
    };
    bt_scan_init(&init);
    bt_scan_cb_register(&scan_cb);

    uint8_t filter[] = { COMPANY_ID & 0xFF, COMPANY_ID >> 8 };
    struct bt_scan_manufacturer_data mdata = {
        .data = filter,
        .data_len = 2
    };

    int err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_MANUFACTURER_DATA, &mdata);
    if (!err) err = bt_scan_filter_enable(BT_SCAN_MANUFACTURER_DATA_FILTER, false);
    return err;
}

int sensor_scanner_init(sensor_packet_handler_t handler)
{
    sensor_handler = handler;

    LOG_INF("Bluetooth initialized");
    int err = scan_init();
    if (err) return err;

    return sensor_scanner_start();
}
