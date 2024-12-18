/*
 * File: bt_log.h
 * Author: Rein Gundersen Bentdal
 * Created: 19.Des 2024
 *
 * Copyright (c) 2024, Rein Gundersen Bentdal
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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