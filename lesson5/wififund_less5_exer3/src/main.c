/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>

/* Step 2 - Include Zephyr's HTTP parser header file */


/* Step 3 - Include the LEDs header file */


LOG_MODULE_REGISTER(http_server, LOG_LEVEL_DBG);

/* Step 4 - Define the port number to be used by the server to listen to incoming HTTP messages */


#define MAX_CLIENT_QUEUE		2
#define STACK_SIZE			4096
#define THREAD_PRIORITY			K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* Step 5 - Define a struct to represent the structure of HTTP requests */


/* Forward declarations */
static void process_tcp4(void);

/* Keep track of the current LED states. 0 = LED OFF, 1 = LED ON.
 * Index 0 corresponds to LED1, index 1 to LED2.
 */
static uint8_t led_states[2];

/* Step 6 - Define HTTP server response strings for demonstration */


/* Step 7 - Set up threads for handling multiple incoming TCP connections simultaneously */


/* Step 8 - Declare and initialize structs and variables used in network management */


/* Event handler for network management. */
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
			LOG_INF("Waiting network to be connected");
		} else {
			LOG_INF("Network disconnected");
			connected = false;
		}

		k_sem_reset(&run_app);

		return;
	}
}

/* Functions for handling incoming HTTP requests */

/* Step 9 -  Define a function that receives the HTTP request and the target LED and updates the LED state accordingly */
/* Update the LED states. Returns true if it was updated, otherwise false.*/


/* Step 10 -   Define a function to handle HTTP requests */



/* Step 11 - Define callbacks for the HTTP parser */


/* Step 12 -  Setup the HTTP server */


/* Step 13 -  Setup the TCP client connection handler */


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

/* Step 14 - Setup functions to handle incoming TCP connections */


/* Step 15 - Start Listening */
/* Step 15.1 - Define function to Start Listening on TCP socket */


int main(void)
{
	int err;

	parser_init();

#if defined(CONFIG_DK_LIBRARY)
	err = dk_leds_init();
	if (err) {
		LOG_ERR("Failed to initialize LEDs");
		return 0;
	}
#endif /* CONFIG_DK_LIBRARY */

	if (IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
		net_mgmt_add_event_callback(&mgmt_cb);

		conn_mgr_mon_resend_status();
	}

	if (!IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		/* If the config library has not been configured to start the
		 * app only after we have a connection, then we can start
		 * it right away.
		 */
		k_sem_give(&run_app);
	}

	/* Wait for the connection. */
	k_sem_take(&run_app, K_FOREVER);

/* Step 15.2 - Call the start_listener function */
	

	(void)err;

	return 0;
}
