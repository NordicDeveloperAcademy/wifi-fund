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
#include <net/wifi_mgmt_ext.h>

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <zephyr/net/http/client.h>


LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

#define HTTP_HOSTNAME "d1jglomgqgmujc.cloudfront.net"
#define HTTP_PORT 80

#define RECV_BUF_SIZE 2048
#define CLIENT_ID_SIZE 36

static char recv_buf[RECV_BUF_SIZE];
static char client_id_buf[CLIENT_ID_SIZE+2];

static int counter = 0;

static int sock;
static struct sockaddr_storage server;

bool nrf_wifi_ps_enabled = 1;
bool http_put = 1;

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
		k_sem_give(&wifi_connected_sem);
		nrf_wifi_ps_enabled = 0;
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
	server4->sin_addr.s_addr = 
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = ((struct sockaddr_in *)result->ai_addr)->sin_port;

	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);

	freeaddrinfo(result);

	return 0;
}

static int server_connect(void)
{
	int err;
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_INF("Failed to create socket, err: %d, %s", errno, strerror(errno));
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_INF("Connecting to server failed, err: %d, %s", errno, strerror(errno));
		return -errno;

	}
	LOG_INF("Successfully connected to HTTP server");

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
		LOG_INF("Received: %s", body_buf);
	} 
}

static void client_id_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data)
{
	LOG_INF("Response status: %s", rsp->http_status);
	
	char client_id_buf_tmp[CLIENT_ID_SIZE+1];
	strncpy(client_id_buf_tmp, rsp->body_frag_start, CLIENT_ID_SIZE);
	client_id_buf_tmp[CLIENT_ID_SIZE]='\0';
	client_id_buf[0]='/';
	strcat(client_id_buf,client_id_buf_tmp);

	LOG_INF("Succesfully aquired client ID: %s", client_id_buf);
}


static int client_http_put(void)
{
	int err = 0;
	int bytes_written;

	struct http_request req;
	memset(&req, 0, sizeof(req));

	char buffer[12] = {0};
	bytes_written = snprintf(buffer, 12, "%d", counter);
	if (bytes_written < 0){
		LOG_INF("Unable to write to buffer, err: %d", bytes_written);
		return bytes_written;
	}

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
		LOG_INF("Failed to send HTTP PUT request %s, err: %d", buffer, err);
	}
	
	return err;
}

static int client_http_get(void)
{
	int err = 0;
	struct http_request req;

	memset(&req, 0, sizeof(req));

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
		LOG_INF("Failed to send HTTP GET request, err: %d", err);
	}
	
	return err;
}

static int client_get_new_id(void){
	int err = 0;

	struct http_request req;
	memset(&req, 0, sizeof(req));

	req.method = HTTP_POST;
	req.url = "/new";
	req.host = HTTP_HOSTNAME;
	req.protocol = "HTTP/1.1";
	req.response = client_id_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	err = http_client_req(sock, &req, 5000, NULL);

	return err;
}

int wifi_set_power_state()
{
	struct net_if *iface = net_if_get_default();
	struct wifi_ps_params params = { 0 };

	if (nrf_wifi_ps_enabled) {
		params.enabled = WIFI_PS_ENABLED;
	}
	else {
		params.enabled = WIFI_PS_DISABLED;
	}

	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params))) {
		LOG_ERR("Power save %s failed. Reason %s", params.enabled ? "enable" : "disable", get_ps_config_err_code_str(params.fail_reason));
		return -1;
	}
	LOG_INF("Set power save: %s", params.enabled ? "enable" : "disable");
	
	nrf_wifi_ps_enabled = nrf_wifi_ps_enabled ? 0 : 1;
	return 0;
}

int wifi_set_ps_wakeup_mode(){
	struct net_if *iface = net_if_get_default();
	struct wifi_ps_params params = { 0 };

	params.type = WIFI_PS_PARAM_WAKEUP_MODE;
	params.wakeup_mode = WIFI_PS_WAKEUP_MODE_DTIM;
	/* Extended/listen interval wakeup mode */
	//params.wakeup_mode = WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL;
	
	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params))) {
		LOG_ERR("Setting wakeup mode failed. Reason %s", get_ps_config_err_code_str(params.fail_reason));
		return -1;
	}
	LOG_INF("Wakeup mode set");
}


static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	if (button & DK_BTN1_MSK) {
		wifi_set_power_state();
	}

	if (button & DK_BTN2_MSK) {
		if (http_put) {
			client_http_put();
			counter++;
		}
		else {
			client_http_get();
		}
		http_put = http_put ? 0 : 1;
	}
}

int main(void)
{
	int ret;
	
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	wifi_set_ps_wakeup_mode();
	wifi_set_power_state();

	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return 0;
	}

	if (server_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	if (client_get_new_id() < 0) {
		LOG_INF("Failed to get client ID");
		return 0;
	}

	while (1) {
		k_sleep(K_FOREVER);
	}

	close(sock);
	return 0;
}