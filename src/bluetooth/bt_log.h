#pragma once

#include <stddef.h>
#include <stdint.h>

// custom UUID
#define BT_UUID_BLE_LOG_VAL \
    BT_UUID_128_ENCODE(0x03b80e5a, 0xffff, 0x4b33, 0xa751, 0x6ce34ec4c700)
#define BT_UUID_BLE_LOG_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_BLE_LOG_VAL)

#define BT_UUID_BLE_LOG_IO_VAL \
    BT_UUID_128_ENCODE(0x03b80e5a, 0xffff, 0xffff, 0xa751, 0x6ce34ec4c700)
#define BT_UUID_BLE_LOG_CHARACTERISTIC \
    BT_UUID_DECLARE_128(BT_UUID_BLE_LOG_IO_VAL)

int bt_log_notify(uint8_t* buf, size_t size);