/*
 ============================================================================
 Name        : lcdDisplayService.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "skn_network_helpers.h"
#include "skn_rpi_helpers.h"

int main(int argc, char *argv[]) {

	int index = 0;
	pthread_t sig_thread;
    sigset_t signal_set;
    uid_t real_user_id = 0;

    char request[SZ_INFO_BUFF];

    memset(request, 0, sizeof(request));
	snprintf(request, sizeof(request), "%02ld Cpus are Online and Available.", skn_get_number_of_cpu_cores() );

    skn_program_name_and_description_set(
    		"lcd_display_service",
			"LCD 4x20 Display Provider."
			);

	/* Parse any command line options,
	 * - check request string override */
    if (skn_handle_display_command_line(argc, argv) == EXIT_FAILURE) {
    	exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	strncpy(request, gd_pch_message, sizeof(request - 1));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	strcpy(request, argv[1]);
    }
	skn_logger(SD_DEBUG, "Welcome Message [%s]", request);

	/*
	 * save the real and effective userids */
	real_user_id = skn_get_userids();

	/*
	 * Set the global interface name and ip address parms */
    get_default_interface_name_and_ipv4_address(gd_ch_intfName, gd_ch_ipAddress);

	/*
	* Setup signal handling before we start
	*/
	if (skn_signal_manager_startup(&sig_thread, &signal_set, &real_user_id) == EXIT_FAILURE ) {
	  exit(EXIT_FAILURE);
	}

	/* **********************************************
	* Do Work Forever
	* - initialize needed resources
	* - start user threads
	*/
	skn_logger(SD_NOTICE, "%s-%s is in startup mode as user(%s)",
		  gd_ch_program_name, PACKAGE_VERSION, gd_pch_effective_userid);

	skn_logger(SD_NOTICE, "Application Active...");


	/*
	 * Do the marvelous work of putting message on display */
	skn_display_manager_do_work(request);

	/*
	* Free any allocated resources before exiting
	* - collect user threads
	* - close open resources
	*/
	skn_logger(SD_NOTICE, "Application Shutdown beginning...");

	/*
	* Cleanup signal handler before exit
	*/
	index = skn_signal_manager_shutdown(sig_thread, &signal_set);

	skn_logger(SD_NOTICE, "Application clean or controlled shutdown complete.");

	exit(index);
}

