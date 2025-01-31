#include <furi-hal-version.h>

#include <furi.h>
#include <stm32wbxx.h>
#include <stm32wbxx_ll_rtc.h>

#include <stdio.h>
#include "ble.h"

#define FURI_HAL_VERSION_OTP_HEADER_MAGIC 0xBABE
#define FURI_HAL_VERSION_NAME_LENGTH 8
#define FURI_HAL_VERSION_ARRAY_NAME_LENGTH (FURI_HAL_VERSION_NAME_LENGTH + 1)
/** BLE symbol + "Flipper " + name */
#define FURI_HAL_VERSION_DEVICE_NAME_LENGTH (1 + 8 + FURI_HAL_VERSION_ARRAY_NAME_LENGTH)
#define FURI_HAL_VERSION_OTP_ADDRESS OTP_AREA_BASE

/** OTP Versions enum */
typedef enum {
    FuriHalVersionOtpVersion0=0x00,
    FuriHalVersionOtpVersion1=0x01,
    FuriHalVersionOtpVersionEmpty=0xFFFFFFFE,
    FuriHalVersionOtpVersionUnknown=0xFFFFFFFF,
} FuriHalVersionOtpVersion;

/** OTP V0 Structure: prototypes and early EVT */
typedef struct {
    uint8_t board_version;
    uint8_t board_target;
    uint8_t board_body;
    uint8_t board_connect;
    uint32_t header_timestamp;
    char name[FURI_HAL_VERSION_NAME_LENGTH];
} FuriHalVersionOTPv0;

/** OTP V1 Structure: late EVT, DVT, PVT, Production */
typedef struct {
    /* First 64 bits: header */
    uint16_t header_magic;
    uint8_t header_version;
    uint8_t header_reserved;
    uint32_t header_timestamp;

    /* Second 64 bits: board info */
    uint8_t board_version; /** Board version */
    uint8_t board_target; /** Board target firmware */
    uint8_t board_body; /** Board body */
    uint8_t board_connect; /** Board interconnect */
    uint8_t board_color; /** Board color */
    uint8_t board_region; /** Board region */
    uint16_t board_reserved; /** Reserved for future use, 0x0000 */

    /* Third 64 bits: Unique Device Name */
    char name[FURI_HAL_VERSION_NAME_LENGTH]; /** Unique Device Name */
} FuriHalVersionOTPv1;

/** Represenation Model: */
typedef struct {
    FuriHalVersionOtpVersion otp_version;

    uint32_t timestamp;

    uint8_t board_version; /** Board version */
    uint8_t board_target; /** Board target firmware */
    uint8_t board_body; /** Board body */
    uint8_t board_connect; /** Board interconnect */
    uint8_t board_color; /** Board color */
    uint8_t board_region; /** Board region */

    char name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH]; /** \0 terminated name */
    char device_name[FURI_HAL_VERSION_DEVICE_NAME_LENGTH];  /** device name for special needs */
    uint8_t ble_mac[6];
} FuriHalVersion;

static FuriHalVersion furi_hal_version = {0};

static FuriHalVersionOtpVersion furi_hal_version_get_otp_version() {
    if (*(uint64_t*)FURI_HAL_VERSION_OTP_ADDRESS == 0xFFFFFFFF) {
        return FuriHalVersionOtpVersionEmpty;
    } else {
        if (((FuriHalVersionOTPv1*)FURI_HAL_VERSION_OTP_ADDRESS)->header_magic == FURI_HAL_VERSION_OTP_HEADER_MAGIC) {
            return FuriHalVersionOtpVersion1;
        } else if (((FuriHalVersionOTPv0*)FURI_HAL_VERSION_OTP_ADDRESS)->board_version <= 10) {
            return FuriHalVersionOtpVersion0;
        } else {
            return FuriHalVersionOtpVersionUnknown;
        }
    }
}

static void furi_hal_version_set_name(const char* name) {
    if(name != NULL) {
        strlcpy(furi_hal_version.name, name, FURI_HAL_VERSION_ARRAY_NAME_LENGTH);
        snprintf(
            furi_hal_version.device_name,
            FURI_HAL_VERSION_DEVICE_NAME_LENGTH,
            "xFlipper %s",
            furi_hal_version.name);
    } else {
        snprintf(
            furi_hal_version.device_name,
            FURI_HAL_VERSION_DEVICE_NAME_LENGTH,
            "xFlipper");
    }

    furi_hal_version.device_name[0] = AD_TYPE_COMPLETE_LOCAL_NAME;

    // BLE Mac address
    uint32_t udn = LL_FLASH_GetUDN();
    uint32_t company_id = LL_FLASH_GetSTCompanyID();
    uint32_t device_id = LL_FLASH_GetDeviceID();
    furi_hal_version.ble_mac[0] = (uint8_t)(udn & 0x000000FF);
    furi_hal_version.ble_mac[1] = (uint8_t)( (udn & 0x0000FF00) >> 8 );
    furi_hal_version.ble_mac[2] = (uint8_t)( (udn & 0x00FF0000) >> 16 );
    furi_hal_version.ble_mac[3] = (uint8_t)device_id;
    furi_hal_version.ble_mac[4] = (uint8_t)(company_id & 0x000000FF);
    furi_hal_version.ble_mac[5] = (uint8_t)( (company_id & 0x0000FF00) >> 8 );
}

static void furi_hal_version_load_otp_default() {
    furi_hal_version_set_name(NULL);
}

static void furi_hal_version_load_otp_v0() {
    const FuriHalVersionOTPv0* otp = (FuriHalVersionOTPv0*)FURI_HAL_VERSION_OTP_ADDRESS;

    furi_hal_version.timestamp = otp->header_timestamp;
    furi_hal_version.board_version = otp->board_version;
    furi_hal_version.board_target = otp->board_target;
    furi_hal_version.board_body = otp->board_body;
    furi_hal_version.board_connect = otp->board_connect;
    furi_hal_version.board_color = 0;
    furi_hal_version.board_region = 0;

    furi_hal_version_set_name(otp->name);
}

static void furi_hal_version_load_otp_v1() {
    const FuriHalVersionOTPv1* otp = (FuriHalVersionOTPv1*)FURI_HAL_VERSION_OTP_ADDRESS;

    furi_hal_version.timestamp = otp->header_timestamp;
    furi_hal_version.board_version = otp->board_version;
    furi_hal_version.board_target = otp->board_target;
    furi_hal_version.board_body = otp->board_body;
    furi_hal_version.board_connect = otp->board_connect;
    furi_hal_version.board_color = otp->board_color;
    furi_hal_version.board_region = otp->board_region;

    furi_hal_version_set_name(otp->name);
}

void furi_hal_version_init() {
    furi_hal_version.otp_version = furi_hal_version_get_otp_version();
    switch(furi_hal_version.otp_version) {
        case FuriHalVersionOtpVersionUnknown:
            furi_hal_version_load_otp_default();
        break;
        case FuriHalVersionOtpVersionEmpty:
            furi_hal_version_load_otp_default();
        break;
        case FuriHalVersionOtpVersion0:
            furi_hal_version_load_otp_v0();
        break;
        case FuriHalVersionOtpVersion1:
            furi_hal_version_load_otp_v1();
        break;
        default: furi_check(0);
    }
    FURI_LOG_I("FuriHalVersion", "Init OK");
}

bool furi_hal_version_do_i_belong_here() {
    return furi_hal_version_get_hw_target() == 6;
}

const char* furi_hal_version_get_model_name() {
    return "Flipper Zero";
}

const uint8_t furi_hal_version_get_hw_version() {
    return furi_hal_version.board_version;
}

const uint8_t furi_hal_version_get_hw_target() {
    return furi_hal_version.board_target;
}

const uint8_t furi_hal_version_get_hw_body() {
    return furi_hal_version.board_body;
}

const FuriHalVersionColor furi_hal_version_get_hw_color() {
    return furi_hal_version.board_color;
}

const uint8_t furi_hal_version_get_hw_connect() {
    return furi_hal_version.board_connect;
}

const FuriHalVersionRegion furi_hal_version_get_hw_region() {
    return furi_hal_version.board_region;
}

const uint32_t furi_hal_version_get_hw_timestamp() {
    return furi_hal_version.timestamp;
}

const char* furi_hal_version_get_name_ptr() {
    return *furi_hal_version.name == 0x00 ? NULL : furi_hal_version.name;
}

const char* furi_hal_version_get_device_name_ptr() {
    return furi_hal_version.device_name + 1;
}

const char* furi_hal_version_get_ble_local_device_name_ptr() {
    return furi_hal_version.device_name;
}

const uint8_t* furi_hal_version_get_ble_mac() {
    return furi_hal_version.ble_mac;
}

const struct Version* furi_hal_version_get_firmware_version(void) {
    return version_get();
}

const struct Version* furi_hal_version_get_boot_version(void) {
#ifdef NO_BOOTLOADER
    return 0;
#else
    /* Backup register which points to structure in flash memory */
    return (const struct Version*)LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR1);
#endif
}

size_t furi_hal_version_uid_size() {
    return 64/8;
}

const uint8_t* furi_hal_version_uid() {
    return (const uint8_t *)UID64_BASE;
}
