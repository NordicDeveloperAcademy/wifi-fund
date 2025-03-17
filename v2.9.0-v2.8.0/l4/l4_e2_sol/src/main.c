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

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/socket.h>
#include <net/mqtt_helper.h>

LOG_MODULE_REGISTER(Lesson4_Exercise2, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

#define MESSAGE_BUFFER_SIZE 128

#define LED1_ON_CMD       "LED1ON"
#define LED1_OFF_CMD      "LED1OFF"
#define LED2_ON_CMD       "LED2ON"
#define LED2_OFF_CMD      "LED2OFF"
#define BUTTON1_MSG       "Button 1 pressed"
#define BUTTON2_MSG       "Button 2 pressed"

#define SUBSCRIBE_TOPIC_ID 1234

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

static uint8_t client_id[sizeof(CONFIG_BOARD) + 11];


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

static void subscribe(void)
{
	int err;

	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = CONFIG_MQTT_SAMPLE_SUB_TOPIC, 
			.size = strlen(CONFIG_MQTT_SAMPLE_SUB_TOPIC)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE};

	struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_TOPIC_ID};

	LOG_INF("Subscribing to %s", CONFIG_MQTT_SAMPLE_SUB_TOPIC);
	err = mqtt_helper_subscribe(&subscription_list);
	if (err) {
		LOG_ERR("Failed to subscribe to topics, error: %d", err);
		return;
	}
}

static int publish(uint8_t *data, size_t len)
{
	int err;
	struct mqtt_publish_param mqtt_param;

	mqtt_param.message.payload.data = data;
	mqtt_param.message.payload.len = len;
	mqtt_param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	mqtt_param.message_id = mqtt_helper_msg_id_get(),
	mqtt_param.message.topic.topic.utf8 = CONFIG_MQTT_SAMPLE_PUB_TOPIC;
	mqtt_param.message.topic.topic.size = strlen(CONFIG_MQTT_SAMPLE_PUB_TOPIC);
	mqtt_param.dup_flag = 0;
	mqtt_param.retain_flag = 0;

	err = mqtt_helper_publish(&mqtt_param);
	if (err) {
		LOG_WRN("Failed to send payload, err: %d", err);
		return err;
	}

	LOG_INF("Published message: \"%.*s\" on topic: \"%.*s\"", mqtt_param.message.payload.len,
								  mqtt_param.message.payload.data,
								  mqtt_param.message.topic.topic.size,
								  mqtt_param.message.topic.topic.utf8);
	return 0;
}

static void on_mqtt_connack(enum mqtt_conn_return_code return_code, bool session_present)
{
	if (return_code == MQTT_CONNECTION_ACCEPTED) {
		LOG_INF("Connected to MQTT broker");
		LOG_INF("Hostname: %s", CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME);
		LOG_INF("Client ID: %s", (char *)client_id);
		LOG_INF("Port: %d", CONFIG_MQTT_HELPER_PORT);
		LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");
		subscribe();
	} else {
		LOG_WRN("Connection to broker not established, return_code: %d", return_code);
	}
}

static void on_mqtt_suback(uint16_t message_id, int result)
{	
	if (result != MQTT_SUBACK_FAILURE) {
		if (message_id == SUBSCRIBE_TOPIC_ID) {
			LOG_INF("Subscribed to %s with QoS %d", CONFIG_MQTT_SAMPLE_SUB_TOPIC, result);
			return;
		}
		LOG_WRN("Subscribed to unknown topic, id: %d with QoS %d", message_id, result);
		return;
	}
	LOG_ERR("Topic subscription failed, error: %d", result);
}

static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
	LOG_INF("Received payload: %.*s on topic: %.*s", payload.size,
							 payload.ptr,
							 topic.size,
							 topic.ptr);
	
	if (strncmp(payload.ptr, LED1_ON_CMD,
			    sizeof(LED1_ON_CMD) - 1) == 0) {
				dk_set_led_on(DK_LED1);
	} else if (strncmp(payload.ptr, LED1_OFF_CMD,
			   sizeof(LED1_OFF_CMD) - 1) == 0) {
				dk_set_led_off(DK_LED1);
	} else if (strncmp(payload.ptr, LED2_ON_CMD,
			   sizeof(LED2_ON_CMD) - 1) == 0) {
				dk_set_led_on(DK_LED2);
	} else if (strncmp(payload.ptr, LED2_OFF_CMD,
			   sizeof(LED2_OFF_CMD) - 1) == 0) {
				dk_set_led_off(DK_LED2);
	}
}

static void on_mqtt_disconnect(int result)
{
	LOG_INF("MQTT client disconnected: %d", result);
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		int err = publish(BUTTON1_MSG, sizeof(BUTTON1_MSG) - 1);
		if (err) {
			LOG_ERR("Failed to send message, %d", err);
			return;
		}
	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		int err = publish(BUTTON2_MSG, sizeof(BUTTON2_MSG) - 1);
		if (err) {
			LOG_ERR("Failed to send message, %d", err);
			return;
		}
	}
}

int main(void)
{
	int err;

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

	struct mqtt_helper_cfg config = {
		.cb = {
			.on_connack = on_mqtt_connack,
			.on_disconnect = on_mqtt_disconnect,
			.on_publish = on_mqtt_publish,
			.on_suback = on_mqtt_suback,
		},
	};

	err = mqtt_helper_init(&config);
	if (err) {
		LOG_ERR("Failed to initialize MQTT helper, error: %d", err);
		return 0;
	}

	uint32_t id = sys_rand32_get();
	snprintf(client_id, sizeof(client_id), "%s-%010u", CONFIG_BOARD, id);

	struct mqtt_helper_conn_params conn_params = {
		.hostname.ptr = CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME,
		.hostname.size = strlen(CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME),
		.device_id.ptr = (char *)client_id,
		.device_id.size = strlen(client_id),
	};

	err = mqtt_helper_connect(&conn_params);
	if (err) {
		LOG_ERR("Failed to connect to MQTT, error code: %d", err);
		return 0;
	}
}