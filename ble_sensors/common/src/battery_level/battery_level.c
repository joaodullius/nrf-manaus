#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/bluetooth/services/bas.h>
#include "battery_level.h"

LOG_MODULE_REGISTER(battery_level, CONFIG_BATTERY_LEVEL_LOG_LEVEL);

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));


int16_t buf;
struct adc_sequence sequence = {
    .buffer = &buf,
    /* buffer size in bytes, not number of samples */
    .buffer_size = sizeof(buf),
    //Optional
    //.calibrate = true,
};

uint32_t count = 0;

int battery_level_init(void)
{
    int err;

    /* STEP 3.3 - validate that the ADC peripheral (SAADC) is ready */
    if (!adc_is_ready_dt(&adc_channel)) {
        LOG_ERR("ADC controller devivce %s not ready", adc_channel.dev->name);
        return -1;
    }
    /* STEP 3.4 - Setup the ADC channel */
    err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        LOG_ERR("Could not setup channel #%d (%d)", 0, err);
        return -1;
    }
    /* STEP 4.2 - Initialize the ADC sequence */
    err = adc_sequence_init_dt(&adc_channel, &sequence);
    if (err < 0) {
        LOG_ERR("Could not initalize sequnce");
        return -1;
    }

    return 0;
}

#define BATTERY_FULL_MV 3300

static uint8_t convert_voltage_to_percentage(int val_mv)
{
    if (val_mv <= 0) {
        return 0;
    }
    if (val_mv >= BATTERY_FULL_MV) {
        return 100;
    }
    return (uint8_t)((val_mv * 100) / BATTERY_FULL_MV);
}

int battery_level_get(void)
{
    int val_mv;
    int err;
    static uint32_t count = 0;

    err = adc_read(adc_channel.dev, &sequence);
    if (err < 0) {
        LOG_ERR("Could not read ADC (%d)", err);
        return err;
    }

    /* Get raw ADC reading */
    val_mv = (int)buf;
    LOG_DBG("ADC reading[%u]: %s, channel %d: Raw: %d", count++, adc_channel.dev->name,
            adc_channel.channel_id, val_mv);

    /* Convert raw ADC value to millivolts */
    err = adc_raw_to_millivolts_dt(&adc_channel, &val_mv);
    if (err < 0) {
        LOG_WRN("Value in mV not available (%d)", err);
    } else {
        LOG_DBG("Converted to: %d mV", val_mv);
    }

    /* Convert the millivolt reading to a percentage */
    uint8_t battery_percentage = convert_voltage_to_percentage(val_mv);
    LOG_DBG("Battery Level: %u%% - %d - %xh", battery_percentage, battery_percentage, battery_percentage);

    return battery_percentage;
}

static int battery_level_thread(struct k_work *work)
{
    if (battery_level_init() != 0) {
        LOG_ERR("Failed to Init Battery Level");
        return -1;
    }
    while(1) {
     
        int ret = battery_level_get();
        if (ret >= 0) {
            bt_bas_set_battery_level((uint8_t)ret);
        } else {
            LOG_ERR("Battery level error: %d", ret);
        }
        k_sleep(K_SECONDS(60));
    }     
}
K_THREAD_DEFINE(battery_level_thread_id, 1024, battery_level_thread, NULL, NULL, NULL, K_LOWEST_THREAD_PRIO, 0, 0);