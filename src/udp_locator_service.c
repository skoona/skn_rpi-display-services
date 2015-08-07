/**
 * udp_locator_service.c
 * - Server
 *
 * cmdline: ./udp_locator_service -m "<delimited-response-message-string>"
 *
 * delimited-response-message-string : required order -> name,ip,port,line-delimiter...
 *   format: "name=<service-name>,ip=<service-ipaddress>ddd.ddd.ddd.ddd,port=<service-portnumber>ddddd <line-delimiter>"
 *	   name=<service-name> text_string_with_no_spaces
 *   	 ip=<service-ipaddress> ddd.ddd.ddd.ddd
 *     port=<service-portnumber> ddddd
 *   REQUIRED   <line-delimiter> is one of these '|', '%', ';'
 *
 *   example: -m "name=lcd_display_service,ip=192.168.1.15,port=48028|"
 *   udp_locator_service -m "mame=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, pip=10.100.1.19, port=48029|name=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, port=48029|"
*/

#include "skn_network_helpers.h"


int main(int argc, char *argv[])
{
	int exit_code = EXIT_SUCCESS;
    char response[SZ_COMM_BUFF];

    memset(response, 0, sizeof(response));

    skn_program_name_and_description_set(
    		"udp_locator_service",
			"Provides IPv4 Address/Port Service info."
			);

    if (skn_handle_locator_command_line(argc, argv) == EXIT_FAILURE) {
    	exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	strncpy(response, gd_pch_message, sizeof(response));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = response;
    }
    skn_get_userids();
    skn_logger(SD_NOTICE, "%s-%s is in startup mode as user(%s)", gd_ch_program_name, PACKAGE_VERSION, gd_pch_effective_userid);

    if ((strlen(response) > 16) && (service_registry_valiadate_response_format(response) == EXIT_FAILURE)) {
    	skn_logger(SD_EMERG, "Message format is invalid! cannot proceed.");
    	log_response_message(response);
    	exit(EXIT_FAILURE);
    }

    /* Initialize Signal handler */
    signals_init();

	gd_i_socket = skn_udp_host_create_broadcast_socket(SKN_FIND_RPI_PORT, 20.0);
	if (gd_i_socket == EXIT_FAILURE) {
		skn_logger(SD_EMERG, "Application Host Init Failed! ExitCode=%d", exit_code);
        signals_cleanup(gi_exit_flag);
    	exit(EXIT_FAILURE);		
	}

	exit_code = service_registry_provider(gd_i_socket, response);
		skn_logger(SD_NOTICE, "Application ExitCode=%d", exit_code);
	
    if (gd_i_socket) close(gd_i_socket);
    signals_cleanup(gi_exit_flag);

    skn_logger(SD_NOTICE, "\n============================\nShutdown Complete\n============================\n");

    exit(exit_code);
}
