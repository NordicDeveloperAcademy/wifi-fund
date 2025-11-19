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
#include <zephyr/net/wifi_credentials.h>
#include <zephyr/net/socket.h>

/* STEP 1.3 - Include the header file for the MQTT helper library */


LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

#define MESSAGE_BUFFER_SIZE 128

/* STEP 2 - Define the commands to control and monitor LEDs and buttons */

/* STEP 3 - Define the message ID used when subscribing to topics */

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

/* STEP 10.1 - Declare the variable to store the client ID */


static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
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

/* STEP 7 - Define the function to publish data */
static int publish(uint8_t *data, size_t len)
{
	int err;
	/* STEP 7.1 - Declare and populate a variable of type mqtt_publish_param */	


	/* STEP 7.2 - Publish to MQTT broker */

	return 0;
}

/* STEP 8.1 - Define callback handler for CONNACK event */


/* STEP 8.2 - Define callback handler for SUBACK event */


/* STEP 8.3 - Define callback handler for PUBLISH event */


/* STEP 8.4 - Define callback handler for DISCONNECT event */



static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		/* STEP 9.1 - Publish message if button 1 is pressed */

	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		/* STEP 9.2 - Publish message if button 2 is pressed */

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

	/* STEP 9 - Initialize the MQTT helper library */

	/* STEP 10.2 - Generate the client ID */

	/* STEP 11 - Establish a connection the MQTT broker */

}