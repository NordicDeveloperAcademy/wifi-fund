/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef _MQTTCONNECTION_H_
#define _MQTTCONNECTION_H_
#define RANDOM_LEN 10
#define CLIENT_ID_LEN sizeof(CONFIG_BOARD) + 1 + RANDOM_LEN
/**@brief Initialize the MQTT client structure
 */
int client_init(struct mqtt_client *client);

/**@brief Initialize the file descriptor structure used by poll.
 */
int fds_init(struct mqtt_client *c, struct pollfd *fds);

/**@brief Function to publish data on the configured topic
 */
int data_publish(struct mqtt_client *c, enum mqtt_qos qos,
	uint8_t *data, size_t len);

#endif /* _CONNECTION_H_ */
