/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_config.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <net/wifi_mgmt_ext.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>

#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>

/* STEP 2.2 - Include the header file for the HTTP parser library */


LOG_MODULE_REGISTER(Lesson5_Exercise3, LOG_LEVEL_INF);

#define MAX_CLIENT_QUEUE		2
#define STACK_SIZE			    4096
#define THREAD_PRIORITY			K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#define EVENT_MASK              (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 4 - Register service to be advertised via DNS */

/* STEP 5 - Define a struct to represent the structure of HTTP requests */


/* Forward declarations */
static void process_tcp4(void);

/* Keep track of the current LED states. 0 = LED OFF, 1 = LED ON.
 * Index 0 corresponds to LED1, index 1 to LED2.
 */
static uint8_t led_states[2];

/* STEP 6 - Define HTTP server response strings for demonstration */


/* STEP 7 - Set up threads for handling multiple incoming TCP connections simultaneously */


/* STEP 8 - Declare and initialize structs and variables used in network management */


/* STEP 11.1 - Define a variable to store the HTTP parser settings */


static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

static void net_mgmt_event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
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
		}

		k_sem_reset(&run_app);

		return;
	}
}

/* STEP 9 -  Define a function to update the LED state */


/* STEP 10 -   Define a function to handle HTTP requests */


/* STEP 11.2 - Define callbacks for the HTTP parser */


/* STEP 11.3 - Define function to initialize the callbacks */


/* STEP 12 -  Setup the HTTP server */


/* STEP 13 -  Setup the TCP client connection handler */


static int get_free_slot(int *accepted)
{
	int i;

	for (i = 0; i < MAX_CLIENT_QUEUE; i++) {
		if (accepted[i] < 0) {
			return i;
		}
	}

	return -1;
}

/* STEP 14.1 - Define a function to handle incoming TCP connections */


/* STEP 14.2 - Define a function to process incoming IPv4 clients */

/* STEP 15.1 - Define function to start listening on TCP socket */


int main(void)
{
	int err;

	parser_init();

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Failed to initialize LEDs");
		return 0;
	}

	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	conn_mgr_mon_resend_status();

	/* Wait for the connection. */
	k_sem_take(&run_app, K_FOREVER);

	/* STEP 15.2 - Start listening on the TCP socket */


	(void)err;

	return 0;
}
