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

/* STEP 1.2 - Include the header file of the HTTP client library */
#include <zephyr/net/http/client.h>

LOG_MODULE_REGISTER(Lesson5_Exercise1, LOG_LEVEL_INF);

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 3 - Declare the necessary buffers for receiving messages */
#define RECV_BUF_SIZE  2048
#define CLIENT_ID_SIZE 36

static char recv_buf[RECV_BUF_SIZE];
static char client_id_buf[CLIENT_ID_SIZE + 2];

/* STEP 4 - Define the variable for the counter as 0 */
static int counter = 0;

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

	LOG_INF("Connected to server");
	return 0;
}

static void response_cb(struct http_response *rsp, enum http_final_call final_data, void *user_data)
{
	/* STEP 9 - Define the callback function to print the body */
	LOG_INF("Response status: %s", rsp->http_status);

	if (rsp->body_frag_len > 0) {
		char body_buf[rsp->body_frag_len];
		strncpy(body_buf, rsp->body_frag_start, rsp->body_frag_len);
		body_buf[rsp->body_frag_len] = '\0';
		LOG_INF("Received: %s", body_buf);
	}

	LOG_INF("Closing socket: %d", sock);
	close(sock);
}

static void client_id_cb(struct http_response *rsp, enum http_final_call final_data,
			 void *user_data)
{
	/* STEP 6.1 - Log the HTTP response status */
	LOG_INF("Response status: %s", rsp->http_status);

	/* STEP 6.2 - Retrieve and format the client ID */
	char client_id_buf_tmp[CLIENT_ID_SIZE + 1];
	strncpy(client_id_buf_tmp, rsp->body_frag_start, CLIENT_ID_SIZE);
	client_id_buf_tmp[CLIENT_ID_SIZE] = '\0';
	client_id_buf[0] = '/';
	strcat(client_id_buf, client_id_buf_tmp);

	LOG_INF("Successfully acquired client ID: %s", client_id_buf);

	/* STEP 6.3 - Close the socket */
	LOG_INF("Closing socket: %d", sock);
	close(sock);
}

static int client_http_put(void)
{
	/* STEP 7 - Define the function to send a PUT request to the HTTP server */
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
	/* STEP 8 - Define the function to send a GET request to the HTTP server */
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

	/* STEP 5.1 - Define the structure http_request and fill the block of memory */
	struct http_request req;
	memset(&req, 0, sizeof(req));

	/* STEP 5.2 - Populate the http_request structure */
	const char *headers[] = {"Connection: close\r\n", NULL};
	req.header_fields = headers;
	req.method = HTTP_POST;
	req.url = "/new";
	req.host = CONFIG_HTTP_SAMPLE_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = client_id_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	/* STEP 5.3 - Send the request to the HTTP server */
	LOG_INF("HTTP POST request");
	err = http_client_req(sock, &req, 5000, NULL);
	if (err < 0) {
		LOG_ERR("Failed to send HTTP POST request, err: %d", err);
	}
	return err;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 10 - Define the button handler to send requests upon button triggers */
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		if (server_connect() >= 0) {
			client_http_put();
			counter++;
		}
	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		if (server_connect() >= 0) {
			client_http_get();
		}
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

	/* STEP 11 - Retrieve the client ID upon connection */
	if (client_get_new_id() < 0) {
		LOG_ERR("Failed to get client ID");
		return 0;
	}

	/* STEP 12 - Send a PUT request to HTTP server */
	if (server_connect() >= 0) {
		client_http_put();
		counter++;
	}

	return 0;
}