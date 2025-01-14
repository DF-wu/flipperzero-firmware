#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_SVC_DATA_LEN_MAX 255

#define SERIAL_SVC_UUID_128 0x00, 0x00, 0xfe, 0x60, 0xcc, 0x7a, 0x48, 0x2a, 0x98, 0x4a, 0x7f, 0x2e, 0xd5, 0xb3, 0xe5, 0x8f
#define SERIAL_CHAR_RX_UUID_128 0x00, 0x00, 0xfe, 0x61, 0x8e, 0x22, 0x45, 0x41, 0x9d, 0x4c, 0x21, 0xed, 0xae, 0x82, 0xed, 0x19
#define SERIAL_CHAR_TX_UUID_128 0x00, 0x00, 0xfe, 0x62, 0x8e, 0x22, 0x45, 0x41, 0x9d, 0x4c, 0x21, 0xed, 0xae, 0x82, 0xed, 0x19

bool serial_svc_init();

bool serial_svc_update_rx(uint8_t* data, uint8_t data_len);

#ifdef __cplusplus
}
#endif
