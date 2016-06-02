/**
 * skn_network_helpers.h
 *
 *
 */


#ifndef SKN_NETWORK_HELPERS_H__
#define SKN_NETWORK_HELPERS_H__

#include "skn_common_headers.h"
#include "skn_signal_manager.h"

#include <sys/utsname.h>

typedef struct _ipBroadcastArray {
    char cbName[SZ_CHAR_BUFF];
    char chDefaultIntfName[SZ_CHAR_BUFF];
    char ifNameStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char ipAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char maskAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char broadAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    int defaultIndex;
    int count; // index = count - 1
} IPBroadcastArray, *PIPBroadcastArray;

#define ARY_MAX_REGISTRY 128

typedef struct _registryEntry {
    char cbName[SZ_CHAR_BUFF];
    char name[SZ_INFO_BUFF];
    char ip[SZ_INFO_BUFF];
    int port;
} RegistryEntry, *PRegistryEntry;

typedef struct _serviceRegistry {
    char cbName[SZ_CHAR_BUFF];
    int iSocket; /* active socket */
    int count;   /* current number of entries */
    int computedMax; /* computed container size of .entry */
    PRegistryEntry entry[ARY_MAX_REGISTRY];
} ServiceRegistry, *PServiceRegistry;

typedef struct _UDPServiceProvider *PUDPServiceProvider;

typedef struct _serviceRequest {
    char cbName[SZ_CHAR_BUFF];
    PUDPServiceProvider provider;     /* Main Service Provider Object */
    PRegistryEntry pre;               /* Targeted Registry Entry */
    int iSocket;                      /* regular socket */
    struct sockaddr_in remote_addr;   /* remote address */
    socklen_t addrlen;                /* length of addresses -- sizeof(remote_addr) */
    char remote_host[SZ_INFO_BUFF];   /* Last Remote Host ip or node name */
    char error_message[SZ_INFO_BUFF];
    int  error_flag;                  /* != 0 is error */
    char request[SZ_INFO_BUFF];       /* message to send */
    int  req_size;
    char response[SZ_INFO_BUFF];      /* message recieved */
    int  rsp_size;
} ServiceRequest, *PServiceRequest;

struct _UDPServiceProvider {
    char cbName[SZ_CHAR_BUFF];
    char chRegistry[SZ_REGISTRY_BUFF];  /* Responder locator message */
    int iType;     /* 0=inactive, 1=internal, 2=external, 3=threaded-internal, 4-threaded-external */
    pthread_t sp_thread;   /* related thread - likely responder thread */
    int *exit_flag;  /* pointer to exit flag, != 0 will exit */
    int iSocket;   /* broadcast socket */
    ServiceRegistry  registry;
    IPBroadcastArray ipb;
} UDPServiceProvider;

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
extern int gd_i_i2c_address;


/*
 * Service Registry Public Routines
 */
extern PServiceRegistry skn_service_registry_new(char *request);

extern int skn_service_registry_entry_count(PServiceRegistry psr);
extern int skn_service_registry_list_entries(PServiceRegistry psr);
extern PRegistryEntry skn_service_registry_find_entry(PServiceRegistry psreg, char *serviceName);
extern void skn_service_registry_destroy(PServiceRegistry psreg);


/*
 * General Utilities
*/
extern long sknGetNumberCpuCores();
extern int sknGenerateLoadavgInfo(char *msg);
extern int sknGenerateUnameInfo(char *msg);
extern int sknGenerateDatetimeInfo(char *msg);
extern double skn_duration_in_milliseconds(struct timeval *pstart, struct timeval *pend);
extern void skn_program_name_and_description_set(const char *name, const char *desc);
extern int skn_logger(const char *level, const char *format, ...);
extern int skn_handle_locator_command_line(int argc, char **argv);
extern int skn_time_delay_ms(double delay_time);
//extern void skn_delay_microseconds (int delay_us);
extern char * skn_strip(char * alpha);
extern uid_t skn_get_userids();

/*
 * Server/Client Communication Routines
*/
extern int skn_udp_host_create_broadcast_socket(int port, double rcvTimeout);
extern int skn_udp_host_create_regular_socket(int port, double rcvTimeout);
extern PServiceRequest skn_udp_service_provider_service_request_new(PRegistryEntry pre, int host_socket, char *request);

extern int  skn_display_manager_message_consumer_startup(PDisplayManager pdm);
extern void skn_display_manager_message_consumer_shutdown(PDisplayManager pdm);

extern int  skn_get_default_interface_name(char *pchDefaultInterfaceName);
extern int  skn_get_broadcast_ip_array(PIPBroadcastArray paB);
extern void skn_get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);

/*
 * Service Provider
 */
extern int  skn_udp_service_provider_registry_responder(int i_socket, char *response);
extern void skn_udp_service_provider_registry_entry_response_logger(const char * response);
extern int  skn_udp_service_provider_send(PServiceRequest psr);
extern int  skn_service_registry_valiadate_response_format(const char *response);


#endif // SKN_NETWORK_HELPERS_H__
