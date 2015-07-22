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
    	strncpy(request, gd_pch_message, sizeof(request));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	strcpy(request, argv[1]);
    }
	skn_logger(SD_DEBUG, "Welcome Message [%s]", request);


	// handle signals                   -- threaded (A)
	// initialize lcd via wiring pi
	// setup rolling display context
	// start request listener           -- threaded (B)
	// -- process request or timer      -- threaded (B)
	// stop request listener
	// shutdown lcd        (B)
	// shutdown signals	   (A)

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

	  int lastPosLine[MAX_DISPLAY_ROWS] = {0, 0, 0, 0};
	  char ch_lcd_message[MAX_DISPLAY_ROWS][SZ_INFO_BUFF];

	  wiringPiSetupSys () ;
	  pcf8574Setup (AF_BASE, 0x27) ;
	  skn_pcf8574LCDSetup (HIGH) ;

	    skn_scroller_wrap_blanks(request);
		generate_rpi_model_info(ch_lcd_message[3]); skn_scroller_wrap_blanks(ch_lcd_message[3]);

	  while(gi_exit_flag == SKN_RUN_MODE_RUN) {
		generate_datetime_info(ch_lcd_message[2]);
		generate_cpu_temps_info(ch_lcd_message[1]); skn_scroller_wrap_blanks(ch_lcd_message[1]);

		lastPosLine[0] = skn_scroller_scroll_line (lastPosLine[0], 0, gd_i_cols, request, SCROLL_WAIT);
		lastPosLine[1] = skn_scroller_scroll_line (lastPosLine[1], 1, gd_i_cols, ch_lcd_message[1], SCROLL_WAIT);
		lastPosLine[2] = skn_scroller_scroll_line (lastPosLine[2], 2, gd_i_cols, ch_lcd_message[2], SCROLL_WAIT);
		lastPosLine[3] = skn_scroller_scroll_line (lastPosLine[3], 3, gd_i_cols, ch_lcd_message[3], SCROLL_WAIT);
	  }

	  lcdClear (gd_i_lcd_handle) ;
	  skn_lcd_backlight_set (LOW) ;


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

