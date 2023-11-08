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

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT|	\
				NET_EVENT_WIFI_TWT| 	\
				NET_EVENT_WIFI_TWT_SLEEP_STATE)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

/* STEP x - Define the port and IPv4 address for the echo server */
#define SERVER_PORT 7777
#define SERVER_IPV4_ADDR "192.168.32.119"

#define SSTRLEN(s) (sizeof(s) - 1)
#define RECV_BUF_SIZE 256

/* STEP x - Define counter and   */
static int counter = 0;
static int recv_counter = 0;

/* STEP 5.1 - Declare the structure for the socket and server address */
static int sock;
static struct sockaddr_in server;

/* STEP 5.2 - Declare the buffer for receiving from server */
static uint8_t recv_buf[RECV_BUF_SIZE];

/* STEP x - */
int send_packet();
int receive_packet();

/* STEP x - Define wakeup time and interval for TWT (interval = wake up time + sleeping time)  */
#define TWT_WAKE_INTERVAL_MS 65
#define TWT_INTERVAL_S 		 30

bool nrf_wifi_twt_enabled = 0;
static uint32_t twt_flow_id = 1;

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
	/* STEP x - */
	const struct wifi_twt_params *resp = (const struct wifi_twt_params *)cb->info;
	
	if (resp->operation == WIFI_TWT_TEARDOWN) {
		LOG_INF("TWT teardown received for flow ID %d\n",
		      resp->flow_id);
		nrf_wifi_twt_enabled = 0;
		return;
	}

	twt_flow_id = resp->flow_id;

	/* STEP x - */
	if (resp->resp_status == WIFI_TWT_RESP_RECEIVED) {
		LOG_INF("TWT response: %s",
		      wifi_twt_setup_cmd_txt(resp->setup_cmd));

		if (resp->setup_cmd == WIFI_TWT_SETUP_CMD_ACCEPT) {
			nrf_wifi_twt_enabled = 1;
		
		LOG_INF("== TWT negotiated parameters ==");
		LOG_INF("TWT Dialog token: %d",
		      resp->dialog_token);
		LOG_INF("TWT flow ID: %d",
		      resp->flow_id);
		LOG_INF("TWT negotiation type: %s",
		      wifi_twt_negotiation_type_txt(resp->negotiation_type));
		LOG_INF("TWT responder: %s",
		       resp->setup.responder ? "true" : "false");
		LOG_INF("TWT implicit: %s",
		      resp->setup.implicit ? "true" : "false");
		LOG_INF("TWT announce: %s",
		      resp->setup.announce ? "true" : "false");
		LOG_INF("TWT trigger: %s",
		      resp->setup.trigger ? "true" : "false");
		LOG_INF("TWT wake interval: %d ms (%d us)",
		      resp->setup.twt_wake_interval/USEC_PER_MSEC,
			  resp->setup.twt_wake_interval);
		LOG_INF("TWT interval: %lld s (%lld us)",
			  resp->setup.twt_interval/USEC_PER_SEC,
		      resp->setup.twt_interval);
		LOG_INF("===============================");
		}
	} 
	else {
		LOG_INF("TWT response timed out\n");
	}
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
	/* STEP x - */
	case NET_EVENT_WIFI_TWT:
		handle_wifi_twt_event(cb);
		break;
	/* STEP x - */
	case NET_EVENT_WIFI_TWT_SLEEP_STATE:
		int *twt_state;
		twt_state = (int *)(cb->info);
		LOG_INF("TWT sleep state: %s", *twt_state ? "awake" : "sleeping" );
		if (*twt_state == WIFI_TWT_STATE_AWAKE) {
			send_packet();
				receive_packet();
		}
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
	/* STEP x - */
	struct net_if *iface = net_if_get_default();

	struct wifi_twt_params params = { 0 };

	params.negotiation_type = WIFI_TWT_INDIVIDUAL;
	params.setup_cmd = WIFI_TWT_SETUP_CMD_REQUEST;
	params.flow_id = twt_flow_id;
	params.dialog_token = 1;


	if (!nrf_wifi_twt_enabled){
		/* STEP x - */
		params.operation = WIFI_TWT_SETUP;
		params.setup.responder = 0;
		params.setup.trigger = 1;
		params.setup.implicit = 1;
		params.setup.announce = 1;
		params.setup.twt_wake_interval = TWT_WAKE_INTERVAL_MS * USEC_PER_MSEC;
		params.setup.twt_interval = TWT_INTERVAL_S * USEC_PER_SEC;
	}
	else {
		/* STEP x - */
		params.operation = WIFI_TWT_TEARDOWN;
		params.teardown.teardown_all = 1;
		twt_flow_id = twt_flow_id<WIFI_MAX_TWT_FLOWS ? twt_flow_id+1 : 1;
		nrf_wifi_twt_enabled = 0;
	}

	/* STEP x - */
	if (net_mgmt(NET_REQUEST_WIFI_TWT, iface, &params, sizeof(params))) {
		LOG_ERR("%s with %s failed, reason : %s",
			wifi_twt_operation_txt(params.operation),
			wifi_twt_negotiation_type_txt(params.negotiation_type),
			wifi_twt_get_err_code_str(params.fail_reason));
		return -1;
	}
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

	recv_counter++;
	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;
	
	if (button & DK_BTN1_MSK) {
		wifi_set_twt();
	}

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
	/* STEP x - */
	send_packet();

	while (1) {
		k_sleep(K_MSEC(1000));
		/* STEP x - */
		receive_packet();
	}
	close(sock);
	return 0;
}