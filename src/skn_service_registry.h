/*
 * skn_service_registry.h
 *
 *  Created on: May 28, 2016
 *      Author: jscott
 */


#include "skn_network_helpers.h"

#ifndef SKN_SERVICE_REGISTRY_H_
#define SKN_SERVICE_REGISTRY_H_


extern PUDPServiceProvider skn_udp_service_provider_new(char * optionalBroadcastMessage);

extern int skn_udp_service_provider_registry_entries(PUDPServiceProvider provider);
extern char * skn_udp_service_provider_get_this_ipaddress(PUDPServiceProvider provider);
extern char * skn_udp_service_provider_get_this_nodename(PUDPServiceProvider provider);
extern char * skn_udp_service_provider_get_this_interface_name(PUDPServiceProvider provider);

extern PServiceRequest skn_udp_service_provider_request_service_by_nodename(PUDPServiceProvider provider, char *nodeName);
extern PServiceRequest skn_udp_service_provider_request_service_by_servicename(PUDPServiceProvider provider, char *serviceName);
extern PServiceRequest skn_udp_service_provider_receive_any(PUDPServiceProvider provider);


extern int skn_udp_service_provider_send(PServiceRequest serviceRequest);
extern int skn_udp_service_provider_set_request_message(PServiceRequest serviceRequest, char *message);
extern char * skn_udp_service_provider_get_request_message(PServiceRequest serviceRequest);
extern char * skn_udp_service_provider_get_response_message(PServiceRequest serviceRequest);


extern void skn_udp_service_provider_service_request_destroy(PServiceRequest serviceRequest);
extern void skn_udp_service_provider_destroy(PUDPServiceProvider provider);

extern int skn_udp_service_provider_registry_responder_run(char *optionalRegistryString);

#endif /* SKN_SERVICE_REGISTRY_H_ */
