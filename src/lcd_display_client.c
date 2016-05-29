/**
 * udp_locator_client.c
 * - Client
 *
 * cmdline: ./udp_locator_client -m "<request-message-string>"
*/

#include "skn_network_helpers.h"


/**
 * DO NOT USE THIS IN MODULES THAT HANDLE A I2C Based LCD
 * RPi cannot handle I2C and GetCpuTemp() without locking the process
 * in an uniterrupted sleep; forcing a power cycle.
 *
 * Redhat/Centos: /sys/class/hwmon/hwmon0/device/temp1_input
 * Ubuntu/Debian: /sys/class/thermal/thermal_zone0/temp
 *
*/
long sknGetCpuTemps(PCpuTemps temps) {
    long lRaw = 0;
    int rc = 0;

    FILE *sysFs = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (sysFs == NULL) {
        skn_logger(SD_WARNING, "Warning: Failed to open Debian CPU temperature: %d:%s\n", errno, strerror(errno));
        sysFs = fopen("/sys/class/hwmon/hwmon0/device/temp1_input", "r");
        if (sysFs == NULL) {
            skn_logger(SD_WARNING, "Warning: Failed to open Centos CPU temperature: %d:%s\n", errno, strerror(errno));
            return -1;
        }
    }

    rc = fscanf(sysFs, "%ld", &lRaw);
    fclose(sysFs);

    if (rc != 1 || lRaw < 0) {
        skn_logger(SD_WARNING, "Warning: Failed to READ CPU temperature: %d:%s\n", strerror(errno));
        return -1;
    }

    if (temps != NULL) { // populate struct
        snprintf(temps->c, sizeof(temps->c), "%3.1fC", (double )(lRaw / 1000.0));
        snprintf(temps->f, sizeof(temps->f), "%3.1fF", (double )(lRaw / 1000.0 * 9 / 5 + 32));
        temps->raw = lRaw;
        strncpy(temps->cbName, "CpuTemps", sizeof(temps->cbName) - 1);
    }

    return lRaw;
}


/**
 * DO NOT USE THIS IN MODULES THAT HANDLE A I2C Based LCD
 * RPi cannot handle I2C and GetCpuTemp() without locking the process
 * in an uniterrupted sleep; forcing a power cycle.
*/
int sknGenerateCpuTempsInfo(char *msg) {
    static CpuTemps cpuTemp;
    int mLen = 0;

    memset(&cpuTemp, 0, sizeof(CpuTemps));
    if ( sknGetCpuTemps(&cpuTemp) != -1 ) {
        mLen = snprintf(msg, SZ_INFO_BUFF-1, "CPU: %s %s", cpuTemp.c, cpuTemp.f);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF-1, "Temp: N/A");
    }

    return mLen;
}


int main(int argc, char *argv[])
{
    char request[SZ_INFO_BUFF];
    char registry[SZ_CHAR_BUFF];
    PServiceRegistry psr = NULL;
    PRegistryEntry pre = NULL;
    PServiceRequest pnsr = NULL;
    int vIndex = 0;
    long host_update_cycle = 0;

    memset(registry, 0, sizeof(registry));
    memset(request, 0, sizeof(request));
	strncpy(registry, "DisplayClient: Raspberry Pi where are you?", sizeof(registry) - 1);

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

	skn_logger(SD_DEBUG, "Request  Message [%s]", request);
	skn_logger(SD_DEBUG, "Registry Message [%s]", registry);

	/* Initialize Signal handler */
	skn_signals_init();

    skn_logger(SD_NOTICE, "Application Active...");

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	psr = skn_service_registry_new(registry);
	if (psr != NULL && skn_service_registry_entry_count(psr) != 0) {
	    char *service_name = "lcd_display_service";

	    if (gd_pch_service_name != NULL) {
	        service_name = gd_pch_service_name;
	    }

		/* find a single entry */
		pre = skn_service_registry_find_entry(psr, service_name);
		if (pre != NULL) {
            skn_logger(" ", "\nLCD DisplayService (%s) is located at IPv4: %s:%d\n", pre->name, pre->ip, pre->port);
		}

        /*
         * Switch to non-broadcast type socket */
        gd_i_socket = skn_udp_host_create_regular_socket(0, 8.0);
        if (gd_i_socket == EXIT_FAILURE) {
            if (psr != NULL) skn_service_registry_destroy(psr);
            skn_signals_cleanup(gi_exit_flag);
            exit(EXIT_FAILURE);
        }
    }


	// we have the location
	if (pre != NULL) {
	    if (request[0] == 0) {
	        snprintf(request, sizeof(request), "%02ld Cores Available.",  sknGetNumberCpuCores() );
	    }
	    pnsr = skn_udp_service_provider_service_request_new(pre, gd_i_socket, request);
	}
	if (pnsr != NULL) {
        do {
            vIndex = skn_udp_service_provider_send(pnsr);
            if ((vIndex == EXIT_FAILURE) && (gd_i_update == 0)) { // ignore if non-stop is set
                break;
            }
            sleep(gd_i_update);

            switch (host_update_cycle++) {  // cycle through other info
                case 0:
                    sknGenerateLoadavgInfo(pnsr->request);
                    break;
                case 1:
                    sknGenerateDatetimeInfo(pnsr->request);
                    break;
                case 2:
                    sknGenerateUnameInfo(pnsr->request);
                break;
                case 3:
                    sknGenerateCpuTempsInfo(pnsr->request);
                    host_update_cycle = 0;
                break;
            }

        } while(gd_i_update != 0 && gi_exit_flag == SKN_RUN_MODE_RUN);
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
    if (psr != NULL) skn_service_registry_destroy(psr);
    skn_signals_cleanup(gi_exit_flag);

    exit(EXIT_SUCCESS);
}
