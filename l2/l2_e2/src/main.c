/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <dk_buttons_and_leds.h>

/* STEP 3 - Include the necessary header files */


LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

/* STEP 4 - Define a macro for the relevant network events */


/* STEP 5 - Declare the callback structure for Wi-Fi events */


/* STEP 6.1 - Define the boolean connected and the semaphore run_app */


/* STEP 6.2 - Define the callback function for network events */


#ifdef CONFIG_WIFI_CREDENTIALS_STATIC 
/* STEP 8 - Define the function to populate the Wi-Fi credential parameters */
static int wifi_args_to_params(struct wifi_connect_req_params *params)
{

	/* STEP 8.1 - Populate the SSID and password */


	/* STEP 8.2 - Populate the rest of the relevant members */

	return 0;
}
#endif //CONFIG_WIFI_CREDENTIALS_STATIC

int main(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	LOG_INF("Initializing Wi-Fi driver");
	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	/* STEP 7 - Initialize and add the callback function for network events */


	#ifdef CONFIG_WIFI_CREDENTIALS_STATIC 
	/* STEP 9.1 - Declare the variable for the network configuration parameters */


	/* STEP 9.2 - Get the network interface */


	/* STEP 10 - Populate cnx_params with the network configuration */


	/* STEP 11 - Call net_mgmt() to request the Wi-Fi connection */

	#endif //CONFIG_WIFI_CREDENTIALS_STATIC

	k_sem_take(&run_app, K_FOREVER);

	return 0;
}