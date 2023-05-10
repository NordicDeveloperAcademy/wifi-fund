#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>

static int __wifi_args_to_params(struct wifi_connect_req_params *params)
{
	params->timeout = SYS_FOREVER_MS;

	/* SSID */
	params->ssid = CONFIG_SOCKET_SAMPLE_SSID;
	params->ssid_length = strlen(params->ssid);

#if defined(CONFIG_STA_KEY_MGMT_WPA2)
	params->security = 1;
#elif defined(CONFIG_STA_KEY_MGMT_WPA2_256)
	params->security = 2;
#elif defined(CONFIG_STA_KEY_MGMT_WPA3)
	params->security = 3;
#else
	params->security = 0;
#endif

#if !defined(CONFIG_STA_KEY_MGMT_NONE)
	params->psk = CONFIG_SOCKET_SAMPLE_PASSWORD;
	params->psk_length = strlen(params->psk);
#endif
	params->channel = WIFI_CHANNEL_ANY;

	/* MFP (optional) */
	params->mfp = WIFI_MFP_OPTIONAL;

	return 0;
}

int wifi_connect(void)
{
	struct net_if *iface = net_if_get_default();
    static struct wifi_connect_req_params cnx_params;

	__wifi_args_to_params(&cnx_params);

	if (iface == NULL) {
		LOG_ERR("Returned network interface is NULL");
		return -1;
	}

	int err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
    &cnx_params, sizeof(struct wifi_connect_req_params));

	if (err) {
		LOG_ERR("Connecting to Wi-Fi failed. error: %d", err);
		return ENOEXEC;
	}

    LOG_INF("Connection requested");
    return 0;
}