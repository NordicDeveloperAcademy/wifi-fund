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

/* STEP 1.2 - Include the header file of the HTTP client library */


LOG_MODULE_REGISTER(Lesson5_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 2 - Define the macros for the HTTP server hostname and port */


/* STEP 3 - Declare the necessary buffers for receiving messages */


/* STEP 4 - Define the variable for the counter as 0 */


static int sock;
static struct sockaddr_storage server;

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
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};

	err = getaddrinfo(HTTP_HOSTNAME, STRINGIFY(HTTP_PORT), &hints, &result);
	if (err != 0) {
		LOG_ERR("getaddrinfo failed, err: %d, %s", err, gai_strerror(err));
		return -EIO;
	}

	if (result == NULL) {
		LOG_ERR("Error, address not found");
		return -ENOENT;
	}

	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);
	server4->sin_addr.s_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = ((struct sockaddr_in *)result->ai_addr)->sin_port;

	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr, sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);

	freeaddrinfo(result);

	return 0;
}

static int server_connect(void)
{
	int err;
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connecting to server failed, err: %d, %s", errno, strerror(errno));
		return -errno;
	}

	return 0;
}

static void response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	/* STEP 9 - Define the callback function to print the body */

}

static void client_id_cb(struct http_response *rsp, enum http_final_call final_data,
			 void *user_data)
{
	/* STEP 6.1 - Log the HTTP response status */


	/* STEP 6.2 - Retrieve and format the client ID */

}

static int client_http_put(void)
{
	/* STEP 7 - Define the function to send a PUT request to the HTTP server */

}

static int client_http_get(void)
{
	/* STEP 8 - Define the function to send a GET request to the HTTP server */

}

static int client_get_new_id(void)
{
	int err = 0;

	/* STEP 5.1 - Define the structure http_request and fill the block of memory */


	/* STEP 5.2 - Populate the http_request structure */


	/* STEP 5.3 - Send the request to the HTTP server */


	return err;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 10 - Define the button handler to send requests upon button triggers */

}

int main(void)
{

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
	if (server_resolve() != 0) {
		LOG_ERR("Failed to resolve server name");
		return 0;
	}

	if (server_connect() != 0) {
		LOG_ERR("Failed to initialize client");
		return 0;
	}
	LOG_INF("Successfully connected to HTTP server");

	/* STEP 11 - Retrieve the client ID upon connection */


	return 0;
}
