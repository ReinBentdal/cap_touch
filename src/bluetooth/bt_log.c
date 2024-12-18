/*
 * File: bt_log.c
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

#include "bt_log.h"

#include <errno.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include "bluetooth/bt_connection_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_log, 3);

static void _ccc_cfg_changed_bt(const struct bt_gatt_attr* attr, uint16_t value);



/* BLE MIDI Service Declaration */
BT_GATT_SERVICE_DEFINE(_bt_log_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_BLE_LOG_SERVICE),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_BLE_LOG_CHARACTERISTIC,
        BT_GATT_CHRC_NOTIFY,  // Only notify property
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, // No permissions required for notification
        NULL, NULL, NULL
    ), // No read or write callback functions
    BT_GATT_CCC(_ccc_cfg_changed_bt, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

int bt_log_notify(uint8_t* buf, size_t size) {
  /* bluetooth transmission should not be enabled if there is no bt connection */
  struct bt_conn* ble_conn = bt_connection_get();
  if (ble_conn == NULL) {
    return -ENOTCONN;
  }

  /* points to the MIDI service */
  const struct bt_gatt_attr* attr = &_bt_log_service.attrs[1];

  if (bt_gatt_is_subscribed(ble_conn, attr, BT_GATT_CCC_NOTIFY)) {
    int ret = bt_gatt_notify(ble_conn, attr, buf, size);
    /* we tell pm service that an event occured, such that device is kept in bt fast interval */
    return ret;
  } else {
    return -EINVAL;
  }
}

static void _ccc_cfg_changed_bt(const struct bt_gatt_attr* attr, uint16_t value) {
  LOG_INF("BT LOG Notification %s", value ? "enabled" : "disabled");
}