#ifndef BATTERY_LEVEL_H
#define BATTERY_LEVEL_H

/**
 * @brief Get the battery voltage in millivolts.
 *
 * This function reads the internal voltage using the ADC and converts it
 * to battery voltage using the nRF52 series formula:
 * battery_voltage (mV) = ((600 * 6) * sample) / (1 << 12)
 *
 * @return int Battery voltage in mV on success, or a negative error code on failure.
 */
int battery_level_get(void);
int battery_level_init(void);

#endif /* BATTERY_LEVEL_H */
