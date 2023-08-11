/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/random/rand32.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>

/* STEP 1.3 - Include the header file for the MQTT library */
#include <zephyr/net/mqtt.h>

LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

K_SEM_DEFINE(wifi_connected_sem, 0, 1);
K_SEM_DEFINE(ipv4_obtained_sem, 0, 1);

#define WIFI_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
				NET_EVENT_WIFI_DISCONNECT_RESULT)

#define IPV4_MGMT_EVENTS (NET_EVENT_IPV4_ADDR_ADD | \
				NET_EVENT_IPV4_ADDR_DEL)

#define MQTT_CLIENT_ID "WiFi_Fund_Course_Less4Exer1"
#define CLIENT_ID_LEN sizeof(CONFIG_BOARD) + 11

static struct net_mgmt_event_callback wifi_mgmt_cb;
static struct net_mgmt_event_callback ipv4_mgmt_cb;

static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

static struct sockaddr_storage server;
static struct mqtt_client client;
static struct pollfd fds;

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

	err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
	if (err) {
		LOG_INF("getaddrinfo failed, err: %d, %s", err, gai_strerror(err));
		return -ECHILD;
	}

	if (result == NULL) {
		LOG_INF("Error, address not found");
		return -ENOENT;
	}

	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);
	server4->sin_addr.s_addr =
			((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr, 
		sizeof(ipv4_addr));
	LOG_INF("IPv4 address of MQTT broker found %s", ipv4_addr);
	
	freeaddrinfo(result);
	return err;
}

static int get_received_payload(struct mqtt_client *c, size_t length)
{
	int ret;
	int err = 0;

	if (length > sizeof(payload_buf)) {
		err = -EMSGSIZE;
	}

	/* Truncate payload until it fits in the payload buffer. */
	while (length > sizeof(payload_buf)) {
		ret = mqtt_read_publish_payload_blocking(
				c, payload_buf, (length - sizeof(payload_buf)));
		if (ret == 0) {
			return -EIO;
		} else if (ret < 0) {
			return ret;
		}

		length -= ret;
	}

	ret = mqtt_readall_publish_payload(c, payload_buf, length);
	if (ret) {
		return ret;
	}

	return err;
}

static int subscribe(struct mqtt_client *const c)
{
	/* STEP 3.1 - Declare a variable of type mqtt_topic */

	/* STEP 3.2 - Define a subscription list */

	/* STEP 3.3 - Subscribe to the topics */

}

static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	LOG_INF("%s%s", (char *)prefix, (char *)buf);
}

/* STEP 6 - Define the function to publish data */


void mqtt_evt_handler(struct mqtt_client *const c,
		      const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
	/* STEP 4 - Upon a successful connection, subscribe to topics */


	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected: %d", evt->result);
		break;

	case MQTT_EVT_PUBLISH:
	/* STEP 5 - Listen to published messages received from the broker and extract the message */
	{
		/* STEP 5.1 - Extract the payload and (if relevant) send acknowledgement */


	/* STEP 5.2 - On successful extraction of data, exmaine command and toggle LED accordingly */


	/* STEP 5.3 - On failed extraction of data, examine error code */

	} break;
	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error: %d", evt->result);
			break;
		}
		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
		break;
	case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT SUBACK error: %d", evt->result);
			break;
		}
		LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
		break;
	case MQTT_EVT_PINGRESP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PINGRESP error: %d", evt->result);
		}
		break;
	default:
		LOG_INF("Unhandled MQTT event type: %d", evt->type);
		break;
	}
}

static const uint8_t* client_id_get(void)
{
	static uint8_t client_id[MAX(sizeof(MQTT_CLIENT_ID),
				     CLIENT_ID_LEN)];

	if (strlen(MQTT_CLIENT_ID) > 0) {
		snprintf(client_id, sizeof(client_id), "%s",
			 MQTT_CLIENT_ID);
		goto exit;
	}

	uint32_t id = sys_rand32_get();
	snprintf(client_id, sizeof(client_id), "%s-%010u", CONFIG_BOARD, id);

exit:
	LOG_DBG("client_id = %s", (char *)client_id);

	return client_id;
}


int client_init(struct mqtt_client *client)
{
	int err;
	/* STEP 2.1 - Initialize the client instance */

	/* STEP 2.2 - Resolve the configured hostname and initializes the MQTT broker structure */

	/* STEP 2.3 - MQTT client configuration */

	/* STEP 2.4 - MQTT buffers configuration */

	/* STEP 2.5 - Set the transport type to non-secure */

	return err;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		/* STEP 7.1 - When button 1 is pressed, send a message */

	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		/* STEP 7.2 - When button 2 is pressed, send a message */	

	}
}

int main(void)
{
	int err;
	uint32_t connect_attempt = 0;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}
	
	if (wifi_connect() != 0) {
		LOG_ERR("Failed to connect to Wi-Fi");
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}
	
	LOG_INF("Connecting to MQTT broker");

	err = client_init(&client);
	if (err) {
		LOG_ERR("Failed to initialize MQTT client: %d", err);
		return err;
	}

do_connect:
	if (connect_attempt++ > 0) {
		LOG_INF("Reconnecting in 60 seconds...");
		k_sleep(K_SECONDS(60));
	}

	/* STEP 9 - Establish a connection the MQTT broker */

	/* STEP 9.1 - Configure fds to monitor the socket */

	while (1) {
		/* STEP 9.2 - Continously poll the socket for incoming data */

		/* STEP 9.3 - In the event of incoming data, process it */

	}

	LOG_INF("Disconnecting MQTT client");

	err = mqtt_disconnect(&client);
	if (err) {
		LOG_ERR("Could not disconnect MQTT client: %d", err);
	}
	goto do_connect;
}