/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <net/wifi_mgmt_ext.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <net/mqtt_helper.h>

/* STEP 1.6 - Include the header file for the TLS credentials library */
#include <zephyr/net/tls_credentials.h>

/* STEP 2.3 - Include the certificate */
/*static const unsigned char ca_certificate[] = {
#include "certificate.h"
};*/

/*static const unsigned char ca_certificate[] = {
#include "certificate.h"
};*/

LOG_MODULE_REGISTER(Lesson4_Exercise2, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)


#define CLIENT_ID_LEN  sizeof(CONFIG_BOARD) + 11

/* STEP 3.1 - Define a macro for the credential security tag */
#define MQTT_TLS_SEC_TAG 24

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

static struct sockaddr_storage server;
static struct mqtt_client client;
//static struct pollfd fds;

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				   struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}
	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_INF("Network connected");
		connected = true;
		k_sem_give(&run_app);
		return;
	}
	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_INF("Waiting for network to be connected");
		} else {
			LOG_INF("Network disconnected");
			connected = false;
			(void)mqtt_helper_disconnect();
		}
		k_sem_reset(&run_app);
		return;
	}
}

static int get_received_payload(struct mqtt_client *c, size_t length)
{
	int ret;
	int err = 0;

	if (length > sizeof(payload_buf)) {
		err = -EMSGSIZE;
	}

	/* Truncate payload until it fits in the payload buffer. */
	while (length > sizeof(payload_buf)) {
		ret = mqtt_read_publish_payload_blocking(c, payload_buf,
							 (length - sizeof(payload_buf)));
		if (ret == 0) {
			return -EIO;
		} else if (ret < 0) {
			return ret;
		}

		length -= ret;
	}

	ret = mqtt_readall_publish_payload(c, payload_buf, length);
	if (ret) {
		return ret;
	}

	return err;
}

static void subscribe(void)
{
	int err;

	struct mqtt_topic subscribe_topic = {
		.topic = {.utf8 = CONFIG_MQTT_SUB_TOPIC, .size = strlen(CONFIG_MQTT_SUB_TOPIC)},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE};

	struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = 1234
	};

	LOG_INF("Subscribing to %s", CONFIG_MQTT_SUB_TOPIC);
	err = mqtt_helper_subscribe(&subscription_list);
	if (err) {
		LOG_ERR("Failed to subscribe to topics, error: %d", err);
		return;
	}
	//return mqtt_subscribe(c, &subscription_list);
}

static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	LOG_INF("%s%s", (char *)prefix, (char *)buf);
}

static int publish(struct mqtt_client *c, enum mqtt_qos qos, uint8_t *data, size_t len)
{
	int err;
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
	param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	//data_print("Publishing: ", data, len);
	//LOG_INF("to topic: %s len: %u", CONFIG_MQTT_PUB_TOPIC,
		//(unsigned int)strlen(CONFIG_MQTT_PUB_TOPIC));

	err = mqtt_helper_publish(&param);
	if (err) {
		LOG_WRN("Failed to send payload, err: %d", err);
		return err;
	}

	LOG_INF("Published message: \"%.*s\" on topic: \"%.*s\"", param.message.payload.len,
								  param.message.payload.data,
								  param.message.topic.topic.size,
								  param.message.topic.topic.utf8);
	return 0;
}

/* Callback handlers from MQTT helper library.
 * The functions are called whenever specific MQTT packets are received from the broker, or
 * some library state has changed.
 */
static void on_mqtt_connack(enum mqtt_conn_return_code return_code, bool session_present)
{
	ARG_UNUSED(return_code);
}

static void on_mqtt_disconnect(int result)
{
	ARG_UNUSED(result);
}

static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
	LOG_INF("Received payload: %.*s on topic: %.*s", payload.size,
							 payload.ptr,
							 topic.size,
							 topic.ptr);
}

static void on_mqtt_suback(uint16_t message_id, int result)
{
	if ((message_id == 2469) && (result == 0)) {
		LOG_INF("Subscribed to topic %s", CONFIG_MQTT_SUB_TOPIC);
	} else if (result) {
		LOG_ERR("Topic subscription failed, error: %d", result);
	} else {
		LOG_WRN("Subscribed to unknown topic, id: %d", message_id);
	}
}

static void connected_entry(struct mqtt_client *c)
{
	LOG_INF("Connected to MQTT broker");
	LOG_INF("Hostname: %s", CONFIG_MQTT_BROKER_HOSTNAME);
	LOG_INF("Client ID: %s", c->client_id.utf8);
	LOG_INF("Port: %d", CONFIG_MQTT_BROKER_PORT);
	LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");

	subscribe();
}

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed: %d", evt->result);
			break;
		}

		LOG_INF("MQTT client connected");
		connected_entry(c);
		//subscribe(c);
		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected: %d", evt->result);
		break;

	case MQTT_EVT_PUBLISH:
		const struct mqtt_publish_param *p = &evt->param.publish;
		err = get_received_payload(c, p->message.payload.len);
		if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
			const struct mqtt_puback_param ack = {.message_id = p->message_id};
			mqtt_publish_qos1_ack(c, &ack);
		}

		if (err >= 0) {
			data_print("Received: ", payload_buf, p->message.payload.len);
			if (strncmp(payload_buf, CONFIG_LED1_ON_CMD,
				    sizeof(CONFIG_LED1_ON_CMD) - 1) == 0) {
				dk_set_led_on(DK_LED1);
			} else if (strncmp(payload_buf, CONFIG_LED1_OFF_CMD,
					   sizeof(CONFIG_LED1_OFF_CMD) - 1) == 0) {
				dk_set_led_off(DK_LED1);
			} else if (strncmp(payload_buf, CONFIG_LED2_ON_CMD,
					   sizeof(CONFIG_LED2_ON_CMD) - 1) == 0) {
				dk_set_led_on(DK_LED2);
			} else if (strncmp(payload_buf, CONFIG_LED2_OFF_CMD,
					   sizeof(CONFIG_LED2_OFF_CMD) - 1) == 0) {
				dk_set_led_off(DK_LED2);
			}

		} else if (err == -EMSGSIZE) {
			LOG_ERR("Received payload (%d bytes) is larger than the payload buffer "
				"size (%d bytes).",
				p->message.payload.len, sizeof(payload_buf));
		} else {
			LOG_ERR("get_received_payload failed: %d", err);
			LOG_INF("Disconnecting MQTT client...");

			err = mqtt_disconnect(c);
			if (err) {
				LOG_ERR("Could not disconnect: %d", err);
			}
		}
		break;
	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error: %d", evt->result);
			break;
		}
		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
		break;
	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT SUBACK error: %d", evt->result);
			break;
		}
		LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
		break;
	case MQTT_EVT_PINGRESP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PINGRESP error: %d", evt->result);
		}
		break;
	default:
		LOG_INF("Unhandled MQTT event type: %d", evt->type);
		break;
	}
}

static const uint8_t *client_id_get(void)
{
	static uint8_t client_id[MAX(sizeof(CONFIG_MQTT_CLIENT_ID), CLIENT_ID_LEN)];

	if (strlen(CONFIG_MQTT_CLIENT_ID) > 0) {
		snprintf(client_id, sizeof(client_id), "%s", CONFIG_MQTT_CLIENT_ID);
		goto exit;
	}

	uint32_t id = sys_rand32_get();
	snprintf(client_id, sizeof(client_id), "%s-%010u", CONFIG_BOARD, id);

exit:
	LOG_DBG("client_id = %s", (char *)client_id);

	return client_id;
}

int client_init(struct mqtt_client *client)
{
	int err;
	mqtt_client_init(client);



	client->broker = &server;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = client_id_get();
	client->client_id.size = strlen(client->client_id.utf8);
	client->password = NULL;
	client->user_name = NULL;
	client->protocol_version = MQTT_VERSION_3_1_1;
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* STEP 4 - Set the transport type to secure */
	struct mqtt_sec_config *tls_cfg = &(client->transport).tls.config;
	static sec_tag_t sec_tag_list[] = {MQTT_TLS_SEC_TAG};

	client->transport.type = MQTT_TRANSPORT_SECURE;

	tls_cfg->peer_verify = TLS_PEER_VERIFY_OPTIONAL;
	tls_cfg->cipher_count = 0;
	tls_cfg->cipher_list = NULL;
	tls_cfg->sec_tag_count = ARRAY_SIZE(sec_tag_list);
	tls_cfg->sec_tag_list = sec_tag_list;
	tls_cfg->session_cache = TLS_SESSION_CACHE_DISABLED;
	tls_cfg->hostname = CONFIG_MQTT_BROKER_HOSTNAME;

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		int err = publish(&client, MQTT_QOS_1_AT_LEAST_ONCE, CONFIG_BUTTON1_MSG,
				  sizeof(CONFIG_BUTTON1_MSG) - 1);
		if (err) {
			LOG_ERR("Failed to send message, %d", err);
			return;
		}
	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		int err = publish(&client, MQTT_QOS_1_AT_LEAST_ONCE, CONFIG_BUTTON2_MSG,
				  sizeof(CONFIG_BUTTON2_MSG) - 1);
		if (err) {
			LOG_ERR("Failed to send message, %d", err);
			return;
		}
	}
}

int main(void)
{
	int err;
	char * client_id = client_id_get();

	struct mqtt_helper_cfg config = {
		.cb = {
			.on_connack = on_mqtt_connack,
			.on_disconnect = on_mqtt_disconnect,
			.on_publish = on_mqtt_publish,
			.on_suback = on_mqtt_suback,
		},
	};

	struct mqtt_helper_conn_params conn_params = {
		.hostname.ptr = CONFIG_MQTT_BROKER_HOSTNAME,
		.hostname.size = strlen(CONFIG_MQTT_BROKER_HOSTNAME),
		.device_id.ptr = client_id,
		.device_id.size = strlen(client_id),
	};

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	LOG_INF("Waiting to connect to Wi-Fi");
	k_sem_take(&run_app, K_FOREVER);

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	err = mqtt_helper_init(&config);
	if (err) {
		LOG_ERR("mqtt_helper_init, error: %d", err);
		return 0;
	}

	LOG_INF("Connecting to MQTT broker");

	err = mqtt_helper_connect(&conn_params);
	if (err) {
		LOG_ERR("Failed connecting to MQTT, error code: %d", err);
		return 0;
	}

	/* STEP 5 - Update the file descriptor to use the TLS socket */
	/*fds.fd = client.transport.tls.sock;
	fds.events = POLLIN;*/

	//while (1) {
		/*err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
		if (err < 0) {
			LOG_ERR("Error in poll(): %d", errno);
			break;
		}

		err = mqtt_live(&client);
		if ((err != 0) && (err != -EAGAIN)) {
			LOG_ERR("Error in mqtt_live: %d", err);
			break;
		}
		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				LOG_ERR("Error in mqtt_input: %d", err);
				break;
			}
		}
		if ((fds.revents & POLLERR) == POLLERR) {
			LOG_ERR("POLLERR");
			break;
		}
		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			LOG_ERR("POLLNVAL");
			break;
		}*/
	//}
	//return 0;
	/*LOG_INF("Disconnecting MQTT client");
	err = mqtt_disconnect(&client);
	if (err) {
		LOG_ERR("Could not disconnect MQTT client: %d", err);
		return err;
	}*/
}