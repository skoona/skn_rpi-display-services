/**
 * udp_locator_client.c
 * - Client
 *
 * cmdline: ./udp_locator_client -m "<request-message-string>"
*/

#include "skn_network_helpers.h"


int main(int argc, char *argv[])
{
    char request[SZ_COMM_BUFF];

    memset(request, 0, sizeof(request));
	strcpy(request, "Raspberry Pi where are you?");

    skn_util_set_program_name_and_description(
    		"udp_locator_client",
			"Collect IPv4 Address/Port Service info from all providers."
			);

	/* Parse any command line options,
	 * like request string override */
    if (skn_locator_client_command_line_parse(argc, argv) == EXIT_FAILURE) {
    	exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	strncpy(request, gd_pch_message, sizeof(request));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	strcpy(request, argv[1]);
    }
	skn_util_logger(SD_DEBUG, "Request Message [%s]", request);

	/* Initialize Signal handler */
	skn_signals_init();

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	PServiceRegistry psr = skn_service_registry_new(request);
	if (psr != NULL && skn_service_registry_entry_count(psr) != 0) {
        char *service_name = "lcd_display_service";

        if (gd_pch_service_name != NULL) {
            service_name = gd_pch_service_name;
        }

		skn_service_registry_list_entries(psr);

		/* find a single entry */
		PRegistryEntry pre = skn_service_registry_find_entry(psr, service_name);
		if (pre != NULL) {
			skn_util_logger(" ", "\nLCD DisplayService (%s) is located at IPv4: %s:%d\n", pre->name, pre->ip, pre->port);
		}
    }

	/* Cleanup and shutdown
	 * - if shutdown was caused by signal handler
	 *   then a termination signal will be sent via signal()
	 *   otherwise, a normal exit occurs
	 */
    if (gd_i_socket) close(gd_i_socket);
    if (psr != NULL) skn_service_registry_destroy(psr);
    skn_signals_cleanup(gi_exit_flag);

    exit(EXIT_SUCCESS);
}
