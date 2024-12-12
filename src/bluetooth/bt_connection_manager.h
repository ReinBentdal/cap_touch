#ifndef _BT_CONNECTION_H_
#define _BT_CONNECTION_H_

#include <stdbool.h>
#include <zephyr/bluetooth/conn.h>

enum bt_connection_state {BT_CONNECTED, BT_DISCONNECTED};

typedef void (*bt_state_change_cb)(enum bt_connection_state);

int bt_connection_init(bt_state_change_cb state_change_cb);
struct bt_conn* bt_connection_get(void);

#endif