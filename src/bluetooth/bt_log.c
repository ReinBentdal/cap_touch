#include <errno.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include "bluetooth/bt_connection_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt_log, 3);

static void _ccc_cfg_changed_bt(const struct bt_gatt_attr* attr, uint16_t value);

// custom UUID
#define BT_UUID_BLE_LOG_VAL \
    BT_UUID_128_ENCODE(0x03b80e5a, 0xffff, 0x4b33, 0xa751, 0x6ce34ec4c700)
#define BT_UUID_BLE_LOG_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_BLE_LOG_VAL)

#define BT_UUID_BLE_LOG_IO_VAL \
    BT_UUID_128_ENCODE(0x03b80e5a, 0xffff, 0xffff, 0xa751, 0x6ce34ec4c700)
#define BT_UUID_BLE_LOG_CHARACTERISTIC \
    BT_UUID_DECLARE_128(BT_UUID_BLE_LOG_IO_VAL)

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