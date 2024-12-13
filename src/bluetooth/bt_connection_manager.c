#include "bt_connection_manager.h"

#include <stdlib.h>
#include <errno.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>

#include "bt_log.h"
#include "utils/macros_common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bt, 3);

static const struct bt_data _ble_advertisement_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, (sizeof(CONFIG_BT_DEVICE_NAME) - 1)),
};

static const struct bt_data _ble_scan_response_data[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_BLE_LOG_VAL), // HID UUID: 0x1812 in little-endian format
};


static const struct bt_le_adv_param* _advertisement_params_fast = BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE, BT_GAP_ADV_FAST_INT_MIN_2, BT_GAP_ADV_FAST_INT_MAX_2, NULL);

static const struct bt_le_conn_param _connection_params_fast = BT_LE_CONN_PARAM_INIT(
	CONFIG_BT_PERIPHERAL_PREF_MIN_INT,
	CONFIG_BT_PERIPHERAL_PREF_MAX_INT,
	CONFIG_BT_PERIPHERAL_PREF_LATENCY, 
	CONFIG_BT_PERIPHERAL_PREF_TIMEOUT
);

static bt_state_change_cb _bt_state_change_cb;

static struct bt_conn* _ble_conn;

static void _advertisement_start(void);
static void _connection_negotiate(void);
static void _bt_ready(int err);
static void _ble_connected_cb(struct bt_conn* conn, uint8_t conn_err);
static void _ble_disconnected_cb(struct bt_conn *conn, uint8_t reason);
static bool _ble_param_request(struct bt_conn *conn, struct bt_le_conn_param *param);
static void _ble_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout);
static struct bt_conn_cb _ble_connection_cb = {
	.connected = _ble_connected_cb,
	.disconnected = _ble_disconnected_cb,
	.le_param_req = _ble_param_request,
	.le_param_updated = _ble_param_updated,
};

int bt_connection_init(bt_state_change_cb bt_state_change_cb) {
	_bt_state_change_cb = bt_state_change_cb;

	bt_conn_cb_register(&_ble_connection_cb);

	int ret = bt_enable(_bt_ready);
	RETURN_ON_ERR(ret);

  	return 0;
}

struct bt_conn* bt_connection_get(void) { return _ble_conn; }

static void _bt_ready(int err) {
	LOG_INF("Bluetooth ready cb");
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}
	_advertisement_start();
}

static void _advertisement_start(void) {
	int ret = bt_le_adv_start(_advertisement_params_fast, _ble_advertisement_data, ARRAY_SIZE(_ble_advertisement_data), _ble_scan_response_data, ARRAY_SIZE(_ble_scan_response_data));
  	LOG_ERR_IF(ret, "failed to start advertising");
}

static void _connection_negotiate(void) {
	__ASSERT_NO_MSG(_ble_conn != NULL);
	LOG_INF("Sending connection parameters interval %d:%d, latency %d, timeout %d", _connection_params_fast.interval_min, _connection_params_fast.interval_max, _connection_params_fast.latency, _connection_params_fast.timeout);
	int ret = bt_conn_le_param_update(_ble_conn, &_connection_params_fast);
	LOG_WRN_IF(ret, "Connection parameter update failed (err %d)", ret);
}

static void _ble_connected_cb(struct bt_conn* conn, uint8_t conn_err) {

	if (conn_err) {
		LOG_ERR("Connection failed (err %u)", conn_err);
		return;
	}

	char addr[BT_ADDR_LE_STR_LEN];
	(void)bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Connected to %s", addr);

	// _ble_conn = conn;
	_ble_conn = bt_conn_ref(conn);
	RETURN_ON_ERR_MSG(_ble_conn == NULL, "Failed to ref connection");

	_connection_negotiate();

	if (_bt_state_change_cb) {
		_bt_state_change_cb(BT_CONNECTED);
	}
}

static void _ble_disconnected_cb(struct bt_conn *conn, uint8_t reason) {
	char addr[BT_ADDR_LE_STR_LEN];

	(void)bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (_ble_conn) {
		bt_conn_unref(_ble_conn);
		_ble_conn = NULL;
	}

	_advertisement_start();

	if (_bt_state_change_cb) {
		_bt_state_change_cb(BT_DISCONNECTED);
	}
}

static bool _ble_param_request(struct bt_conn *conn, struct bt_le_conn_param *param) {
	/* do not accept ble connection interval larger than specified */
	if ( (param->interval_max > CONFIG_BT_PERIPHERAL_PREF_MAX_INT)) {
		LOG_INF("Connection parameter rejected");
		return false;
	}

	LOG_INF("Connection parameter accepted: I %u-%u, L %u, T %u", param->interval_min, param->interval_max, param->latency, param->timeout);
	return true;
}

static void _ble_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency, uint16_t timeout) {
	LOG_INF("Connection parameters updated: I %u, L %u, T %u", interval, latency, timeout);

	if (interval > _connection_params_fast.interval_max) {
		int err = bt_conn_le_param_update(conn, &_connection_params_fast);
		if (err) {
			LOG_WRN("Connection parameter update failed (err %d)", err);
		} else {
			LOG_INF("Connection parameter update requested with interval %u - %u", _connection_params_fast.interval_min, _connection_params_fast.interval_max);
		}
	} else {
		LOG_INF("Connection parameter accepted: I %u, L %u, T %u", interval, latency, timeout);
	}
}