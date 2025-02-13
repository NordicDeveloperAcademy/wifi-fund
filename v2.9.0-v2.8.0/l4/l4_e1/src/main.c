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

/* STEP 1.3 - Include the header file for the MQTT library */

/* STEP 1.x - Include the header file for the MQTT helper library */


LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

#define CLIENT_ID_LEN  sizeof(CONFIG_BOARD) + 11
#define MESSAGE_BUFFER_SIZE 128

/* STEP 2 - Define the commands to control and monitor LEDs and buttons */

/* STEP 3 - 3. Define the message ID used when subscribing to topics */

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);


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
			/* STEP 5 - Disconnect from MQTT broker if disconnected from network */
		}
		k_sem_reset(&run_app);
		return;
	}
}

/* STEP 6 - Define the function to subscribe to topics */
static void subscribe(void)
{
	int err;

	/* STEP 6.1 - Declare a variable of type mqtt_topic */

	/* STEP 6.2 - Define a subscription list */

	/* STEP 6.3 - Subscribe to topics */

}

/* STEP 6 - Define the function to publish data */

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		/* STEP 4 - Upon a successful connection, subscribe to topics */
		
	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected: %d", evt->result);
		break;

	case MQTT_EVT_PUBLISH:
		/* STEP 5 - Listen to published messages received from the broker and extract the
		 * message */
		{
			/* STEP 5.1 - Extract the payload and (if relevant) send acknowledgement */

			/* STEP 5.2 - On successful extraction of data, exmaine command and toggle
			 * LED accordingly */

			/* STEP 5.3 - On failed extraction of data, examine error code */
			
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
	/* STEP 2.1 - Initialize the client instance */

	/* STEP 2.2 - Resolve the configured hostname and initializes the MQTT broker structure */

	/* STEP 2.3 - MQTT client configuration */

	/* STEP 2.4 - MQTT buffers configuration */

	/* STEP 2.5 - Set the transport type to non-secure */

	return err;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	int err;
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		/* STEP 7.1 - When button 1 is pressed, send a message */

	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		/* STEP 7.2 - When button 2 is pressed, send a message */

	}
}

int main(void)
{
	int err;
	uint32_t connect_attempt = 0;

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

	LOG_INF("Connecting to MQTT broker");

	err = client_init(&client);
	if (err) {
		LOG_ERR("Failed to initialize MQTT client: %d", err);
		return err;
	}

do_connect:
	if (connect_attempt++ > 0) {
		LOG_INF("Reconnecting in 60 seconds...");
		k_sleep(K_SECONDS(60));
	}

	/* STEP 8 - Establish a connection the MQTT broker */

	/* STEP 9.1 - Configure fds to monitor the socket */

	while (1) {
		/* STEP 9.2 - Continously poll the socket for incoming data */

		/* STEP 9.3 - In the event of incoming data, process it */
		
	}

	LOG_INF("Disconnecting MQTT client");

	err = mqtt_disconnect(&client);
	if (err) {
		LOG_ERR("Could not disconnect MQTT client: %d", err);
	}
	goto do_connect;
}