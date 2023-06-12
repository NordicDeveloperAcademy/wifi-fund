/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/net/wifi_mgmt.h>
#include <dk_buttons_and_leds.h>

LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static struct net_mgmt_event_callback wifi_connect_cb;

static int wifi_args_to_params(struct wifi_connect_req_params *params)
{

	params->ssid = CONFIG_WIFI_SSID;
	params->ssid_length = strlen(params->ssid);

	params->psk = CONFIG_WIFI_PASSWORD;
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
		LOG_INF("Connected to Wi-Fi Network: %s", CONFIG_WIFI_SSID);
        dk_set_led_on(DK_LED1);
		k_sem_give(&wifi_connected_sem);
		break;
	default:
		break;
	}
}

int main(void)
{
	int err;
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx_params;


	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	net_mgmt_init_event_callback(&wifi_connect_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_connect_cb);

	wifi_args_to_params(&cnx_params);

	if (iface == NULL) {
		LOG_ERR("Returned network interface is NULL");
		return -1;
	}

	err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params));
	if (err) {
		LOG_ERR("Connecting to Wi-Fi failed. Error: %d", err);
		return ENOEXEC;
	}
	LOG_INF("Waiting for Wi-Fi connection");
	return 0;
}