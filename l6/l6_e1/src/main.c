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
#include <zephyr/net/http/client.h>

LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

#define HTTP_HOSTNAME "echo.thingy.rocks"
#define HTTP_PORT     80

#define RECV_BUF_SIZE  2048
#define CLIENT_ID_SIZE 36

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
static K_SEM_DEFINE(run_app, 0, 1);

static char recv_buf[RECV_BUF_SIZE];
static char client_id_buf[CLIENT_ID_SIZE + 2];

static int counter = 0;

static int sock;
static struct sockaddr_storage server;

/* STEP 1 - Create two variables to keep track of the power save status and PUT/GET request. */


/* STEP 5.1 - Create a variables to keep track of the power save wakeup mode. */

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

static int server_resolve(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(HTTP_HOSTNAME, STRINGIFY(HTTP_PORT), &hints, &result);
	if (err != 0) {
		LOG_INF("getaddrinfo failed, err: %d, %s", err, gai_strerror(err));
		return -EIO;
	}

	if (result == NULL) {
		LOG_INF("Error, address not found");
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

static void response_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data)
{	
	LOG_INF("Response status: %s", rsp->http_status);

	if (rsp->body_frag_len > 0) {
		char body_buf[rsp->body_frag_len];
		strncpy(body_buf, rsp->body_frag_start, rsp->body_frag_len);
		body_buf[rsp->body_frag_len] = '\0';
		LOG_INF("Received: %s", body_buf);
	} 
}

static void client_id_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data)
{
	LOG_INF("Response status: %s", rsp->http_status);
	
	char client_id_buf_tmp[CLIENT_ID_SIZE + 1];
	strncpy(client_id_buf_tmp, rsp->body_frag_start, CLIENT_ID_SIZE);
	client_id_buf_tmp[CLIENT_ID_SIZE] = '\0';
	client_id_buf[0] = '/';
	strcat(client_id_buf, client_id_buf_tmp);

	LOG_INF("Successfully acquired client ID: %s", client_id_buf);
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
	req.host = HTTP_HOSTNAME;
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
	close(sock);
	return err;
}

static int client_http_get(void)
{
	int err = 0;
	struct http_request req;
	const char *headers[] = {"Connection: close\r\n", NULL};
	memset(&req, 0, sizeof(req));
	req.header_fields = headers;
	req.method = HTTP_GET;
	req.url = client_id_buf;
	req.host = HTTP_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = response_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	LOG_INF("HTTP GET request");
	err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		LOG_ERR("Failed to send HTTP GET request, err: %d", err);
	}
	close(sock);
	return err;
}

static int client_get_new_id(void)
{
	int err = 0;
	const char *headers[] = {"Connection: close\r\n", NULL};
	struct http_request req;
	memset(&req, 0, sizeof(req));

	req.header_fields = headers;
	req.method = HTTP_POST;
	req.url = "/new";
	req.host = HTTP_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = client_id_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	err = http_client_req(sock, &req, 5000, NULL);
	close(sock);
	return err;
}

int wifi_set_power_state()
{
	struct net_if *iface = net_if_get_first_wifi();

	/* STEP 2.1 - Define the Wi-Fi power save parameters struct wifi_ps_params. */
	

	/* STEP 2.2 - Create an if statement to check if power saving is currently enabled.
	 * If it is not currently enabled, set the wifi_ps_params enabled parameter to WIFI_PS_ENABLED to enable power saving.
	 * If it is enabled, set enabled to WIFI_PS_DISABLED to disable power saving. */
	

	/* STEP 2.3 - Send the power save request with net_mgmt. */
	
	
	LOG_INF("Set power save: %s", params.enabled ? "enable" : "disable");
	
	/* STEP 2.4 - Toggle the value of nrf_wifi_ps_enabled to indicate the new power save status. */
	
	
	return 0;
}

int wifi_set_ps_wakeup_mode()
{
	struct net_if *iface = net_if_get_default();

	/* STEP 5.2 - Define a new wifi_ps_params struct for the wakeup mode request. */
	

	/* STEP 5.3 - Create an if statement to check the wakeup mode.
	 * If nrf_wifi_ps_wakeup_mode is true, the wakeup mode is listen interval and we want to change it to DTIM.
	 * If nrf_wifi_ps_wakeup_mode is false, the wakeup mode is DTIM and we want to change it to listen interval. */
	

	/* STEP 5.4 - Set the request type to wakeup mode. */
	

	/* STEP 5.5 - Send the wakeup mode request with net_mgmt like we did in STEP 2. */


	/* STEP 5.6 - Toggle the value of nrf_wifi_ps_wakeup_mode to indicate the wakeup mode. */
	

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;
	
	
	if (button & DK_BTN1_MSK) {
		/* STEP 3.1 - Call wifi_set_power_state() when button 1 is pressed. */

		/* STEP 5.7 - Modify button_handler to call wifi_set_ps_wakeup_mode() instead of wifi_set_power_state() when button 1 is pressed. */
		
	}

	
	if (button & DK_BTN2_MSK) {
		/* STEP 3.2 - When button 2 is pressed, if http_put is true, call client_http_put() and increase the counter variable with 1. 
	 	* If http_put is false, call client_http_get() instead. */

		/* STEP 3.3 - Toggle the value of http_put. */
		
	}
}

int main(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	LOG_INF("Waiting to connect to Wi-Fi");
	k_sem_take(&run_app, K_FOREVER);

	if (server_resolve() != 0) {
		LOG_ERR("Failed to resolve server name");
		return 0;
	}

	if (server_connect() != 0) {
		LOG_ERR("Failed to initialize client");
		return 0;
	}
	LOG_INF("Successfully connected to HTTP server");

	if (client_get_new_id() < 0) {
		LOG_ERR("Failed to get client ID");
		return 0;
	}

	close(sock);
	return 0;
}