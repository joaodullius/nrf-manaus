#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include "sensor_scanner.h"
#include "concentrator_periph.h"
#include "worker_shadow_service.h"
#include "sensor_common.h"

#ifdef CONFIG_ACCEPT_LIST
#include "accept_list_service.h"
#endif


LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

K_FIFO_DEFINE(sensor_data_fifo);

void sensor_data_handler(const sensor_packet_t *parsed)
{
    sensor_packet_t *pkt = k_malloc(sizeof(sensor_packet_t));
    if (!pkt) {
        LOG_ERR("Out of memory. Attempting to purge FIFO...");
        int dropped = 0;
        while ((pkt = k_fifo_get(&sensor_data_fifo, K_NO_WAIT)) != NULL && dropped < 5) {
            k_free(pkt);
            dropped++;
        }

        pkt = k_malloc(sizeof(sensor_packet_t));
        if (!pkt) {
            LOG_ERR("Still out of memory after purging. Dropping new packet.");
            return;
        }

        LOG_WRN("Recovered after purging %d old packets", dropped);
    }

    #ifdef CONFIG_SENSOR_SCANNER_DUPLICATE_FILTER
    if (!sensor_scanner_is_new_data(parsed)) {
        k_free(pkt);
        return; // Duplicate data, free the packet and return
    }
    #endif

    bt_addr_le_copy(&pkt->addr, &parsed->addr);
    pkt->timestamp = parsed->timestamp;
    pkt->sensor_data = parsed->sensor_data;
    k_fifo_put(&sensor_data_fifo, pkt);
}

void sensor_data_worker(void *a, void *b, void *c)
{
    concentrator_shadow_t shadow = {0};

    while (1) {
        sensor_packet_t *pkt = k_fifo_get(&sensor_data_fifo, K_FOREVER);
        if (!pkt) continue;

        //Put local timestamp in Shadow
        shadow.concentrator_timestamp = k_uptime_get_32();

        switch (pkt->sensor_data.type) {
            case SENSOR_TYPE_LIGHT:
                shadow.light = pkt->sensor_data.values[0];
                LOG_DBG("Light Intensity: %d lx", shadow.light);
                break;

            case SENSOR_TYPE_TEMP:
                shadow.temperature = pkt->sensor_data.values[0];
                LOG_DBG("Temperature: %.2f °C",
                        shadow.temperature / (double)TEMP_SCALING_FACTOR);
                break;

            case SENSOR_TYPE_PRESSURE:
                shadow.pressure = pkt->sensor_data.values[0];
                LOG_DBG("Pressure: %.1f hPa",
                        shadow.pressure / (double)PRESSURE_SCALING_FACTOR);
                break;

            case SENSOR_TYPE_ENVIRONMENTAL:
                shadow.temperature = pkt->sensor_data.values[0];
                shadow.pressure = pkt->sensor_data.values[1];
                LOG_DBG("Environmental: %.2f °C | %.1f hPa",
                        shadow.temperature / (double)TEMP_SCALING_FACTOR,
                        shadow.pressure / (double)PRESSURE_SCALING_FACTOR);
                break;

            case SENSOR_TYPE_GNSS: {
                shadow.fix_type = pkt->sensor_data.values[0];
                shadow.latitude = ((int32_t)pkt->sensor_data.values[1] << 16) |
                                  (pkt->sensor_data.values[2] & 0xFFFF);
                shadow.longitude = ((int32_t)pkt->sensor_data.values[3] << 16) |
                                   (pkt->sensor_data.values[4] & 0xFFFF);
                LOG_DBG("GNSS: Fix Type %d | Lat: %.7f° | Lon: %.7f°",
                        shadow.fix_type,
                        shadow.latitude / (double)SCALING_FACTOR,
                        shadow.longitude / (double)SCALING_FACTOR);
                break;
            }

            case SENSOR_TYPE_MOTION:
                shadow.movement = pkt->sensor_data.values[0];
                shadow.posture = pkt->sensor_data.values[1];
                LOG_DBG("Motion: %s | Posture: %s",
                        shadow.movement ? "MOVING" : "STILL",
                        shadow.posture ? "STANDING" : "NON_STANDING");
                break;

            default:
                LOG_DBG("Unknown sensor type: %d", pkt->sensor_data.type);
                break;
        }

        send_worker_shadow_notification(shadow);
        k_free(pkt);
    }
}



K_THREAD_DEFINE(worker_tid, 1024, sensor_data_worker, NULL, NULL, NULL, 5, 0, 0);

extern bool worker_shadow_notify_enabled;

int main(void)
{

#ifdef CONFIG_ACCEPT_LIST
    // Initialize the accept list service
    if(accept_list_service_init() != 0) {
        LOG_ERR("Accept list service init failed");
        return -1;
    }
#endif // CONFIG_ACCEPT_LIST

    concentrator_periph_init();
    
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
    }

    settings_load();
    concentrator_periph_bt_ready(err);

    LOG_INF("Sensor Concentrator Starting...");
    if (sensor_scanner_init(sensor_data_handler) != 0) {
        LOG_ERR("Failed to initialize scanner");
        return -1;
    }
    return 0;
}
