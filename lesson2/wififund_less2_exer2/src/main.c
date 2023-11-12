/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <dk_buttons_and_leds.h>

/* STEP 3 - Include the necessary header files */



LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

/* STEP 4 - Define a macro for the relevant Wi-Fi events */

/* STEP 12.1 Define a macro for the IPv4 events to watch */


/* STEP 5 - Declare and define the callback structure for Wi-Fi events */

/* STEP 12.2 Declare the callback structure for IPv4 events */


/* STEP 7 - Define the function to populate the Wi-Fi credential parameters */
static int wifi_args_to_params(struct wifi_connect_req_params *params)
{

	/* STEP 7.1 Populate the SSID and password */
	

	/* STEP 7.2 - Populate the rest of the relevant members */
	

	return 0;
}

/* STEP 6 - Define the callback function for Wi-Fi events */

/* STEP 12.3 - Define the callback function for IPv4 events */



int main(void)
{
	/* STEP 8.1 - Declare the variable for the network configuration parameters */
	
	/* STEP 8.2 - Define the variable for the network interface */
	

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	/* STEP 9 - Initialize and add the callback function for Wi-Fi events */
	

	/* STEP 12.4 Initialize and add the callback function for IPv4 events */
	

	/* STEP 10 - Populate cnx_params with the network configuration */
	

	/* STEP 11 - Call net_mgmt() to request the Wi-Fi connection */


	return 0;
}