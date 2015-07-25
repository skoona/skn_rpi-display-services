/*
 * skn_common_headers.h
 *
 *  Created on: Jul 22, 2015
 *      Author: jscott
 */

#ifndef SRC_SKN_COMMON_HEADERS_H_
#define SRC_SKN_COMMON_HEADERS_H_

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

/**
 * #include <sysexits.h>
 * $ man 3 sysexits
 *
 * Program Exit Codes with SystemD in mind
 * ref: http://refspecs.linuxbase.org/LSB_3.1.1/LSB-Core-generic/LSB-Core-generic/iniscrptact.html
 *
	In case of an error while processing any init-script action except
	for status, the init script shall print an error message and exit
	with a non-zero status code:

	1	generic or unspecified error (current practice)
	2	invalid or excess argument(s)
	3	unimplemented feature (for example, "reload")
	4	user had insufficient privilege
	5	program is not installed
	6	program is not configured
	7	program is not running
	8-99	reserved for future LSB use
	100-149	reserved for distribution use
	150-199	reserved for application use
	200-254	reserved
*
*/

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
#define SZ_COMM_BUFF 256

#define ARY_MAX_INTF 8
#define SKN_RUN_MODE_RUN  0
#define SKN_RUN_MODE_STOP 1

#ifndef	TRUE
#  define	TRUE	(1==1)
#  define	FALSE	(1==2)
#endif

#define SCROLL_WAIT    1
#define SCROLL_NOWAIT  0
#define MAX_DISPLAY_ROWS 4
#define PLATFORM_ERROR -1

/*
 * Defines for the Adafruit IC2/SPI LCD Backpack
 * - which use the I2C controller MCP23008
*/
//
// #define  AF_BASE     100
// #define  AF_BACKLIGHT (AF_BASE + 7)
//
// #define  AF_E        (AF_BASE + 2)
// #define  AF_RS       (AF_BASE + 1)
// #define  AF_RW       (AF_BASE + 0)
//
// #define  AF_DB4      (AF_BASE + 3)
// #define  AF_DB5      (AF_BASE + 4)
// #define  AF_DB6      (AF_BASE + 5)
// #define  AF_DB7      (AF_BASE + 6)

typedef struct _IIC {
    int af_base;
    int af_backlight;

    int af_e;
    int af_rs;
    int af_rw;

    int af_db4;
    int af_db5;
    int af_db6;
    int af_db7;
} I2C, *PI2C;


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
	char name[SZ_INFO_BUFF];
	char ip[SZ_INFO_BUFF];
	int port;
} RegistryEntry, *PRegistryEntry;

typedef struct _serviceRegistry {
	char cbName[SZ_CHAR_BUFF];
    int count;  // current number of entries
    int computedMax; // computed container size
    PRegistryEntry entry[ARY_MAX_INTF * 2];
} ServiceRegistry, *PServiceRegistry;

typedef struct _serviceRequest {
	char cbName[SZ_CHAR_BUFF];
	PRegistryEntry pre;
	char request[SZ_INFO_BUFF];
	char response[SZ_INFO_BUFF];
	int socket;
} ServiceRequest, *PServiceRequest;

/*
 * Char bufs for cpu temperature
*/
#define TZ_ADJUST 4
typedef struct _temps {
	char cbName[SZ_CHAR_LABEL];
	char c[SZ_CHAR_LABEL];
	char f[SZ_CHAR_LABEL];
	long raw;
} CpuTemps, *PCpuTemps;

typedef struct _DISPLAY_LINE {
	char cbName[SZ_CHAR_BUFF];
	int  active;
    char ch_display_msg[SZ_INFO_BUFF];
    int  msg_len;
    int  display_pos;
    int  scroll_enabled;
    void * next;
    void * prev;
} DisplayLine, *PDisplayLine;

typedef struct _DISPLAY_MANAGER {
	char cbName[SZ_CHAR_BUFF];
    char ch_welcome_msg[SZ_INFO_BUFF];
    int  msg_len;
    int  display_pos;
    int  dsp_rows;
    int  dsp_cols;
	PDisplayLine pdsp_collection[ARY_MAX_INTF]; // all available lines
	int  current_line; // top of display
    int  next_line;  // actual index  -- should be within display_lines of current
    int  lcd_handle;
    pthread_t dm_thread;   // new message thread
    int  i_socket;
} DisplayManager, *PDisplayManager;


#endif /* SRC_SKN_COMMON_HEADERS_H_ */
