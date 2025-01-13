/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
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

#include <zephyr/net/conn_mgr_monitor.h>

/* STEP 3 - Include Zephyr's HTTP parser header file */
#include <zephyr/net/http/parser.h>

LOG_MODULE_REGISTER(Lesson5_Exercise3, LOG_LEVEL_INF);

/* STEP 4 - Define the port number for the server */
#define SERVER_PORT 8080

#define MAX_CLIENT_QUEUE 2
#define STACK_SIZE	 4096
#define THREAD_PRIORITY	 K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#define EVENT_MASK	 (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

/* STEP 5 - Define a struct to represent the structure of HTTP requests */
struct http_req {
	struct http_parser parser;
	int socket;
	bool received_all;
	enum http_method method;
	const char *url;
	size_t url_len;
	const char *body;
	size_t body_len;
};

/* Forward declarations */
static void process_tcp4(void);

/* Keep track of the current LED states. 0 = LED OFF, 1 = LED ON.
 * Index 0 corresponds to LED1, index 1 to LED2.
 */
static uint8_t led_states[2];

/* STEP 6 - Define HTTP server response strings for demonstration */
static const char response_200[] = "HTTP/1.1 200 OK\r\n";
static const char response_403[] = "HTTP/1.1 403 Forbidden\r\n\r\n";
static const char response_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";

/* STEP 7 - Set up threads for handling multiple incoming TCP connections simultaneously */
K_THREAD_STACK_ARRAY_DEFINE(tcp4_handler_stack, MAX_CLIENT_QUEUE, STACK_SIZE);
static struct k_thread tcp4_handler_thread[MAX_CLIENT_QUEUE];
static k_tid_t tcp4_handler_tid[MAX_CLIENT_QUEUE];
K_THREAD_DEFINE(tcp4_thread_id, STACK_SIZE, process_tcp4, NULL, NULL, NULL, THREAD_PRIORITY, 0, -1);

/* STEP 8 - Declare and initialize structs and variables used in network management */
static int tcp4_listen_sock;
static int tcp4_accepted[MAX_CLIENT_QUEUE];

/* STEP 11.1 - Define a variable to store the HTTP parser settings */
static struct http_parser_settings parser_settings;

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
		k_sem_give(&run_app);

		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_INF("Waiting for network to be connected");
		} else {
			LOG_INF("Network disconnected");
			connected = false;
		}

		k_sem_reset(&run_app);

		return;
	}
}

/* STEP 9 -  Define a function to update the LED state */
static bool handle_led_update(struct http_req *request, size_t led_id)
{
	char new_state_tmp[2] = {0};
	uint8_t new_state;
	size_t led_index = led_id - 1;

	if (request->body_len < 1) {
		return false;
	}

	memcpy(new_state_tmp, request->body, 1);

	new_state = atoi(request->body);

	if (new_state <= 1) {
		led_states[led_index] = new_state;
	} else {
		LOG_WRN("Attempted to update LED%d state to illegal value %d", led_id,
			new_state);
		return false;
	}

	(void)dk_set_led(led_index, led_states[led_index]);

	LOG_INF("LED state updated to %d", led_states[led_index]);

	return true;
}

/* STEP 10 -   Define a function to handle HTTP requests */
static void handle_http_request(struct http_req *request)
{
	size_t len;
	const char *resp_ptr = response_403;
	char dynamic_response_buf[100];
	char url[100];
	size_t url_len = MIN(sizeof(url) - 1, request->url_len);

	memcpy(url, request->url, url_len);

	url[url_len] = '\0';

	if (request->method == HTTP_PUT) {
		size_t led_id;
		int ret = sscanf(url, "/led/%u", &led_id);

		LOG_DBG("PUT %s", url);

		/* Handle PUT requests to the "led" resource. It is safe to use strcmp() because
		 * we know that both strings are null-terminated. Otherwise, strncmp would be used.
		 */
		if ((ret == 1) && (led_id > 0) && (led_id < (ARRAY_SIZE(led_states) + 1))) {
			if (handle_led_update(request, led_id)) {
				(void)snprintk(dynamic_response_buf, sizeof(dynamic_response_buf),
						"%s\r\n", response_200);
				resp_ptr = dynamic_response_buf;
			}
		} else {
			LOG_INF("Attempt to update unsupported resource '%s'", url);
			resp_ptr = response_403;
		}
	}

	if (request->method == HTTP_GET) {
		size_t led_id;
		int ret = sscanf(url, "/led/%u", &led_id);

		LOG_DBG("GET %s", url);

		/* Handle GET requests to the "led" resource */
		if ((ret == 1) && (led_id > 0) && (led_id < (ARRAY_SIZE(led_states) + 1))) {
			char body[2];
			size_t led_index = led_id - 1;

			(void)snprintk(body, sizeof(body), "%d", led_states[led_index]);

			(void)snprintk(dynamic_response_buf, sizeof(dynamic_response_buf),
				       "%sContent-Length: %d\r\n\r\n%s", response_200, strlen(body),
				       body);

			resp_ptr = dynamic_response_buf;
		} else {
			LOG_INF("Attempt to fetch unknown resource '%.*s'", request->url_len,
				request->url);

			resp_ptr = response_404;
		}
	}

	/* Get the total length of the HTTP response */
	len = strlen(resp_ptr);

	while (len) {
		ssize_t out_len;

		out_len = send(request->socket, resp_ptr, len, 0);

		if (out_len < 0) {
			LOG_ERR("Error while sending: %d", -errno);
			return;
		}

		resp_ptr = (const char *)resp_ptr + out_len;
		len -= out_len;
	}
}

/* STEP 11.2 - Define callbacks for the HTTP parser */
static int on_body(struct http_parser *parser, const char *at, size_t length)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->body = at;
	req->body_len = length;

	LOG_DBG("on_body: %d", parser->method);
	LOG_DBG("> %.*s", length, at);

	return 0;
}

static int on_headers_complete(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->method = parser->method;

	LOG_DBG("on_headers_complete, method: %s", http_method_str(parser->method));

	return 0;
}

static int on_message_begin(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->received_all = false;

	LOG_DBG("on_message_begin, method: %d", parser->method);

	return 0;
}

static int on_message_complete(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->received_all = true;

	LOG_DBG("on_message_complete, method: %d", parser->method);

	return 0;
}

static int on_url(struct http_parser *parser, const char *at, size_t length)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->url = at;
	req->url_len = length;

	LOG_DBG("on_url, method: %d", parser->method);
	LOG_DBG("> %.*s", length, at);

	return 0;
}

/* STEP 11.3 - Define function to initialize the callbacks */
static void parser_init(void)
{
	http_parser_settings_init(&parser_settings);

	parser_settings.on_body = on_body;
	parser_settings.on_headers_complete = on_headers_complete;
	parser_settings.on_message_begin = on_message_begin;
	parser_settings.on_message_complete = on_message_complete;
	parser_settings.on_url = on_url;
}

/* STEP 12 -  Setup the HTTP server */
static int setup_server(int *sock, struct sockaddr *bind_addr, socklen_t bind_addrlen)
{
	int ret;

	*sock = socket(bind_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);

	if (*sock < 0) {
		LOG_ERR("Failed to create TCP socket: %d", errno);
		return -errno;
	}

	ret = bind(*sock, bind_addr, bind_addrlen);
	if (ret < 0) {
		LOG_ERR("Failed to bind TCP socket %d", errno);
		return -errno;
	}

	ret = listen(*sock, MAX_CLIENT_QUEUE);
	if (ret < 0) {
		LOG_ERR("Failed to listen on TCP socket %d", errno);
		ret = -errno;
	}

	return ret;
}

/* STEP 13 -  Setup the TCP client connection handler */
static void client_conn_handler(void *ptr1, void *ptr2, void *ptr3)
{
	ARG_UNUSED(ptr1);
	int *sock = ptr2;
	k_tid_t *in_use = ptr3;
	int received;
	int ret;
	char buf[1024];
	size_t offset = 0;
	size_t total_received = 0;
	struct http_req request = {
		.socket = *sock,
	};

	http_parser_init(&request.parser, HTTP_REQUEST);

	while (1) {
		/* Receive TCP fragment. This is a naive implementation that blocks indefinitely.
		 * There is no timeout or mechanism to handle a client that just goes silent in the
		 * middle of an HTTP request. Errors such as incomplete or wrongly-formatted
		 * requests are not handled.
		 */
		received = recv(request.socket, buf + offset, sizeof(buf) - offset, 0);
		if (received == 0) {
			/* Connection closed */
			LOG_DBG("[%d] Connection closed by peer", request.socket);
			break;
		} else if (received < 0) {
			/* Socket error */
			ret = -errno;
			LOG_ERR("[%d] Connection error %d", request.socket, ret);
			break;
		}

		/* Parse the received data as HTTP request */
		(void)http_parser_execute(&request.parser, &parser_settings, buf + offset,
					  received);

		total_received += received;
		offset += received;

		if (offset >= sizeof(buf)) {
			offset = 0;
		}

		/* If the HTTP request has been completely received, stop receiving data and
		 * proceed to process the request.
		 */
		if (request.received_all) {
			handle_http_request(&request);
			break;
		}
	};

	(void)close(request.socket);

	*sock = -1;
	*in_use = NULL;
}

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

/* STEP 14 - Setup functions to handle incoming TCP connections */
static int process_tcp(int *sock, int *accepted)
{
	static int counter;
	int client;
	int slot;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char addr_str[INET_ADDRSTRLEN];

	client = accept(*sock, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client < 0) {
		LOG_ERR("Error in accept %d, stopping server", -errno);
		return -errno;
	}

	slot = get_free_slot(accepted);
	if (slot < 0 || slot >= MAX_CLIENT_QUEUE) {
		LOG_ERR("Cannot accept more connections");
		close(client);
		return 0;
	}

	accepted[slot] = client;

	if (client_addr.sin_family == AF_INET) {
		tcp4_handler_tid[slot] = k_thread_create(
			&tcp4_handler_thread[slot], tcp4_handler_stack[slot],
			K_THREAD_STACK_SIZEOF(tcp4_handler_stack[slot]),
			(k_thread_entry_t)client_conn_handler, INT_TO_POINTER(slot),
			&accepted[slot], &tcp4_handler_tid[slot], THREAD_PRIORITY, 0, K_NO_WAIT);
	}

	net_addr_ntop(client_addr.sin_family, &client_addr.sin_addr, addr_str, sizeof(addr_str));

	LOG_INF("[%d] Connection #%d from %s", client, ++counter, addr_str);

	return 0;
}

/* Processing incoming IPv4 clients */
static void process_tcp4(void)
{
	int ret;
	struct sockaddr_in addr4 = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
	};

	ret = setup_server(&tcp4_listen_sock, (struct sockaddr *)&addr4, sizeof(addr4));
	if (ret < 0) {
		return;
	}

	LOG_DBG("Waiting for IPv4 HTTP connections on port %d, sock %d", SERVER_PORT,
		tcp4_listen_sock);

	while (ret == 0) {
		ret = process_tcp(&tcp4_listen_sock, tcp4_accepted);
	}
}

/* STEP 15.1 - Define function to start listening on TCP socket */
void start_listener(void)
{
	for (size_t i = 0; i < MAX_CLIENT_QUEUE; i++) {
		tcp4_accepted[i] = -1;
		tcp4_listen_sock = -1;
	}
	k_thread_start(tcp4_thread_id);
}

int main(void)
{
	int err;

	parser_init();

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Failed to initialize LEDs");
		return 0;
	}

	net_mgmt_init_event_callback(&mgmt_cb, net_mgmt_event_handler, EVENT_MASK);
	net_mgmt_add_event_callback(&mgmt_cb);

	conn_mgr_mon_resend_status();

	/* Wait for the connection. */
	k_sem_take(&run_app, K_FOREVER);

	/* STEP 15.2 - Call the start_listener function */
	start_listener();

	(void)err;

	return 0;
}
