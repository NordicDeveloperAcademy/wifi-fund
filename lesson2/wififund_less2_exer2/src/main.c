/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <dk_buttons_and_leds.h>

/* STEP 3 - Include the necessary header files */



LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

/* STEP 4 - Declare the callback structure */


/* STEP 6 - Define the function to populate the Wi-Fi credential parameters */


/* STEP 5 - Define the callback function */


int main(void)
{
	/* STEP 7.1 - Declare the variable for the network configuration parameters */
	
	/* STEP 7.2 - Define the variable for the network interface */
	

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	/* Sleep to allow initialization of Wi-Fi driver */
	k_sleep(K_SECONDS(1));

	/* STEP 8 - Initialize and add the callback function */
	

	/* STEP 9 - Populate cnx_params with the network configuration */
	

	/* STEP 10 - Call net_mgmt() to request the Wi-Fi connection */
	
	return 0;
}