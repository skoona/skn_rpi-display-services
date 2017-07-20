/*
 * skn_udp_service_provider.c
 *
 *  Created on: May 28, 2016
 *      Author: jscott
 *
 * ------------------------
 * API Stories:
 * ------------------------
 *
 *  - [Clients]
 *  *** Send sensor measurement data, or any message, to display service
 *
 *  PServiceRequest service = NULL;
 *  char *serviceName = "lcd_display_service";
 *
 *   service = skn_udp_service_request_new(NULL, serviceName);
 *
 *  // LOOP
 *   // DO SOMETHING TO GET DATA TO SEND
 *
 *   skn_udp_service_request_set_send_message(service, "Send any string upto 255 chars");
 *
 *   if( -1 == skn_udp_service_request_send_receive(service) ) {
 *      printf("UDP Transaction Error: %s",
 *             skn_udp_service_request_get_error_message(service) );
 *      break;  // exit loop or maybe ignore and continue
 *   } else {
 *      printf("UDP Transaction Received: %s",
 *             skn_udp_service_request_get_received_message(service) );
 *   }
 *
 *  // END OF LOOP
 *
 *   skn_udp_service_request_destroy(service);
 *
 *   return(EXIT_SUCCESS);
 *
 *
 * ------------------------
 *
 * - [Service Providers]
 * *** Listen for Requests and do service.
 *
 *  PServiceRequest service = NULL;
 *  char *strOk = "202 Accepted";
 *  char *strFail = "406 Not Acceptable";
 *  char *request = NULL;
 *
 *   service = skn_udp_service_request_new(NULL, NULL);
 *
 *  // LOOP
 *     if ( -1 == skn_udp_service_request_receive_any(service) ) {
 *        // print error
 *        continue;
 *     }
 *
 *     // ** Process Request
 *     request = skn_udp_service_request_get_received_message(service);
 *
 *     if ( PROCESSED-OK ) {
 *       skn_udp_service_request_set_send_message(service, strOK);
 *     } else {
 *       skn_udp_service_request_set_send_message(service, strFail);
 *     }
 *     skn_udp_service_request_send(service);
 *
 *  // END LOOP
 *
 *   skn_udp_service_request_destroy(service);
 *
 *
 * ------------------------
 *
 * - [Location/Locator Providers]
 * *** Respond to Broadcasted Location Requests with this nodes service(s) list
 *     Until exit_flag is set
 *
 *  PUDPServiceProvider provider = NULL;
 *  PServiceRequest service = NULL;
 *  int exit_flag = 0;
 *  char *optionalRegistryString = "name=lcd_display_service,ip=192.168.1.15,port=48029|";
 *
 *   provider = skn_udp_service_provider_registry_responder_new(&exit_flag, optionalRegistryString);
 *
 *    ... do something else, cause the above is in a thread ...
 *
 *   skn_udp_service_provider_destroy(provider);
 *
 */

#include "skn_udp_service_provider.h"



/**
 * skn_udp_service_request_new()
 * - Establishes UDP Communications with serviceName using the service lookup registry protocol
 * - Optionally returns a high-level provider object is passed the address of a pointer to save to
 *
 * @param provider
 * @param serviceName
 * @return PServiceRequest service request object
 */
PServiceRequest skn_udp_service_request_new(PUDPServiceProvider *provider, char * serviceName) {
    PServiceRequest psr = NULL;
    PUDPServiceProvider pusp = NULL;

    // Do Registry Lookup
    // Initialize Request block


    return (psr);
}
