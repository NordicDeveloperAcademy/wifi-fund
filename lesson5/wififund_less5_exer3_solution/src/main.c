/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_pkt.h>

LOG_MODULE_REGISTER(Lesson5_Exercise3, LOG_LEVEL_INF);
K_SEM_DEFINE(wifi_connected_sem, 0, 1);

#define HTTP_BIND_PORT 8080

static int serv;

#ifndef USE_BIG_PAYLOAD
#define USE_BIG_PAYLOAD 1
#endif

#define CHECK(r) { if (r == -1) { printf("Error: " #r "\n"); exit(1); } }

static const char content[] = {
#if USE_BIG_PAYLOAD
    #include "response_big.html.bin.inc"
#else
    #include "response_small.html.bin.inc"
#endif
};

static struct net_mgmt_event_callback wifi_connect_cb;

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

static void wifi_connect_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to Wi-Fi Network: %s", CONFIG_WIFI_CREDENTIALS_STATIC_SSID);
        dk_set_led_on(DK_LED1);
		k_sem_give(&wifi_connected_sem);
		break;
	default:
		break;
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

	net_mgmt_init_event_callback(&wifi_connect_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_connect_cb);

	wifi_args_to_params(&cnx_params);

	LOG_INF("Connecting to Wi-Fi");
	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));
	if (err) {
		LOG_ERR("Connecting to Wi-Fi failed, err: %d", err);
		return ENOEXEC;
	}
	k_sem_take(&wifi_connected_sem, K_FOREVER);
	k_sleep(K_SECONDS(6));

	return 0;
}

static int server_setup() 
{

	serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serv < 0) {
		LOG_INF("Failed to set up HTTP socket, err: %d, %s", errno, strerror(errno));
		return -errno;	
	}

	struct sockaddr_in bind_addr;
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(HTTP_BIND_PORT);
	int err = bind(serv, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
	if (err < 0) {
		LOG_INF("Failed to bind to HTTP socket, err: %d", errno);
		return -errno;
	}

	err = listen(serv, 5);
	if (err < 0) {
		LOG_INF("listen() failed, err: %d", errno);
		return -errno;
	}

	LOG_INF("HTTP server waits for a connection on port %d", HTTP_BIND_PORT);

	return 0;
}

int main(void)
{
	static int counter;
	int ret;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}
	
	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	if (server_setup() != 0){
		LOG_ERR("Failed to setup HTTP server");
		return -1;
	}

	
	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		char addr_str[32];
		int req_state = 0;
		const char *data;
		size_t len;

		int client = accept(serv, (struct sockaddr *)&client_addr,
				    &client_addr_len);
		if (client < 0) {
			LOG_INF("Error in accept: %d - continuing", errno);
			continue;
		}

		inet_ntop(client_addr.sin_family, &client_addr.sin_addr,
			  addr_str, sizeof(addr_str));
		LOG_INF("Connection #%d from %s\n", counter++, addr_str);

		/* Discard HTTP request (or otherwise client will get
		 * connection reset error).
		 */
		while (1) {
			ssize_t r;
			char c;

			r = recv(client, &c, 1, 0);
			if (r == 0) {
				goto close_client;
			}

			if (r < 0) {
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				}

				LOG_INF("Got error %d when receiving from "
				       "socket", errno);
				goto close_client;
			}
			if (req_state == 0 && c == '\r') {
				req_state++;
			} else if (req_state == 1 && c == '\n') {
				req_state++;
			} else if (req_state == 2 && c == '\r') {
				req_state++;
			} else if (req_state == 3 && c == '\n') {
				break;
			} else {
				req_state = 0;
			}
		}

		data = content;
		len = sizeof(content);
		while (len) {
			int sent_len = send(client, data, len, 0);

			if (sent_len == -1) {
				LOG_INF("Error sending data to peer, err: %d", errno);
				break;
			}
			data += sent_len;
			len -= sent_len;
		}
close_client:
		ret = close(client);
		if (ret == 0) {
			LOG_INF("Connection from %s closed", addr_str);
		} else {
			LOG_INF("Got error %d while closing socket", errno);
		}
	}
	return 0;
}
