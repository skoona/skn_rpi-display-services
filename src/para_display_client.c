/**
 * para_display_client.c
 * - Parallella (Epiphany III) Display Temperature Client
 *
 * cmdline: ./para_display_client -m "<request-message-string>"
*/

#include "skn_network_helpers.h"

#define kXADCPATH  "/sys/bus/iio/devices/iio:device0/"


int sknGetConstants(int *nOffset, float *fScale) {
  char  strRead[SZ_INFO_BUFF];
  FILE *sysfile;

  if((sysfile = fopen(kXADCPATH "in_temp0_offset", "ra")) == NULL) {
    fprintf(stderr, "ERROR: Can't open offset device file\n");
    return 1;
  }
  fgets(strRead, SZ_INFO_BUFF-1, sysfile);
  fclose(sysfile);
  *nOffset = atoi(strRead);

  if((sysfile = fopen(kXADCPATH "in_temp0_scale", "ra")) == NULL) {
    fprintf(stderr, "ERROR: Can't open scale device file\n");
    return 2;
  }
  fgets(strRead, SZ_INFO_BUFF-1, sysfile);
  fclose(sysfile);
  *fScale = atof(strRead);

  return 0;
}

/**
 * Zynq chips temperature
 */
int sknGetTemp(double *fTemp, double *cTemp, int nOffset, float fScale) {
  FILE *sysfile;
  char  strRead[SZ_INFO_BUFF];
  int  nRaw;

  if(nOffset == 0 || fScale == 0.0)
    return 1;

  if((sysfile = fopen(kXADCPATH "in_temp0_raw", "ra")) == NULL) {
    return 2;
  }

  fgets(strRead, SZ_INFO_BUFF-1, sysfile);
  fclose(sysfile);
  nRaw = atoi(strRead);

  *fTemp = (double ) (nRaw + nOffset) * fScale / 1000.0;

  *cTemp = (double ) (*fTemp - 32.0) * 5 / 9;

  return 0;
}


int main(int argc, char *argv[])
{
    char request[SZ_INFO_BUFF];
    char registry[SZ_CHAR_BUFF];
    PServiceRegistry psr = NULL;
    PRegistryEntry pre = NULL;
    PServiceRequest pnsr = NULL;
    int vIndex = 0;
    int      nOffset = 0;
    float    fScale = 0.0;

    memset(registry, 0, sizeof(registry));
    memset(request, 0, sizeof(request));
	strncpy(registry, "DisplayClient: Raspberry Pi where are you?", sizeof(registry) - 1);

    skn_program_name_and_description_set(
    		"para_display_client",
			"Send Epiphany III ( Zynq ) Temperature to Display Service."
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

    // get some platform constants
    if(sknGetConstants(&nOffset, &fScale)) {
        skn_logger(SD_ERR, "GetConstants() Failed! Shutting Down!");
        exit(EXIT_FAILURE);
    }

	/* Initialize Signal handler */
	signals_init();

	/* Create local socket for sending requests */
	gd_i_socket = skn_udp_host_create_broadcast_socket(0, 4.0);
	if (gd_i_socket == EXIT_FAILURE) {
        signals_cleanup(gi_exit_flag);
    	exit(EXIT_FAILURE);		
	}

    skn_logger(SD_NOTICE, "Application Active...");

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	psr = service_registry_get_via_udp_broadcast(gd_i_socket, registry);
	if (psr != NULL && service_registry_entry_count(psr) != 0) {
	    char *service_name = "lcd_display_service";

	    if (gd_pch_service_name != NULL) {
	        service_name = gd_pch_service_name;
	    }

		/* find a single entry */
		pre = service_registry_find_entry(psr, service_name);
		if (pre != NULL) {
            skn_logger(" ", "\nLCD DisplayService (%s) is located at IPv4: %s:%d\n", pre->name, pre->ip, pre->port);
		}

        /*
         * Switch to non-broadcast type socket */
        close(gd_i_socket); gd_i_socket = -1;
        gd_i_socket = skn_udp_host_create_regular_socket(0, 8.0);
        if (gd_i_socket == EXIT_FAILURE) {
            if (psr != NULL) service_registry_destroy(psr);
            signals_cleanup(gi_exit_flag);
            exit(EXIT_FAILURE);
        }
    }

	// we have the location
	if (pre != NULL) {
	    if (request[0] == 0) {
	        snprintf(request, sizeof(request), "%02ld Cores Available.",  skn_get_number_of_cpu_cores() );
	    }
	    pnsr = skn_service_request_create(pre, gd_i_socket, request);
	}
	if (pnsr != NULL) {
        double fTemp = 0.0;
        double cTemp = 0.0;
        do {
            vIndex = skn_udp_service_request(pnsr);
            if ((vIndex == EXIT_FAILURE) && (gd_i_update == 0)) { // ignore if non-stop is set
                break;
            }
            sleep(gd_i_update);

            sknGetTemp(&fTemp, &cTemp, nOffset, fScale);
            snprintf(pnsr->request, sizeof(pnsr->request) -1,  "%.1fF %.1fC", fTemp, cTemp);


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
    if (psr != NULL) service_registry_destroy(psr);
    signals_cleanup(gi_exit_flag);

    exit(EXIT_SUCCESS);
}
