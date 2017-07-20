/*
 ============================================================================
 Name        : lcdDisplayService.c
 ============================================================================
 */

#include "skn_network_helpers.h"
#include "skn_rpi_helpers.h"


int main(int argc, char *argv[]) {

	int index = 0;
    PSknSignalManager pssm = NULL;

    char request[SZ_INFO_BUFF];

    memset(request, 0, sizeof(request));
	snprintf(request, sizeof(request), "%02ld Cpus Online.", skn_util_generate_number_cpu_cores() );

    skn_util_set_program_name_and_description(
    		"lcd_display_service",
			"LCD 4x20 Display Provider."
			);

	/* Parse any command line options,
	 * - check request string override */
    if (skn_display_service_command_line_parse(argc, argv) == EXIT_FAILURE) {
    	exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	strncpy(request, gd_pch_message, (SZ_INFO_BUFF - 1));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	strcpy(request, argv[1]);
    }
    skn_util_get_userids();
    skn_util_logger(SD_NOTICE, "%s-%s is in startup mode as user(%s)", gd_ch_program_name, PACKAGE_VERSION, gd_pch_effective_userid);

	skn_util_logger(SD_DEBUG, "Welcome Message [%s]", request);

	/*
	 * Set the global interface name and ip address parms */
    skn_util_get_default_interface_name_and_ipv4_address(gd_ch_intfName, gd_ch_ipAddress);

	/*
	* Setup signal handling before we start
	*/
    pssm = sknSignalManagerInit(&gi_exit_flag);
	if ( NULL == pssm ) {
	  exit(EXIT_FAILURE);
	}

	/* **********************************************
	* Do Work Forever
	* - initialize needed resources
	* - start user threads
	*/

	/*
	 * Do the marvelous work of putting message on display */
	skn_display_manager_do_work(request);

	/*
	* Free any allocated resources before exiting
	* - collect user threads
	* - close open resources
	*/
	skn_util_logger(SD_NOTICE, "Application beginning orderly shutdown...");

	/*
	* Cleanup signal handler before exit
	*/
	sknSignalManagerShutdown(pssm);

    skn_util_logger(SD_NOTICE, "\n============================\nShutdown Complete\n============================\n");

	exit(index);
}

