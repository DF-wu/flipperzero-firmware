#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <m-string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Power IC type */
typedef enum {
    FuriHalPowerICCharger,
    FuriHalPowerICFuelGauge,
} FuriHalPowerIC;

/** Initialize drivers */
void furi_hal_power_init();

/**
 * Get current insomnia level
 * @return insomnia level: 0 - no insomnia, >0 - insomnia, bearer count.
 */
uint16_t furi_hal_power_insomnia_level();

/**
 * Enter insomnia mode
 * Prevents device from going to sleep
 * @warning Internally increases insomnia level
 * Must be paired with furi_hal_power_insomnia_exit
 */
void furi_hal_power_insomnia_enter();

/**
 * Exit insomnia mode
 * Allow device to go to sleep
 * @warning Internally decreases insomnia level.
 * Must be paired with furi_hal_power_insomnia_enter
 */
void furi_hal_power_insomnia_exit();

/** Check if sleep availble */
bool furi_hal_power_sleep_available();

/** Check if deep sleep availble */
bool furi_hal_power_deep_sleep_available();

/** Go to sleep */
void furi_hal_power_sleep();

/** Get predicted remaining battery capacity in percents */
uint8_t furi_hal_power_get_pct();

/** Get battery health state in percents */
uint8_t furi_hal_power_get_bat_health_pct();

/** Get charging status */
bool furi_hal_power_is_charging();

/** Poweroff device */
void furi_hal_power_off();

/** Reset device */
void furi_hal_power_reset();

/** OTG enable */
void furi_hal_power_enable_otg();

/** OTG disable */
void furi_hal_power_disable_otg();

/** Get remaining battery battery capacity in mAh */
uint32_t furi_hal_power_get_battery_remaining_capacity();

/** Get full charge battery capacity in mAh */
uint32_t furi_hal_power_get_battery_full_capacity();

/** Get battery voltage in V */
float furi_hal_power_get_battery_voltage(FuriHalPowerIC ic);

/** Get battery current in A */
float furi_hal_power_get_battery_current(FuriHalPowerIC ic);

/** Get temperature in C */
float furi_hal_power_get_battery_temperature(FuriHalPowerIC ic);

/** Get System voltage in V */
float furi_hal_power_get_system_voltage();

/** Get USB voltage in V */
float furi_hal_power_get_usb_voltage();

/** Get power system component state */
void furi_hal_power_dump_state();

/** Enable 3.3v on external gpio and sd card */
void furi_hal_power_enable_external_3_3v();

/** Disable 3.3v on external gpio and sd card */
void furi_hal_power_disable_external_3_3v();

#ifdef __cplusplus
}
#endif
