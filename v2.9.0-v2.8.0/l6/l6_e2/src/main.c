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

LOG_MODULE_REGISTER(Lesson6_Exercise2, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 3.1 - Define a mask for the TWT events */


/* STEP 1.1 - Define the IPv4 address and port for the server */


#define SSTRLEN(s) (sizeof(s) - 1)
#define RECV_BUF_SIZE 256

static int counter = 0;
static int recv_counter = 0;

static int sock;
static struct sockaddr_in server;

static uint8_t recv_buf[RECV_BUF_SIZE];

int send_packet();
int receive_packet();

/* STEP 1.2 - Define macros for wakeup time and interval for TWT  */


/* STEP 1.3 - Create variables for TWT status, flow ID, and sending packets */


static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback twt_mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

static int sock;
static struct sockaddr_in server;

static uint8_t recv_buf[MESSAGE_SIZE];

static void handle_wifi_twt_event(struct net_mgmt_event_callback *cb)
{
	/* STEP 4.1 - Create a struct for the received TWT event */


	/* STEP 4.2 - Upon a TWT teardown initiated by the AP, toggle the state */


	/* STEP 4.3 - Update the flow ID received in the TWT response */


	/* STEP 4.4 - Check if a TWT response was received */

	/* STEP 4.5 - Upon an accepted TWT setup, log the negotiated parameters */

}

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

static void twt_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				   struct net_if *iface)
{
	switch (mgmt_event) {
	/* STEP 3.2.1 - Upon a TWT event, call handle_wifi_twt_event() to handle the response */

	/* STEP 3.2.2 -	Upon TWT sleep state event, inform the user of the current sleep state */

	}
}

int wifi_set_twt()
{
	struct net_if *iface = net_if_get_first_wifi();

	/* STEP 2.1 - Define the TWT parameters struct */


	if (!nrf_wifi_twt_enabled) {
		/* STEP 2.2.1 - Fill in the TWT setup specific parameters */

	} else {
		/* STEP 2.2.2 - Fill in the TWT teardown specific parameters */

	}

	/* STEP 2.3 - Send the TWT request with net_mgmt */

	LOG_INF("-------------------------------");
	LOG_INF("TWT operation %s requested", wifi_twt_operation_txt(twt_params.operation));
	LOG_INF("-------------------------------");
	return 0;
}

static int server_connect(void)
{
	int err;
	
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);

	err = inet_pton(AF_INET, SERVER_IPV4_ADDR, &server.sin_addr);
	if (err <= 0) {
		LOG_ERR("Invalid address, err: %d, %s", errno, strerror(errno));
		close(sock);
		return -errno;
	}

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server, sizeof(server));
	if (err < 0) {
		LOG_ERR("Connecting to server failed, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	LOG_INF("Connected to server");

	return 0;
}

int send_packet()
{
	int err;
	int bytes_written;
	char buffer[30] = {"\0"};
	bytes_written = snprintf(buffer, 30, "Hello from nRF70 Series! %d", counter);
	if (bytes_written < 0) {
		LOG_INF("Unable to write to buffer, err: %d", bytes_written);
		return bytes_written;
	}

	err = sendto(sock, &buffer, SSTRLEN(buffer), 0, (struct sockaddr *)&server, sizeof(server));
	if (err < 0) {
		LOG_ERR("Failed to send message, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	LOG_INF("Successfully sent message: %s", buffer);
	counter++;
	return 0;
}

int receive_packet()
{
	int received;

	socklen_t addr_len;
	addr_len = sizeof(server);
	received = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr *)&server,
			    &addr_len);

	if (received <= 0) {
		return -1;
	}

	recv_buf[received] = 0;
	LOG_INF("Data received from the server: (%s)", recv_buf);
	recv_counter++;

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	/* STEP 5.1 - Enable or disable TWT when button 1 is pressed */


	/* STEP 5.2 - Enable or disable sending packets during TWT awake when button 2 is pressed */

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

	/* STEP 3.3 - Initialize and add the TWT event handler */

	LOG_INF("Waiting to connect to Wi-Fi");
	k_sem_take(&run_app, K_FOREVER);

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	LOG_INF("Connecting to %s:%d", SERVER_IPV4_ADDR, SERVER_PORT);	
	if (server_connect() != 0) {
		LOG_ERR("Failed to connect to server");
		return 0;
	}

	LOG_INF("Press button 1 on your DK to enable or disable TWT");
	send_packet();

	while (1) {
		k_sleep(K_MSEC(1000));
		if (recv_counter < counter) {
			receive_packet();
		}
	}
	close(sock);
	return 0;
}