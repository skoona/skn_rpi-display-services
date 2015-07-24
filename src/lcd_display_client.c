/**
 * udp_locator_client.c
 * - Client
 *
 * cmdline: ./udp_locator_client -m "<request-message-string>"
*/

#include "skn_network_helpers.h"
//#include "skn_rpi_helpers.h"


int main(int argc, char *argv[])
{
    char request[SZ_INFO_BUFF];
    PRegistryEntry pre = NULL;
    int vIndex = 0;

    memset(request, 0, sizeof(request));
	strcpy(request, "Raspberry Pi where are you?");

    skn_program_name_and_description_set(
    		"lcd_display_client",
			"Send messages to the Display Service."
			);

	/* Parse any command line options,
	 * like request string override */
    if (skn_handle_locator_command_line(argc, argv) == EXIT_FAILURE) {
    	exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	strncpy(request, gd_pch_message, sizeof(request));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	strcpy(request, argv[1]);
    }
	skn_logger(SD_DEBUG, "Request Message [%s]", request);

	/* Initialize Signal handler */
	signals_init();

	/* Create local socket for sending requests */
	gd_i_socket = skn_udp_host_socket_create(0, 5);
	if (gd_i_socket == EXIT_FAILURE) {
        signals_cleanup(gi_exit_flag);
    	exit(EXIT_FAILURE);		
	}

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	PServiceRegistry psr = service_registry_get_via_udp_broadcast(gd_i_socket, request);
	if (psr != NULL && service_registry_entry_count(psr) != 0) {

		/* find a single entry */
		pre = service_registry_find_entry(psr, "lcd_display_service");
		if (pre != NULL) {
			skn_logger(SD_INFO, "LCD Display Service is locate at IPv4: %s:%d", pre->ip, pre->port);
		}
    }

	// we have the location
	PServiceRequest pnsr = skn_service_request_create(pre, gd_i_socket, request);
	if (pnsr != NULL) {
	    skn_logger(SD_NOTICE, "Application Active...");

        do {
            generate_uname_info(pnsr->request);
            vIndex = skn_udp_service_request(pnsr);
            if (vIndex == EXIT_FAILURE) {
                break;
            }
            sleep(1);
        } while(gd_i_debug != 0 && gi_exit_flag == SKN_RUN_MODE_RUN);
        free(pnsr);  // Done
    } else {
        skn_logger(SD_WARNING, "Unable to create Network Request.");
    }

	/* Cleanup and shutdown
	 * - if shutdown was caused by signal handler
	 *   then a termination signal will be sent via signal()
	 *   otherwise, a normal exit occurs
	 */
    if (gd_i_socket) close(gd_i_socket);
    if (psr != NULL) service_registry_destroy(psr);
    signals_cleanup(gi_exit_flag);

    exit(EXIT_SUCCESS);
}
