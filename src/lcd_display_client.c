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
    PServiceRegistry psr = NULL;
    PRegistryEntry pre = NULL;
    PServiceRequest pnsr = NULL;
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
	gd_i_socket = skn_udp_host_create_broadcast_socket(0, 8);
	if (gd_i_socket == EXIT_FAILURE) {
        signals_cleanup(gi_exit_flag);
    	exit(EXIT_FAILURE);		
	}

    skn_logger(SD_NOTICE, "Application Active...");

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	psr = service_registry_get_via_udp_broadcast(gd_i_socket, request);
	if (psr != NULL && service_registry_entry_count(psr) != 0) {
	    char *service_name = "lcd_display_service";

	    if (gd_pch_service_name != NULL) {
	        service_name = gd_pch_service_name;
	    }

		/* find a single entry */
		pre = service_registry_find_entry(psr, service_name);
		if (pre != NULL) {
            skn_logger(SD_INFO, "LCD DisplayService (%s) is located at IPv4: %s:%d", pre->name, pre->ip, pre->port);
		}

        /*
         * Switch to non-broadcast type socket */
        close(gd_i_socket); gd_i_socket = -1;
        gd_i_socket = skn_udp_host_create_regular_socket(0, 8);
        if (gd_i_socket == EXIT_FAILURE) {
            if (psr != NULL) service_registry_destroy(psr);
            signals_cleanup(gi_exit_flag);
            exit(EXIT_FAILURE);
        }
    }


	// we have the location
	if (pre != NULL) {
	    pnsr = skn_service_request_create(pre, gd_i_socket, request);
	}
	if (pnsr != NULL) {
        do {
            generate_uname_info(pnsr->request);
            vIndex = skn_udp_service_request(pnsr);
            if (vIndex == EXIT_FAILURE) {
                break;
            }
            sleep(gd_i_update);
        } while(gd_i_update != 0 && gi_exit_flag == SKN_RUN_MODE_RUN);
        free(pnsr);  // Done
    } else {
        skn_logger(SD_WARNING, "Unable to create Network Request.");
    }

    skn_logger(SD_NOTICE, "Application Shutdown... (%d)", gi_exit_flag);

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
