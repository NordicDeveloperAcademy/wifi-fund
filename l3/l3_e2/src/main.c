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

#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <net/wifi_mgmt_ext.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/socket.h>

/* STEP 2 - Include the header files for the zperf API and nrfx clock */ 


LOG_MODULE_REGISTER(Lesson3_Exercise2, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 3.1 - Define port for the zperf server */


/* STEP 3.2 - Define packet size, rate and test duration */


/* STEP 4 - Create a socket address struct for the server address */


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

static void udp_upload_results_cb(enum zperf_status status,
			  struct zperf_results *result,
			  void *user_data)
{
	unsigned int client_rate_in_kbps;

	/* Handle the three zperf session statuses: started, finished, and error */
	switch (status) {
	case ZPERF_SESSION_STARTED:
		/* STEP 7.1 - Inform the user that the UDP session has started */

		break;
	case ZPERF_SESSION_FINISHED:
		LOG_INF("Wi-Fi throughput test: Upload completed!");
		/* STEP 7.2 - If client_time_in_us is not zero, calculate the throughput rate in kilobit per second. Otherwise, set it to zero */

		/* STEP 7.3 - Print the results of the throughput test */

		break;
	case ZPERF_SESSION_ERROR:
		/* STEP 7.4 - Inform the user that there is an error with the UDP session */

		break;
	}
}

int main(void)
{
	int ret;

	/* Configures the clock domain divider for the HF clock */
	#ifdef CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT
	nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
	#endif

	LOG_INF("Starting %s with CPU frequency: %d MHz", CONFIG_BOARD, SystemCoreClock/MHZ(1));
	
	k_sleep(K_SECONDS(1));
	
	
	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	LOG_INF("Waiting to connect to Wi-Fi");
	k_sem_take(&run_app, K_FOREVER);

	/* STEP 5.1 - Initialize a struct for storing the zperf upload parameters */


	/* STEP 5.2 - Configure packet size, rate and duration from the defines created earlier */
	

	/* STEP 5.3 - Convert the server address from a string to IP address */

	LOG_INF("IPv4 address %s", CONFIG_NET_CONFIG_PEER_IPV4_ADDR);

	/* STEP 5.4 - Add the zperf server address to the zperf_upload_params struct */


	LOG_INF("Starting Wi-Fi throughput test: Zperf client");

	/* STEP 6 - Call zperf_udp_upload_async() to start the asynchronous UDP upload */


	return 0;
}