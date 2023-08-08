/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <net/wifi_mgmt_ext.h>

/* STEP 2 - Include the header file for the socket API */ 


LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);

/* STEP 3.1 - Define the semaphores */


#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

/* STEP 4 - Define the hostname and port for the echo server */
			

#define MESSAGE_SIZE 256 
#define MESSAGE_TO_SEND "Hello from nRF70 Series"
#define SSTRLEN(s) (sizeof(s) - 1)

/* STEP 5.1 - Declare the structure for the socket and server address */


/* STEP 5.2 - Declare the buffer for receiving from server */


static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_mgmt_event_callback ipv4_mgmt_cb;

static int wifi_args_to_params(struct wifi_connect_req_params *params)
{

	params->ssid = CONFIG_WIFI_CREDENTIALS_STATIC_SSID;
	params->ssid_length = strlen(params->ssid);
	params->psk = CONFIG_WIFI_CREDENTIALS_STATIC_PASSWORD;
	params->psk_length = strlen(params->psk);
	params->channel = WIFI_CHANNEL_ANY;
	params->security = 1;
	params->mfp = WIFI_MFP_OPTIONAL;
	params->timeout = SYS_FOREVER_MS;

	return 0;
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to Wi-Fi Network: %s", CONFIG_WIFI_CREDENTIALS_STATIC_SSID);
        dk_set_led_on(DK_LED1);
		/* STEP 3.3 - Upon a Wi-Fi connection, give the semaphore */

		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Disconnected from Wi-Fi Network");
        dk_set_led_off(DK_LED1);
		break;		
	default:
        LOG_ERR("Unknown event: %d", mgmt_event);
		break;
	}
}

static void ipv4_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t event, struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_IPV4_ADDR_ADD:
		LOG_INF("IPv4 address acquired");
		/* STEP 3.4 - Upon acquiring an IPv4 address, give the semaphore */

		break;
	case NET_EVENT_IPV4_ADDR_DEL:
		LOG_INF("IPv4 address lost");
		break;
	default:
		LOG_DBG("Unknown event: 0x%08X", event);
		return;
	}
}

static int wifi_connect() {
	struct wifi_connect_req_params cnx_params;
	struct net_if *iface = net_if_get_default();
	if (iface == NULL) {
		LOG_ERR("Returned network interface is NULL");
		return -1;
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);
	net_mgmt_init_event_callback(&ipv4_mgmt_cb, ipv4_mgmt_event_handler, IPV4_MGMT_EVENTS);
	net_mgmt_add_event_callback(&ipv4_mgmt_cb);

	wifi_args_to_params(&cnx_params);

	LOG_INF("Connecting to Wi-Fi");
	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));
	if (err) {
		LOG_ERR("Connecting to Wi-Fi failed, err: %d", err);
		return ENOEXEC;
	}

	/* STEP 3.2 - Take the semaphores */


	return 0;
}

static int server_resolve(void)
{
	/* STEP 6.1 - Call getaddrinfo() to get the IP address of the echo server */

	/* STEP 6.2 - Retrieve the relevant information from the result structure */

	/* STEP 6.3 - Convert the address into a string and print it */
	
	/* STEP 6.4 - Free the memory allocated for result */

	return 0;
}

static int server_connect(void)
{
	int err;
	/* STEP 7 - Create a UDP socket */

	/* STEP 8 - Connect the socket to the server */

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 9 - Send a message every time button 1 is pressed */

}

int main(void)
{
	int received;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	/* STEP 10 - Resolve the server name and connect to the server */

	LOG_INF("Press button 1 on your DK to send your message");

	while (1) {
		/* STEP 10 - Listen for incoming messages */
			
	 	}
	close(sock);
	return 0;
}