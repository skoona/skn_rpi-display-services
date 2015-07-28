/**
 * skn_network_helpers.h
 *
 *
 */


#ifndef SKN_NETWORK_HELPERS_H__
#define SKN_NETWORK_HELPERS_H__

#include "skn_common_headers.h"
#include <sys/utsname.h>

//extern PDisplayLine skn_display_manager_add_line(PDisplayManager pdmx, char * client_request_message);


/*
 * Globals defined in skn_network_helpers.c
*/
extern sig_atomic_t gi_exit_flag;
extern char *gd_pch_message;
extern signed int gd_i_debug;
extern int gd_i_socket;
extern char gd_ch_program_name[SZ_INFO_BUFF];
extern char gd_ch_program_desc[SZ_INFO_BUFF];
extern char *gd_pch_effective_userid;
extern char gd_ch_ipAddress[SZ_CHAR_BUFF];
extern char gd_ch_intfName[SZ_CHAR_BUFF];
extern char gd_ch_hostName[SZ_CHAR_BUFF];
extern char gd_ch_hostShortName[SZ_CHAR_BUFF];
extern int gd_i_display;
extern int gd_i_unique_registry;
extern int gd_i_update;
extern char * gd_pch_service_name;

/*
 * General Utilities
*/
extern long skn_get_number_of_cpu_cores();
extern int generate_loadavg_info(char *msg);
extern int generate_uname_info(char *msg);
extern int generate_datetime_info(char *msg);
extern double skn_duration_in_milliseconds(struct timeval *pstart, struct timeval *pend);
extern void skn_program_name_and_description_set(const char *name, const char *desc);
extern int skn_logger(const char *level, const char *format, ...);
extern int skn_handle_locator_command_line(int argc, char **argv);
extern char * skn_strip(char * alpha);
extern uid_t skn_get_userids();

extern void signals_init();
extern void signals_cleanup(int sig);

/*
 * Server/Client Communication Routines
*/
extern int skn_udp_host_create_broadcast_socket(int port, int rcvTimeout);
extern int skn_udp_host_create_regular_socket(int port, int rcvTimeout);
extern PServiceRequest skn_service_request_create(PRegistryEntry pre, int host_socket, char *request);
extern int skn_udp_service_request(PServiceRequest psr);
extern int skn_display_manager_message_consumer_startup(PDisplayManager pdm);
extern void skn_display_manager_message_consumer_shutdown(PDisplayManager pdm);

extern int get_default_interface_name(char *pchDefaultInterfaceName);
extern int get_broadcast_ip_array(PIPBroadcastArray paB);
extern void log_response_message(const char * response);
extern void get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);

/*
 * Service Registry Public Routines
 */
extern PServiceRegistry service_registry_valiadated_registry(const char *response);
extern int service_registry_valiadate_response_format(const char *response);
extern int service_registry_provider(int i_socket, char *response);
extern PServiceRegistry service_registry_get_via_udp_broadcast(int i_socket, char *request);
extern int service_registry_entry_count(PServiceRegistry psr);
extern int service_registry_list_entries(PServiceRegistry psr);
extern PRegistryEntry service_registry_find_entry(PServiceRegistry psreg, char *serviceName);
extern void * service_registry_get_entry_ref(PRegistryEntry prent, char *field);
extern void service_registry_destroy(PServiceRegistry psreg);

#endif // SKN_NETWORK_HELPERS_H__
