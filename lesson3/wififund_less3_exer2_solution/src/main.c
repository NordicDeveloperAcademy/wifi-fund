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

/* STEP 2 - Include the header file for the zperf API */ 
#include <zephyr/net/zperf.h>


/* Just for using inet_ntop to print*/
#include <zephyr/net/socket.h>

LOG_MODULE_REGISTER(Lesson3_Exercise2, LOG_LEVEL_INF);

/* STEP 3.1 - Define the semaphores */
K_SEM_DEFINE(wifi_connected_sem, 0, 1);
K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

#define PEER_IPV4_ADDR "192.168.1.253"
#define PEER_PORT 5001

#define WIFI_ZPERF_PKT_SIZE 1024
#define WIFI_ZPERF_RATE 10000
#define WIFI_TEST_DURATION 20000

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
		k_sem_give(&wifi_connected_sem);
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

	/* STEP 3.2 - Take the semaphores */
	k_sem_take(&wifi_connected_sem, K_FOREVER);
	k_sem_take(&ipv4_obtained_sem, K_FOREVER);
	

	return 0;
}

K_SEM_DEFINE(udp_callback, 0, 1);

static void udp_upload_results_cb(enum zperf_status status,
			  struct zperf_results *result,
			  void *user_data)
{
	unsigned int client_rate_in_kbps;

	switch (status) {
	case ZPERF_SESSION_STARTED:
		LOG_INF("New UDP session started");
		break;
	case ZPERF_SESSION_FINISHED:
		LOG_INF("Wi-Fi benchmark: Upload completed!");
		if (result->client_time_in_us != 0U) {
			client_rate_in_kbps = (uint32_t)
				(((uint64_t)result->nb_packets_sent *
				  (uint64_t)result->packet_size * (uint64_t)8 *
				  (uint64_t)USEC_PER_SEC) /
				 ((uint64_t)result->client_time_in_us * 1024U));
		} else {
			client_rate_in_kbps = 0U;
		}
		/* print results */
		LOG_INF("Upload results:");
		LOG_INF("%u bytes in %u ms",
				(result->nb_packets_sent * result->packet_size),
				(result->client_time_in_us / USEC_PER_MSEC));
		LOG_INF("%u packets sent", result->nb_packets_sent);
		LOG_INF("%u packets lost", result->nb_packets_lost);
		LOG_INF("%u packets received", result->nb_packets_rcvd);
		k_sem_give(&udp_callback);
		break;
	case ZPERF_SESSION_ERROR:
		LOG_ERR("UDP session error");
		break;
	}
}


int main(void)
{
	int ret;

	// if (dk_leds_init() != 0) {
	// 	LOG_ERR("Failed to initialize the LED library");
	// }

	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	// if (dk_buttons_init(button_handler) != 0) {
	// 	LOG_ERR("Failed to initialize the buttons library");
	// }

    // #ifdef CLOCK_FEATURE_HFCLK_DIVIDE_PRESENT
	// nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
    // #endif

	struct zperf_upload_params params;

		/* Start Wi-Fi traffic */
	LOG_INF("Starting Wi-Fi benchmark: Zperf client");
	params.duration_ms = WIFI_TEST_DURATION;
	params.rate_kbps = WIFI_ZPERF_RATE;
	params.packet_size = WIFI_ZPERF_PKT_SIZE;
    // ret = net_addr_pton(AF_INET, PEER_IPV4_ADDR, &in4_addr_my);
	// parse_ipv4_addr(PEER_IPV4_ADDR,
	// 	&in4_addr_my);
	// net_sprint_ipv4_addr(&in4_addr_my.sin_addr);
    ret = net_addr_pton(AF_INET, PEER_IPV4_ADDR, &params.peer_addr);
	// memcpy(&params.peer_addr, &in4_addr_my, sizeof(in4_addr_my));

    /* Printing server address, for debugging only */
    char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &params.peer_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 address of server: %s", ipv4_addr);

	ret = zperf_udp_upload_async(&params, udp_upload_results_cb, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to start Wi-Fi benchmark: %d\n", ret);
		return ret;
	}

	/* Run Wi-Fi traffic */
	if (k_sem_take(&udp_callback, K_FOREVER) != 0) {
		LOG_ERR("Results are not ready");
	} else {
		LOG_INF("UDP SESSION FINISHED");
	}

	while (1) {
	 	}

	return 0;
}