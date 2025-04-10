#include "bt_simple_service_client.h"

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME simple_service
LOG_MODULE_REGISTER(LOG_MODULE_NAME, LOG_LEVEL_INF);

enum {
	SIMPLE_SERVICE_C_INITIALIZED,
	SIMPLE_SERVICE_C_BUTTOM_NOTIF_ENABLED,
	SIMPLE_SERVICE_C_RX_WRITE_PENDING
};

int bt_simple_service_client_init(struct bt_simple_service *simple_service_c,
		       const struct bt_simple_service_client_init_param *simple_service_c_init)
{
	LOG_DBG("bt_simple_service_client_init called");
	if (!simple_service_c || !simple_service_c_init) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&simple_service_c->state, SIMPLE_SERVICE_C_INITIALIZED)) {
		return -EALREADY;
	}

	memcpy(&simple_service_c->cb, &simple_service_c_init->cb, sizeof(simple_service_c->cb));

	return 0;
}

int bt_simple_service_handles_assign(struct bt_gatt_dm *dm,
			  struct bt_simple_service *simple_service_c)

{
	const struct bt_gatt_dm_attr *gatt_service_attr = bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service = bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;


	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_DECLARE_128(WORKER_SHADOW_SERVICE_UUID))) {
		return -ENOTSUP;
	}
	LOG_DBG("Getting handles from Custom  service.");
	memset(&simple_service_c->handles, 0xFF, sizeof(simple_service_c->handles));

	
	/* Get Shadow (READ) Characterstic and CCC*/
	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_DECLARE_128(WORKER_SHADOW_CHAR_UUID));
	if (!gatt_chrc) {
		LOG_ERR("Missing Shadow Read characteristic.");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_DECLARE_128(WORKER_SHADOW_CHAR_UUID));
	if (!gatt_desc) {
		LOG_ERR("Missing Shadow Read value descriptor in characteristic.");
		return -EINVAL;
	}
	LOG_DBG("Found handle for Shadow Read characteristic.");
	simple_service_c->handles.shadow = gatt_desc->handle;
	/* shadow (Read) CCC */
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
	if (!gatt_desc) {
		LOG_ERR("Missing Shadow Read CCC in characteristic.");
		return -EINVAL;
	}
	LOG_DBG("Found handle for CCC of Shadow Read characteristic.");
	simple_service_c->handles.shadow_ccc = gatt_desc->handle;


	/* Assign connection instance. */
	simple_service_c->conn = bt_gatt_dm_conn_get(dm);
	return 0;
}

static uint8_t on_received(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params,
			const void *data, uint16_t length)
{
	struct bt_simple_service *simple_service;

	/* Retrieve Client module context. */
	simple_service = CONTAINER_OF(params, struct bt_simple_service, shadow_notif_params);

	if (!data) {
		LOG_DBG("[UNSUBSCRIBED]");
		params->value_handle = 0;
		atomic_clear_bit(&simple_service->state, SIMPLE_SERVICE_C_BUTTOM_NOTIF_ENABLED);
		if (simple_service->cb.unsubscribed) {
			simple_service->cb.unsubscribed(simple_service);
		}
		return BT_GATT_ITER_STOP;
	}

	uint8_t value = *((uint8_t*)data);
	LOG_DBG("[NOTIFICATION] data %p length %u value %d", data, length, value);

	if (simple_service->cb.received) {
		return simple_service->cb.received(simple_service, data, length);
	}

	return BT_GATT_ITER_CONTINUE;
}

int bt_simple_service_subscribe_receive(struct bt_simple_service *simple_service_c)
{
	int err;

	if (atomic_test_and_set_bit(&simple_service_c->state, SIMPLE_SERVICE_C_BUTTOM_NOTIF_ENABLED)) {
		return -EALREADY;
	}

	simple_service_c->shadow_notif_params.notify = on_received;
	simple_service_c->shadow_notif_params.value = BT_GATT_CCC_NOTIFY;
	simple_service_c->shadow_notif_params.value_handle = simple_service_c->handles.shadow;
	simple_service_c->shadow_notif_params.ccc_handle = simple_service_c->handles.shadow_ccc;
	atomic_set_bit(simple_service_c->shadow_notif_params.flags,
		       BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	err = bt_gatt_subscribe(simple_service_c->conn, &simple_service_c->shadow_notif_params);
	if (err) {
		LOG_ERR("Subscribe failed (err %d)", err);
		atomic_clear_bit(&simple_service_c->state, SIMPLE_SERVICE_C_BUTTOM_NOTIF_ENABLED);
	} else {
		LOG_DBG("[SUBSCRIBED]");
	}

	return err;
}

static void on_sent(struct bt_conn *conn, uint8_t err,
                     struct bt_gatt_write_params *params)
{

	struct bt_simple_service *simple_service_c;
    const void *data;
	uint16_t length;

	/* Retrieve module context. */
	simple_service_c = CONTAINER_OF(params, struct bt_simple_service, write_params);

    /* Make a copy of volatile data that is required by the callback. */
	data = params->data;
	length = params->length;

	atomic_clear_bit(&simple_service_c->state, SIMPLE_SERVICE_C_RX_WRITE_PENDING);
	if (simple_service_c->cb.sent) {
		simple_service_c->cb.sent(simple_service_c, err, data, length);
	}
	
}

