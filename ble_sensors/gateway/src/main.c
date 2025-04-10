#include <zephyr/kernel.h>
#include "gateway_ble.h"
#include "gateway_lte.h"
#include "bt_simple_service_client.h"
#include <zephyr/logging/log.h>
#include <net/aws_iot.h>
#include <string.h>
#include <stdio.h>


#include <zephyr/sys/reboot.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <hw_id.h>
#include <modem/modem_info.h>

LOG_MODULE_REGISTER(gateway_main, LOG_LEVEL_INF);

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()					\
	LOG_ERR("Fatal error! Rebooting the device.");	\
	LOG_PANIC();					\
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))


/* Application specific topics. */
#define MY_CUSTOM_TOPIC_1 "dk/led0"
#define MY_CUSTOM_TOPIC_2 "my-custom-topic/example_2"

/* Variável global para armazenar o shadow atual e mutex para proteção */
static concentrator_shadow_t current_shadow;
static struct k_mutex shadow_mutex;

/* Declarações antecipadas */
static void shadow_update_work_fn(struct k_work *work);
static void connect_work_fn(struct k_work *work);
static void aws_iot_event_handler(const struct aws_iot_evt *const evt);
static void print_shadow(const concentrator_shadow_t *shadow);

/* Work items */
static K_WORK_DELAYABLE_DEFINE(shadow_update_work, shadow_update_work_fn);
static K_WORK_DELAYABLE_DEFINE(connect_work, connect_work_fn);

/* Hardware ID */
static char hw_id[HW_ID_LEN];

/* Função que atualiza o shadow ao receber uma notificação BLE */
uint8_t concentrator_data_handler(struct bt_simple_service *simple_service,
    const uint8_t *data, uint16_t length)
{
	LOG_DBG("Dados recebidos do cliente: %u bytes", length);
    if (length != sizeof(concentrator_shadow_t)) {
        LOG_ERR("Tamanho dos dados recebidos (%u) incompatível", length);
        return BT_GATT_ITER_CONTINUE;
    }

    /* Atualiza o current_shadow com proteção do mutex */
    k_mutex_lock(&shadow_mutex, K_FOREVER);
    memcpy(&current_shadow, data, sizeof(concentrator_shadow_t));
    k_mutex_unlock(&shadow_mutex);

    LOG_DBG("Shadow atualizado via notificação BLE");
    //print_shadow(&current_shadow);

    return BT_GATT_ITER_CONTINUE;
}

/* Função que prepara o JSON com base no shadow atual e envia via AWS IoT */
static void shadow_update_work_fn(struct k_work *work)
{
    int err;
    char message[CONFIG_AWS_IOT_SAMPLE_JSON_MESSAGE_SIZE_MAX] = { 0 };
    struct aws_iot_data tx_data = {
        .qos = MQTT_QOS_0_AT_MOST_ONCE,
        .topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
    };

    concentrator_shadow_t s_copy;
    /* Cria uma cópia dos dados atuais com proteção do mutex */
    k_mutex_lock(&shadow_mutex, K_FOREVER);
    s_copy = current_shadow;
    k_mutex_unlock(&shadow_mutex);


	snprintf(message, sizeof(message),
	"{\"state\":{\"reported\":{"
	"\"uptime\": %lld, "
	"\"app_version\": \"%s\", "
	"\"concentrator_timestamp\": %u, "
	"\"temperature\": %.2f, "
	"\"pressure\": %.1f, "
	"\"latitude\": %.7f, "
	"\"longitude\": %.7f, "
	"\"fix_type\": %u, "
	"\"movement\": %u, "
	"\"posture\": %u, "
	"\"light\": %d"
	"}}}",
	(long long)k_uptime_get(),
	CONFIG_AWS_IOT_SAMPLE_APP_VERSION,
	s_copy.concentrator_timestamp,
	s_copy.temperature / 100.0,
	s_copy.pressure / 10.0,
	s_copy.latitude / 1e7,
	s_copy.longitude / 1e7,
	s_copy.fix_type,
	s_copy.movement,
	s_copy.posture,
	s_copy.light);



    tx_data.ptr = message;
    tx_data.len = strlen(message);
    LOG_INF("Enviando mensagem: %s para o AWS IoT Shadow", message);

    err = aws_iot_send(&tx_data);
    if (err) {
        LOG_ERR("aws_iot_send falhou, erro: %d", err);
        //FATAL_ERROR();
        return;
    }

    (void)k_work_reschedule(&shadow_update_work,
                K_SECONDS(CONFIG_AWS_IOT_SAMPLE_PUBLICATION_INTERVAL_SECONDS));
}


/* Função auxiliar para exibir o shadow */
static void print_shadow(const concentrator_shadow_t *shadow)
{
    LOG_INF("Notificação recebida:");
    LOG_INF(" Timestamp: %u", shadow->concentrator_timestamp);
    LOG_INF(" Temp: %.2f°C", shadow->temperature / 100.0);
    LOG_INF(" Pressure: %.1f hPa", shadow->pressure / 10.0);
    LOG_INF(" Lat: %.7f", shadow->latitude / 1e7);
    LOG_INF(" Lon: %.7f", shadow->longitude / 1e7);
    LOG_INF(" Fix: %d", shadow->fix_type);
    LOG_INF(" Move: %d | Posture: %d", shadow->movement, shadow->posture);
    LOG_INF(" Light: %d", shadow->light);
    LOG_INF("--------------------------------------------------");
}


static void connect_work_fn(struct k_work *work)
{
	int err;
	const struct aws_iot_config config = {
		.client_id = hw_id,
	};

	LOG_INF("Connecting to AWS IoT");

	err = aws_iot_connect(&config);
	if (err == -EAGAIN) {
		LOG_INF("Connection attempt timed out, "
			"Next connection retry in %d seconds",
			CONFIG_AWS_IOT_SAMPLE_CONNECTION_RETRY_TIMEOUT_SECONDS);

		(void)k_work_reschedule(&connect_work,
				K_SECONDS(CONFIG_AWS_IOT_SAMPLE_CONNECTION_RETRY_TIMEOUT_SECONDS));
	} else if (err) {
		LOG_ERR("aws_iot_connect, error: %d", err);
		FATAL_ERROR();
	}
}

static void on_aws_iot_evt_connected(const struct aws_iot_evt *const evt)
{
	(void)k_work_cancel_delayable(&connect_work);

	/* If persistent session is enabled, the AWS IoT library will not subscribe to any topics.
	 * Topics from the last session will be used.
	 */
	if (evt->data.persistent_session) {
		LOG_WRN("Persistent session is enabled, using subscriptions "
			"from the previous session");
	}

	/* Mark image as working to avoid reverting to the former image after a reboot. */
#if defined(CONFIG_BOOTLOADER_MCUBOOT)
	boot_write_img_confirmed();
#endif

	/* Start sequential updates to AWS IoT. */
	(void)k_work_reschedule(&shadow_update_work, K_NO_WAIT);
}

static void on_aws_iot_evt_disconnected(void)
{
	//(void)k_work_cancel_delayable(&shadow_update_work);
	//(void)k_work_reschedule(&connect_work, K_SECONDS(5));
}

static void on_aws_iot_evt_fota_done(const struct aws_iot_evt *const evt)
{
	int err;

	/* Tear down MQTT connection. */
	(void)aws_iot_disconnect();
	(void)k_work_cancel_delayable(&connect_work);

	/* If modem FOTA has been carried out, the modem needs to be reinitialized.
	 * This is carried out by bringing the network interface down/up.
	 */
	if (evt->data.image & DFU_TARGET_IMAGE_TYPE_ANY_MODEM) {
		LOG_INF("Modem FOTA done, reinitializing the modem");

        // Implement modem reinitialization here
		
	} else if (evt->data.image & DFU_TARGET_IMAGE_TYPE_ANY_APPLICATION) {
		LOG_INF("Application FOTA done, rebooting");
		IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));
	} else {
		LOG_WRN("Unexpected FOTA image type");
	}
}

static void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
	switch (evt->type) {
	case AWS_IOT_EVT_CONNECTING:
		LOG_INF("AWS_IOT_EVT_CONNECTING");
		break;
	case AWS_IOT_EVT_CONNECTED:
		LOG_INF("AWS_IOT_EVT_CONNECTED");
		on_aws_iot_evt_connected(evt);
		break;
	case AWS_IOT_EVT_DISCONNECTED:
		LOG_INF("AWS_IOT_EVT_DISCONNECTED");
		on_aws_iot_evt_disconnected();
		break;
	case AWS_IOT_EVT_DATA_RECEIVED:
		LOG_INF("AWS_IOT_EVT_DATA_RECEIVED");

		LOG_INF("Received message: \"%.*s\" on topic: \"%.*s\"", evt->data.msg.len,
									 evt->data.msg.ptr,
									 evt->data.msg.topic.len,
									 evt->data.msg.topic.str);
		break;
	case AWS_IOT_EVT_PUBACK:
		LOG_INF("AWS_IOT_EVT_PUBACK, message ID: %d", evt->data.message_id);
		break;
	case AWS_IOT_EVT_PINGRESP:
		LOG_INF("AWS_IOT_EVT_PINGRESP");
		break;
	case AWS_IOT_EVT_FOTA_START:
		LOG_INF("AWS_IOT_EVT_FOTA_START");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_PENDING:
		LOG_INF("AWS_IOT_EVT_FOTA_ERASE_PENDING");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_DONE:
		LOG_INF("AWS_FOTA_EVT_ERASE_DONE");
		break;
	case AWS_IOT_EVT_FOTA_DONE:
		LOG_INF("AWS_IOT_EVT_FOTA_DONE");
		on_aws_iot_evt_fota_done(evt);
		break;
	case AWS_IOT_EVT_FOTA_DL_PROGRESS:
		LOG_INF("AWS_IOT_EVT_FOTA_DL_PROGRESS, (%d%%)", evt->data.fota_progress);
		break;
	case AWS_IOT_EVT_ERROR:
		LOG_INF("AWS_IOT_EVT_ERROR, %d", evt->data.err);
		FATAL_ERROR();
		break;
	case AWS_IOT_EVT_FOTA_ERROR:
		LOG_INF("AWS_IOT_EVT_FOTA_ERROR");
		break;
	default:
		LOG_WRN("Unknown AWS IoT event type: %d", evt->type);
		break;
	}
}

static int app_topics_subscribe(void)
{
	int err;
	static const struct mqtt_topic topic_list[] = {
		{
			.topic.utf8 = MY_CUSTOM_TOPIC_1,
			.topic.size = sizeof(MY_CUSTOM_TOPIC_1) - 1,
			.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		},
		{
			.topic.utf8 = MY_CUSTOM_TOPIC_2,
			.topic.size = sizeof(MY_CUSTOM_TOPIC_2) - 1,
			.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		}
	};

	err = aws_iot_application_topics_set(topic_list, ARRAY_SIZE(topic_list));
	if (err) {
		LOG_ERR("aws_iot_application_topics_set, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	return 0;
}

static int aws_iot_client_init(void)
{
	int err;

    LOG_INF("Initializing AWS IoT library");

	err = aws_iot_init(aws_iot_event_handler);
	if (err) {
		LOG_ERR("AWS IoT library could not be initialized, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	// /* Add application specific non-shadow topics to the AWS IoT library.
	//  * These topics will be subscribed to when connecting to the broker.
	//  */
	err = app_topics_subscribe();
	if (err) {
		LOG_ERR("Adding application specific topics failed, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	return 0;
}
int main(void)
{
    int err;

    /* Inicializa o mutex */
    k_mutex_init(&shadow_mutex);

    LOG_INF("Gateway BLE Example");

    err = gateway_lte_init();
    if (err) {
        LOG_ERR("Inicialização LTE falhou: %d", err);
        return 1;
    }

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Inicialização Bluetooth falhou: %d", err);
        return 1;
    }

    err = gateway_ble_init(concentrator_data_handler);
    if (err) {
        LOG_ERR("Inicialização BLE falhou: %d", err);
        return 1;
    }

    while (rrc_state == RRC_DISCONNECTED || rrc_state == RRC_CONNECTING) {
        k_sleep(K_MSEC(500));
    }
    LOG_WRN("Gateway conectado à rede LTE");

    /* A inicialização do cliente AWS IoT pode ser realizada aqui, se necessário */
    /* Exemplo:
     * err = aws_iot_client_init();
     * if (err) { ... }
     * k_work_reschedule(&connect_work, K_SECONDS(5));
     */

	 err = aws_iot_client_init();
	if (err) {
		LOG_ERR("AWS IoT client initialization failed: %d", err);
		return 1;
	}
	
	k_work_reschedule(&connect_work, K_SECONDS(5));
	 		

    return 0;
}
