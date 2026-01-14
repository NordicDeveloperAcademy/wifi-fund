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

#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/wifi_credentials.h>

/* STEP 2 - Include the header file for the socket API */ 


LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 3 - Define the hostname and port for the echo server */
			

#define MESSAGE_SIZE 256 
#define MESSAGE_TO_SEND "Hello from nRF70 Series"
#define SSTRLEN(s) (sizeof(s) - 1)

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

/* STEP 4.1 - Declare the structure for the socket and server address */


/* STEP 4.2 - Declare the buffer for receiving from server */


static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
			  uint64_t mgmt_event, struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}
	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_INF("Network connected");
		connected = true;
		dk_set_led_on(DK_LED1);
		k_sem_give(&run_app);
		return;
	}
	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_INF("Waiting for network to be connected");
		} else {
			dk_set_led_off(DK_LED1);
			LOG_INF("Network disconnected");
			connected = false;
		}
		k_sem_reset(&run_app);
		return;
	}
}

static int server_resolve(void)
{
	/* STEP 5.1 - Call getaddrinfo() to get the IP address of the echo server */


	/* STEP 5.2 - Retrieve the relevant information from the result structure */


	/* STEP 5.3 - Convert the address into a string and print it */

	
	/* STEP 5.4 - Free the memory allocated for result */


	return 0;
}

static int server_connect(void)
{
	int err;
	/* STEP 6 - Create a UDP socket */


	/* STEP 7 - Connect the socket to the server */


	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 8 - Send a message every time button 1 is pressed */

}

int main(void)
{
	int received;

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

	/* STEP 9 - Resolve the server name and connect to the server */


	LOG_INF("Press button 1 on your DK to send your message");

	while (1) {
		/* STEP 10 - Listen for incoming messages */

			
	}
	close(sock);
	return 0;
}