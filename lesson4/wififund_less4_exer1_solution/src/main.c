/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <net/wifi_credentials.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <dk_buttons_and_leds.h>

#include "mqtt_connection.h"

LOG_MODULE_REGISTER(MQTT_OVER_WIFI, LOG_LEVEL_INF);

/* The mqtt client struct */
static struct mqtt_client client;
/* File descriptor */
static struct pollfd fds;
static struct net_mgmt_event_callback wifi_prov_cb;

static void get_wifi_credential(void *cb_arg, const char *ssid, size_t ssid_len)
{
struct wifi_credentials_personal config;

	wifi_credentials_get_by_ssid_personal_struct(ssid, ssid_len, &config);
	memcpy((struct wifi_credentials_personal *)cb_arg, &config, sizeof(config));
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
	/* STEP 7.2 - When button 1 is pressed, call data_publish() to publish a message */
		if (button_state & DK_BTN1_MSK){	
			int err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				   CONFIG_BUTTON1_EVENT_PUBLISH_MSG, sizeof(CONFIG_BUTTON1_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	case DK_BTN2_MSK:
		if (button_state & DK_BTN2_MSK){	
			int err = data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				   CONFIG_BUTTON2_EVENT_PUBLISH_MSG, sizeof(CONFIG_BUTTON2_EVENT_PUBLISH_MSG)-1);
			if (err) {
				LOG_ERR("Failed to send message, %d", err);
				return;	
			}
		}
		break;
	}
}

static void connect_mqtt(void)
{
	int err;
	uint32_t connect_attempt = 0;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	err = client_init(&client);
	if (err) {
		LOG_ERR("Failed to initialize MQTT client: %d", err);
		return;
	}

do_connect:
	if (connect_attempt++ > 0) {
		LOG_INF("Reconnecting in %d seconds...",
			CONFIG_MQTT_RECONNECT_DELAY_S);
		k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
	}
	err = mqtt_connect(&client);
	if (err) {
		LOG_ERR("Error in mqtt_connect: %d", err);
		goto do_connect;
	}

	err = fds_init(&client,&fds);
	if (err) {
		LOG_ERR("Error in fds_init: %d", err);
		return;
	}

	while (1) {
		err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
		if (err < 0) {
			LOG_ERR("Error in poll(): %d", errno);
			break;
		}

		err = mqtt_live(&client);
		if ((err != 0) && (err != -EAGAIN)) {
			LOG_ERR("Error in mqtt_live: %d", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				LOG_ERR("Error in mqtt_input: %d", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			LOG_ERR("POLLERR");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			LOG_ERR("POLLNVAL");
			break;
		}
	}

	LOG_INF("Disconnecting MQTT client");

	err = mqtt_disconnect(&client);
	if (err) {
		LOG_ERR("Could not disconnect MQTT client: %d", err);
	}
	goto do_connect;
}

static void wifi_connect_handler(struct net_mgmt_event_callback *cb,
				    uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		LOG_INF("Connected to a Wi-Fi Network");
		break;
	default:
		break;
	}
}
void main(void)
{
	int rc;
	struct wifi_credentials_personal config = { 0 };
	struct net_if *iface = net_if_get_default();
	struct wifi_connect_req_params cnx_params = { 0 };

	/* Sleep 1 seconds to allow initialization of wifi driver. */
	k_sleep(K_SECONDS(1));

	/* Search for stored wifi credential and apply */
	wifi_credentials_for_each_ssid(get_wifi_credential, &config);
	LOG_INF("Code stops before configuration found.\n");
	if (config.header.ssid_len > 0) {
		LOG_INF("Configuration found. Try to apply.\n");

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
		rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
			&cnx_params, sizeof(struct wifi_connect_req_params));
		if (rc < 0) {
			LOG_ERR("Cannot apply saved Wi-Fi configuration, err = %d.\n", rc);
		} else {
			LOG_INF("Configuration applied.\n");
		}
	}
	net_mgmt_init_event_callback(&wifi_prov_cb,
				     wifi_connect_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT);

	net_mgmt_add_event_callback(&wifi_prov_cb);
	/* Wait for the interface to be up */
	k_sleep(K_SECONDS(6));
	LOG_INF("Connecting to MQTT Broker...");
	/* Connect to MQTT Broker */
	connect_mqtt();
}