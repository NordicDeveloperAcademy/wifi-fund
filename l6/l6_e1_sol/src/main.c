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
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>

LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

#define RECV_BUF_SIZE  2048
#define CLIENT_ID_SIZE 36

static char recv_buf[RECV_BUF_SIZE];
static char client_id_buf[CLIENT_ID_SIZE + 2];

static int counter = 0;

static int sock;
static struct sockaddr_storage server;

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

/* STEP 1 - Define variables for power save status and PUT/GET requests */
bool nrf_wifi_ps_enabled = 1;
bool http_put = 1;

/* STEP 6.1 - Define a variable for power save wakeup mode status */
bool nrf_wifi_ps_wakeup_mode = 0;

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

	err = getaddrinfo(CONFIG_HTTP_SAMPLE_HOSTNAME, CONFIG_HTTP_SAMPLE_PORT, &hints, &result);
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
	LOG_INF("IPv4 address of HTTP server found %s", ipv4_addr);

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

static int response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	LOG_INF("Response status: %s", rsp->http_status);

	if (rsp->body_frag_len > 0) {
		char body_buf[rsp->body_frag_len];
		strncpy(body_buf, rsp->body_frag_start, rsp->body_frag_len);
		body_buf[rsp->body_frag_len] = '\0';
		LOG_INF("Received: %s", body_buf);
	}

	close(sock);
	return 0;
}

static int client_id_cb(struct http_response *rsp, enum http_final_call final_data,
			 void *user_data)
{
	LOG_INF("Response status: %s", rsp->http_status);

	char client_id_buf_tmp[CLIENT_ID_SIZE + 1];
	strncpy(client_id_buf_tmp, rsp->body_frag_start, CLIENT_ID_SIZE);
	client_id_buf_tmp[CLIENT_ID_SIZE] = '\0';
	client_id_buf[0] = '/';
	strcat(client_id_buf, client_id_buf_tmp);

	LOG_INF("Successfully acquired client ID: %s", client_id_buf);

	close(sock);
	return 0;
}

static int client_http_put(void)
{
	int err = 0;
	int bytes_written;
	const char *headers[] = {"Connection: close\r\n", NULL};

	struct http_request req;
	memset(&req, 0, sizeof(req));

	char buffer[12] = {0};
	bytes_written = snprintf(buffer, 12, "%d", counter);
	if (bytes_written < 0) {
		LOG_INF("Unable to write to buffer, err: %d", bytes_written);
		return bytes_written;
	}

	req.header_fields = headers;
	req.method = HTTP_PUT;
	req.url = client_id_buf;
	req.host = CONFIG_HTTP_SAMPLE_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.payload = buffer;
	req.payload_len = bytes_written;
	req.response = response_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	LOG_INF("HTTP PUT request: %s", buffer);
	err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		LOG_ERR("Failed to send HTTP PUT request %s, err: %d", buffer, err);
	}

	return err;
}

static int client_http_get(void)
{
	int err = 0;
	const char *headers[] = {"Connection: close\r\n", NULL};

	struct http_request req;
	memset(&req, 0, sizeof(req));

	req.header_fields = headers;
	req.method = HTTP_GET;
	req.url = client_id_buf;
	req.host = CONFIG_HTTP_SAMPLE_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = response_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	LOG_INF("HTTP GET request");
	err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		LOG_ERR("Failed to send HTTP GET request, err: %d", err);
	}

	return err;
}

static int client_get_new_id(void)
{
	int err = 0;

	struct http_request req;
	memset(&req, 0, sizeof(req));

	const char *headers[] = {"Connection: close\r\n", NULL};
	req.header_fields = headers;
	req.method = HTTP_POST;
	req.url = "/new";
	req.host = CONFIG_HTTP_SAMPLE_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = client_id_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	LOG_INF("HTTP POST request");
	err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		LOG_ERR("Failed to send HTTP POST request, err: %d", err);
	}
	return err;
}

int wifi_set_power_state()
{
	struct net_if *iface = net_if_get_first_wifi();

	/* STEP 2.1 - Define the Wi-Fi power save parameters structure */
	struct wifi_ps_params ps_params = {0};

	/* STEP 2.2 - Check if power saving is currently enabled */
	if (!nrf_wifi_ps_enabled) {
		ps_params.enabled = WIFI_PS_ENABLED;
	} else {
		ps_params.enabled = WIFI_PS_DISABLED;
	}

	/* STEP 2.3 - Send the power save request */
	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_params, sizeof(ps_params))) {
		LOG_ERR("Power save %s failed. Reason %s", ps_params.enabled ? "enable" : "disable",
			wifi_ps_get_config_err_code_str(ps_params.fail_reason));
		return -1;
	}
	LOG_INF("Set power save: %s", ps_params.enabled ? "enable" : "disable");

	/* STEP 2.4 - Toggle the power save status */
	nrf_wifi_ps_enabled = nrf_wifi_ps_enabled ? 0 : 1;
	return 0;
}

int wifi_set_ps_wakeup_mode()
{
	struct net_if *iface = net_if_get_default();

	/* STEP 6.2 - Define the Wi-Fi power save parameters structure */
	struct wifi_ps_params ps_params = {0};

	/* STEP 6.3 - Check and toggle the current wakeup mode */
	if (nrf_wifi_ps_wakeup_mode) {
		ps_params.wakeup_mode = WIFI_PS_WAKEUP_MODE_DTIM;
	} else {
		ps_params.wakeup_mode = WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL;
	}

	/* STEP 6.4 - Set the request type to wakeup mode. */
	ps_params.type = WIFI_PS_PARAM_WAKEUP_MODE;

	/* STEP 6.5 - Send the wakeup mode request */
	
	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_params, sizeof(ps_params))) {
		LOG_ERR("Setting wakeup mode failed. Reason %s",
			wifi_ps_get_config_err_code_str(ps_params.fail_reason));
		return -1;
	}
	LOG_INF("Set wakeup mode: %s", ps_params.wakeup_mode ? "Listen interval" : "DTIM");

	/* STEP 6.6 - Toggle the wakeup mode status */
	nrf_wifi_ps_wakeup_mode = nrf_wifi_ps_wakeup_mode ? 0 : 1;

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	if (button & DK_BTN1_MSK) {
		/* STEP 3.1 - When button 1 is pressed, enable or disable power save mode */
		//wifi_set_power_state();

		/* STEP 6.7 - When button 1 is pressed, enable or disable wakeup mode */
		wifi_set_ps_wakeup_mode();
	}


	if (button & DK_BTN2_MSK) {
		/* STEP 3.2 - When button 2 is pressed, alternate sending a PUT or GET request */
		if (http_put) {
			if (server_connect() >= 0) {
				client_http_put();
				counter++;
			}
		} else {
			if (server_connect() >= 0) {
				client_http_get();
			}
		}

		http_put = http_put ? 0 : 1;
	}
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

	LOG_INF("Connecting to %s:%s", CONFIG_HTTP_SAMPLE_HOSTNAME, CONFIG_HTTP_SAMPLE_PORT);
	if (server_connect() != 0) {
		LOG_ERR("Failed to connect to server");
		return 0;
	}

	LOG_INF("Succesfully connected to HTTP server");

	if (client_get_new_id() < 0) {
		LOG_ERR("Failed to get client ID");
		return 0;
	}

	close(sock);
	return 0;
}