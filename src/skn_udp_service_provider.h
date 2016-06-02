/*
 * skn_udp_service_provider.h
 *
 *  Created on: May 28, 2016
 *      Author: jscott
 */


#include "skn_network_helpers.h"

#ifndef SKN_UDP_SERVICE_PROVIDER_H_
#define SKN_UDP_SERVICE_PROVIDER_H_

/**
 * Core Provider API
*/
extern PUDPServiceProvider skn_udp_service_provider_new(int *optional_exit_flag);
extern void   skn_udp_service_provider_destroy(PUDPServiceProvider);
extern char * skn_udp_service_provider_registry_list(PUDPServiceProvider provider);

/*
 * Core Provider: API for Service Providers */
extern PUDPServiceProvider skn_udp_service_provider_registry_responder_new(int *optional_exit_flag, char *optionalRegistryString);

/*
 * Core Provider: Networking Helpers */
extern char * skn_udp_service_provider_get_this_ipaddress();
extern char * skn_udp_service_provider_get_this_nodename();
extern char * skn_udp_service_provider_get_this_interface_name();


/**
 * Core Provider: Service Request API */
extern PServiceRequest skn_udp_service_request_new(PUDPServiceProvider *provider, char * serviceName);
extern void skn_udp_service_request_destroy(PServiceRequest service);

extern int skn_udp_service_request_send_receive(PServiceRequest service);
extern int skn_udp_service_request_send(PServiceRequest service);
extern int skn_udp_service_request_receive(PServiceRequest service);
extern int skn_udp_service_request_receive_any(PServiceRequest service);
extern int skn_udp_service_request_set_send_message(PServiceRequest service, char *message);
extern char * skn_udp_service_request_get_send_message(PServiceRequest service);
extern char * skn_udp_service_request_get_received_message(PServiceRequest service);

extern char * skn_udp_service_request_get_error_message(PServiceRequest service);


#endif /* SKN_UDP_SERVICE_PROVIDER_H_ */
