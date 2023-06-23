
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <net/wifi_mgmt_ext.h>

/* STEP x.x - Include the header file for the socket API */ 
#include <zephyr/net/socket.h>

/* STEP x.x - Define the hostname and port for the echo server */
#define SERVER_HOSTNAME "nordicecho.westeurope.cloudapp.azure.com"
#define SERVER_PORT "2444"

// #define SERVER_HOSTNAME "tcpbin.com"
// #define SERVER_PORT "4242"

#define WIFI_SOCKET_MGMT_EVENTS (NET_EVENT_WIFI_CONNECT_RESULT | \
		    NET_EVENT_WIFI_DISCONNECT_RESULT)

#define MESSAGE_SIZE 256 
#define MESSAGE_TO_SEND "Hello from nRF70 Series"
#define SSTRLEN(s) (sizeof(s) - 1)

/* STEP x.x - Declare the structure for the socket and server address */
static int sock;
static struct sockaddr_storage server;

/* STEP x.x - Declare the buffer for receiving from server */
static uint8_t recv_buf[MESSAGE_SIZE];

K_SEM_DEFINE(wifi_connected, 0, 1);
K_SEM_DEFINE(ipv4_obtained, 0, 1);

LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);

static int server_resolve(void)
{
	/* STEP x.x - Call getaddrinfo() to get the IP address of the echo server */
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
	};

	err = getaddrinfo(SERVER_HOSTNAME, SERVER_PORT, &hints, &result);
	if (err != 0) {
		LOG_INF("ERROR: getaddrinfo failed %d", err);
		return -EIO;
	}

	if (result == NULL) {
		LOG_INF("ERROR: Address not found");
		return -ENOENT;
	}

	/* STEP x.x - Retrieve the relevant information from the result structure */
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server); 

	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = ((struct sockaddr_in *)result->ai_addr)->sin_port;

	/* STEP x.x - Convert the address into a string and print it */
	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);
	
	/* STEP x.x - Free the memory allocated for result */
	freeaddrinfo(result);

	return 0;
}

static int server_connect(void)
{
	int err;
	/* STEP x.x - Create a TCP socket */
	//sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP);
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create socket: %d.", errno);
		return -errno;
	}

	/* STEP x.x - Connect the socket to the server */
	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connect failed : %d", errno);
		return -errno;
	}
	LOG_INF("Successfully connected to server");

	return 0;
}

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback net_mgmt_ipv4_callback;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
{

    const struct wifi_status *wifi_status = (const struct wifi_status *)cb->info;

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		if (wifi_status->status) {
			LOG_INF("Connection attempt failed, status code: %d", wifi_status->status);
			return;
		}
        LOG_INF("Wi-Fi Connected, waiting for IP address");
		k_sem_give(&wifi_connected);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		LOG_INF("Disconnected");
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
		k_sem_give(&ipv4_obtained);
		break;
	case NET_EVENT_IPV4_ADDR_DEL:
		LOG_INF("IPv4 address lost");
		break;
	default:
		LOG_DBG("Unknown event: 0x%08X", event);
		return;
	}
}

static void wifi_register_cb(void)
{
	LOG_INF("Registering wifi events");
	net_mgmt_init_event_callback(&mgmt_cb,
				     wifi_mgmt_event_handler, WIFI_SOCKET_MGMT_EVENTS);
	net_mgmt_add_event_callback(&mgmt_cb);
	LOG_INF("Registering IPv4 events");
	net_mgmt_init_event_callback(&net_mgmt_ipv4_callback, ipv4_mgmt_event_handler,
			     NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL);
	net_mgmt_add_event_callback(&net_mgmt_ipv4_callback);
}

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

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		/* STEP x.x - call send() when button 1 is pressed */
		if (button_state & DK_BTN1_MSK){	
			int err = send(sock, MESSAGE_TO_SEND, SSTRLEN(MESSAGE_TO_SEND), 0);
			if (err < 0) {
				LOG_INF("Failed to send message, %d", errno);
				return;
			} LOG_INF("Successfully sent message: %s", MESSAGE_TO_SEND);
		}
		break;
	}
}

void main(void)
{
	int received;
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* To give it time for the wlan0 interface to start up */
	k_sleep(K_SECONDS(1));
	wifi_register_cb();

	wifi_connect();
	LOG_INF("Wait for Wi-fi connection");
	k_sem_take(&wifi_connected, K_FOREVER);
	LOG_INF("Wait for DHCP");
	k_sem_take(&ipv4_obtained, K_FOREVER);

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return;
	}
	
	if (server_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return;
	}

	LOG_INF("Press button 1 on your DK to send your message");

	while (1) {
		/* STEP x.x - Call recv() to listen to received messages */
		received = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);

	 		if (received < 0) {
	 			LOG_ERR("Socket error: %d, exit", errno);
	 			break;
	 		}

	 		if (received == 0) {
	 			LOG_ERR("Empty datagram");
	 			break;
	 		}

	 		recv_buf[received] = 0;
	 		LOG_INF("Data received from the server: (%s)", recv_buf);
			
	 	}
	(void)close(sock);
}