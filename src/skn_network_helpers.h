/**
 * skn_network_helpers.h
 *
 *
 */

#ifndef SKN_NETWORK_HELPERS_H__
#define SKN_NETWORK_HELPERS_H__

#include <sys/types.h>
#include <pwd.h>        /* umask() */
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <time.h>
#include <stdarg.h>


//#include <systemd/sd-daemon.h>

#define SD_EMERG   "<0>"  /* system is unusable */
#define SD_ALERT   "<1>"  /* action must be taken immediately */
#define SD_CRIT    "<2>"  /* critical conditions */
#define SD_ERR     "<3>"  /* error conditions */
#define SD_WARNING "<4>"  /* warning conditions */
#define SD_NOTICE  "<5>"  /* normal but significant condition */
#define SD_INFO    "<6>"  /* informational */
#define SD_DEBUG   "<7>"  /* debug-level messages */


/*
 * Program Standards passed from compiler
 */
#ifndef PACKAGE_VERSION
	#define PACKAGE_VERSION "1.1"
#endif
#ifndef PACKAGE_NAME
	#define PACKAGE_NAME "lcd_display_service"
#endif
#ifndef PACKAGE_DESCRIPTION
	#define PACKAGE_DESCRIPTION "Locate Raspberry Pi's on the network."
#endif


#define SKN_FIND_RPI_PORT 48028
#define SKN_RPI_DISPLAY_SERVICE_PORT 48029

#define SZ_CHAR_LABEL 48
#define SZ_INFO_BUFF 256
#define SZ_CHAR_BUFF 128
#define SZ_LINE_BUFF 256
#define SZ_COMM_BUFF 1024

#define ARY_MAX_INTF 8

#define SKN_RUN_MODE_RUN  0
#define SKN_RUN_MODE_STOP 1


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

typedef struct _serviceEntry {
	char cbName[SZ_CHAR_BUFF];
	char name[SZ_CHAR_BUFF];
	char ip[SZ_CHAR_BUFF];
	int port;
} RegistryEntry, *PRegistryEntry;

typedef struct _serviceRegistry {
	char cbName[SZ_CHAR_BUFF];
    int count;  // current number of entries
    int computedMax; // computed container size
    PRegistryEntry entry[ARY_MAX_INTF * 2];
} ServiceRegistry, *PServiceRegistry;


/*
 * Globals defined in skn_network_helpers.c
*/
extern sig_atomic_t gi_exit_flag;
extern char *gd_pch_message;
extern signed int gd_i_debug;
extern int gd_i_socket;
extern char gd_ch_program_name[SZ_LINE_BUFF];
extern char gd_ch_program_desc[SZ_LINE_BUFF];
extern char *gd_pch_effective_userid;
extern char gd_ch_ipAddress[NI_MAXHOST];
extern char gd_ch_intfName[SZ_CHAR_BUFF];


/*
 * General Utilities
*/
extern uid_t skn_get_userids();
extern void skn_program_name_and_description_set(const char *name, const char *desc);
extern char * skn_strip(char * alpha);
extern int skn_logger(const char *level, const char *format, ...);
extern int skn_handle_locator_command_line(int argc, char **argv);
extern void signals_init();
extern void signals_cleanup(int sig);

extern int skn_signal_manager_startup(pthread_t *psig_thread, sigset_t *psignal_set, uid_t *preal_user_id);
extern int skn_signal_manager_shutdown(pthread_t sig_thread, sigset_t *psignal_set);


/*
 * Server/Client Communication Routines
*/
extern int host_socket_init(int port, int rcvTimeout);
extern int get_default_interface_name(char *pchDefaultInterfaceName);
extern int get_broadcast_ip_array(PIPBroadcastArray paB);
extern void log_response_message(const char * response);
extern void get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);

/*
 * Service Registry Public Routines
 */
extern int service_registry_valiadate_response_format(const char *response);
extern int service_registry_provider(int i_socket, char *response);
extern PServiceRegistry service_registry_get_via_udp_broadcast(int i_socket, char *request);
extern int service_registry_entry_count(PServiceRegistry psr);
extern int service_registry_list_entries(PServiceRegistry psr);
extern PRegistryEntry service_registry_find_entry(PServiceRegistry psreg, char *serviceName);
extern void * service_registry_get_entry_ref(PRegistryEntry prent, char *field);
extern void service_registry_destroy(PServiceRegistry psreg);

#endif // SKN_NETWORK_HELPERS_H__
