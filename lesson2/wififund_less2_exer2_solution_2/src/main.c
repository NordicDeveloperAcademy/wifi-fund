/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <net/wifi_credentials.h>
#include <dk_buttons_and_leds.h>


LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
static struct net_mgmt_event_callback wifi_connect_cb;

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

static void get_wifi_credential(void *cb_arg, const char *ssid, size_t ssid_len)
{
	struct wifi_credentials_personal config;
	LOG_INF("Getting Wi-Fi credentials");
	int err = wifi_credentials_get_by_ssid_personal_struct(ssid, ssid_len, &config);
	if (err < 0) {
		LOG_INF("Failed to get credentials, err = %d.", err);
	}
	else {LOG_INF("Succesfully got credentials");}

	memcpy((struct wifi_credentials_personal *)cb_arg, &config, sizeof(config));
}

int main(void)
{
	int err;
	struct wifi_credentials_personal config = { 0 };
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx_params;


	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));


	//oonfig.header
	// This is callback for each registered SSID, so it's never called because there are no registered SSID's
	wifi_credentials_for_each_ssid(get_wifi_credential, &config);
	LOG_INF("Looking for configuration to apply.");
	if (config.header.ssid_len > 0) {
		LOG_INF("Configuration found. Try to apply.");

		cnx_params.ssid = config.header.ssid;
		cnx_params.ssid_length = config.header.ssid_len;
		cnx_params.security = config.header.type;

		cnx_params.psk = NULL;
		cnx_params.psk_length = 0;
		cnx_params.sae_password = NULL;
		cnx_params.sae_password_length = 0;

		if (config.header.type != WIFI_SECURITY_TYPE_NONE) {
			cnx_params.psk = config.password;
			cnx_params.psk_length = config.password_len;
		}

		cnx_params.channel = WIFI_CHANNEL_ANY;
		cnx_params.band = config.header.flags & WIFI_CREDENTIALS_FLAG_5GHz ?
				WIFI_FREQ_BAND_5_GHZ : WIFI_FREQ_BAND_2_4_GHZ;
		cnx_params.mfp = WIFI_MFP_OPTIONAL;
		err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
			&cnx_params, sizeof(struct wifi_connect_req_params));
		if (err < 0) {
			LOG_ERR("Cannot apply saved Wi-Fi configuration, err = %d.", err);
		} else {
			LOG_INF("Configuration applied.");
		}
	}


	net_mgmt_init_event_callback(&wifi_connect_cb, wifi_connect_handler, NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_connect_cb);
	k_sem_take(&wifi_connected_sem, K_FOREVER);

	LOG_INF("Waiting for Wi-Fi connection");
	return 0;
}