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
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <net/wifi_mgmt_ext.h>

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(Lesson6_Exercise2, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

/* STEP 3.1 - Modify WIFI_MGMT_EVENTS to add TWT events. */
#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

/* STEP 1.1 - Define the port and IPv4 address for the server. */


#define SSTRLEN(s) (sizeof(s) - 1)
#define RECV_BUF_SIZE 256

static int counter = 0;

static int sock;
static struct sockaddr_in server;

static uint8_t recv_buf[RECV_BUF_SIZE];

int send_packet();
int receive_packet();

/* STEP 1.2 - Define macros for wakeup time and interval for TWT.  */


/* STEP 1.3 - Create two variables to keep track of TWT status and flow ID. */


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

static void handle_wifi_twt_event(struct net_mgmt_event_callback *cb)
{
	/* STEP 4.1 - Create a wifi_twt_params struct for the TWT response and fill it with the response information. */
	
	
	/* STEP 4.2 - If the TWT request was for TWT teardown, change the value of nrf_wifi_twt_enabled and exit the function. */
	

	/* STEP 4.3 - Update twt_flow_id to reflect the flow ID received in the TWT response. */
	

	/* STEP 4.4 - Check if a TWT response was received. If not, the TWT request timed out. */
	

	/* STEP 4.5 - If the TWT setup was accepted, change the value of nrf_wifi_twt_enabled and print the negotiated parameters. */
	
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to Wi-Fi Network: %s", CONFIG_WIFI_CREDENTIALS_STATIC_SSID);
        dk_set_led_on(DK_LED1);
		k_sem_give(&wifi_connected_sem);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Disconnected from Wi-Fi Network");
        dk_set_led_off(DK_LED1);
		break;
	case NET_EVENT_WIFI_TWT:
		/* STEP 3.2.1 - Upon a TWT event, call handle_wifi_twt_event() to handle the response. */

		break;
	case NET_EVENT_WIFI_TWT_SLEEP_STATE:
		/* STEP 3.2.2 -	Upon TWT sleep state event, inform the user of the current sleep state.
		/* When the device is in the awake state, send a packet to the server and check for any received packets. */
		
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
		k_sem_give(&ipv4_obtained_sem);
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

	k_sem_take(&wifi_connected_sem, K_FOREVER);
	k_sem_take(&ipv4_obtained_sem, K_FOREVER);
	
	return 0;
}


int wifi_set_twt()
{
	
	struct net_if *iface = net_if_get_default();

	/* STEP 2.1 - Define the TWT parameters struct wifi_twt_params and fill the parameters that are common for both TWT setup and TWT teardown. */


	if (!nrf_wifi_twt_enabled){
		/* STEP 2.2 - Fill in the TWT setup specific parameters of the wifi_twt_params struct. */
		
	}
	else {
		/* STEP 2.2.2 - Fill in the TWT teardown specific parameters of the wifi_twt_params struct. */
		
	}

	/* STEP 2.3 - Send the TWT request with net_mgmt. */
	

	LOG_INF("TWT operation %s requested", 
			wifi_twt_operation_txt(params.operation));
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
		LOG_INF("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server, sizeof(server));
	if (err < 0) {
		LOG_INF("Connecting to server failed, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	LOG_INF("Connected to server");

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
		LOG_INF("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}
	err = connect(sock, (struct sockaddr *)&server, sizeof(server));
	if (err < 0) {
		LOG_INF("Connecting to server failed, err: %d, %s", errno, strerror(errno));
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
	if (bytes_written < 0){
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
	received = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr *)&server, &addr_len);

	if (received <= 0) {
		return -1;
	}

	recv_buf[received] = 0;
	LOG_INF("Data received from the server: (%s)", recv_buf);

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;
	
	/* STEP x - Call wifi_set_twt() to enable or disable TWT when button 1 is pressed. */
	

	/* You can also send a packet with button 2. */
	if (button & DK_BTN2_MSK) {
		send_packet();
	}
}

int main(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	if (server_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	LOG_INF("Press button 1 on your DK to enable or disable TWT");
	send_packet();

	while (1) {
		k_sleep(K_MSEC(1000));
		receive_packet();
	}
	close(sock);
	return 0;
}