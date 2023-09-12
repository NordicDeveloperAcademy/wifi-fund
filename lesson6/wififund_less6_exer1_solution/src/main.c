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
#include <zephyr/net/socket.h>

#ifdef CONFIG_NET_SHELL
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

const struct shell *shell_backend;
#endif

LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

bool nrf_wifi_ps_enabled = 0;

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

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

int wifi_set_power_state(int wakeup_mode)
{
	struct net_if *iface = net_if_get_default();
	struct wifi_ps_params params = { 0 };

	if (nrf_wifi_ps_enabled) {
		params.enabled = WIFI_PS_DISABLED;
	}
	else {
		params.enabled = WIFI_PS_ENABLED;
		params.wakeup_mode = wakeup_mode;
		// params.listen_interval = ;
		LOG_INF("Wakeup mode: %s", params.wakeup_mode ? "Extended (listen interval)" : "DTIM (legacy)");
	}


	if (net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params))) {
		LOG_ERR("Power save %s failed. Reason %s", params.enabled ? "enable" : "disable", get_ps_config_err_code_str(params.fail_reason));
		return -1;
	}
	LOG_INF("Set power save: %s", params.enabled ? "enable" : "disable");
	
	nrf_wifi_ps_enabled = nrf_wifi_ps_enabled ? 0 : 1;
	return 0;
}

//enum wifi_config_ps_param_fail_reason fail_reason;


static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t buttons = button_state & has_changed;

	if (buttons & DK_BTN1_MSK) {
		// DTIM/Legacy pc mode
		wifi_set_power_state(WIFI_PS_WAKEUP_MODE_DTIM);
	}
	if (buttons & DK_BTN2_MSK) {
		// Listen/Extended ps mode
		wifi_set_power_state(WIFI_PS_WAKEUP_MODE_LISTEN_INTERVAL);
	}
}

const struct shell *shell_backend;

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

	#ifdef CONFIG_NET_SHELL
	shell_backend = shell_backend_uart_get_ptr();
	#endif

	while (1) {
		dk_set_led(DK_LED2, 1);
		k_sleep(K_SECONDS(10));
		/* Scheduling the shell ping to main thread
		 * ie, a lower prio than workq
		 */
		// if (ping_cmd_recv) {
			#ifdef CONFIG_NET_SHELL		
			char ping_cmd[64] = "net ping 8.8.8.8";
			ret = shell_execute_cmd(shell_backend, ping_cmd);
			if (ret) {
				LOG_INF("shell error: %d\n", ret);
			}
			// ping_cmd_recv = false;
			dk_set_led(DK_LED2, 0);
			#endif			
		}

	return 0;
}