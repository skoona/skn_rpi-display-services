/*
 * skn_commons.c
 *
 *  Created on: Jun 3, 2016
 *      Author: jscott
 */

#include "skn_commons.h"

/*
 * Global Exit Flag -- set by signal handler
 * atomic to survive signal handlers
 */
sig_atomic_t gi_exit_flag = SKN_RUN_MODE_RUN; // will hold the signal which cause exit
char *gd_pch_message = NULL;

signed int gd_i_debug = 0;
char gd_ch_program_name[SZ_INFO_BUFF];
char gd_ch_program_desc[SZ_INFO_BUFF];
char *gd_pch_effective_userid = NULL;
int gd_i_socket = -1;
int gd_i_display = 0;
int gd_i_update = 0;
int gd_i_unique_registry = 0;
char gd_ch_ipAddress[SZ_CHAR_BUFF];
char gd_ch_intfName[SZ_CHAR_BUFF];
char gd_ch_hostName[SZ_CHAR_BUFF];
char gd_ch_hostShortName[SZ_CHAR_BUFF];
char * gd_pch_service_name;
int gd_i_i2c_address = 0;


static void skn_display_service_print_usage();
static void skn_locator_service_print_usage();


/*
 * General System Information Utils */
long skn_util_generate_number_cpu_cores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}


int skn_util_generate_loadavg_info(char *msg) {
    double loadavg[4];
    int rc = 0;

    rc = getloadavg(loadavg, 3);

    if (rc != PLATFORM_ERROR) {
        snprintf(msg, SZ_INFO_BUFF -1, "LoadAvg: 1m=%2.1f, 5m=%2.1f, 15m=%2.1F",
                 loadavg[0], loadavg[1], loadavg[2]);
    } else {
        snprintf(msg, SZ_INFO_BUFF -1, "Load Average: Not Available  %d:%d:%s",
                 rc, errno, strerror(errno));
    }

    return rc;
}

int skn_util_generate_uname_info(char *msg) {
    struct utsname info;

    int mLen = 0;
    char * message = "uname() api failed.";

    if (uname(&info) != 0) {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s", message);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s %s, %s %s | Cores=%ld",
                        info.sysname, info.release, info.version, info.machine,
                        skn_util_generate_number_cpu_cores());
    }
    return mLen;
}

int skn_util_generate_datetime_info(char *msg) {
    int mLen = 0;
    struct tm *t;
    time_t tim;

    tim = time(NULL);
    t = localtime(&tim);

    mLen = snprintf(msg, SZ_INFO_BUFF -1, "%02d:%02d:%04d %02d:%02d:%02d",
                    t->tm_mon + 1, t->tm_mday, t->tm_year + 1900,
                    t->tm_hour, t->tm_min, t->tm_sec);

    return mLen;
}

/**
 * Traditional Millisecond delay.
 */
int skn_util_time_delay_ms(double delay_time) {
    struct timespec timeout;
    if (delay_time == 0.0 || delay_time == 0) delay_time = 0.001;
    timeout.tv_sec = (time_t) delay_time;  // extract integer only
    timeout.tv_nsec = (long) ((delay_time - timeout.tv_sec) * 1000000000L); // 1e+9
    return nanosleep(&timeout, NULL);
}


/**
 * skn_duration_in_microseconds()
 * - expressed in %1.6us  seconds.miroseconds
 */
double skn_util_duration_in_ms(struct timeval *pstart, struct timeval *pend) {
    long secs_used = 0;
    double total_micros_used = 0.0;
    struct timeval end;

    if (pend == NULL) {   // calc running duration
        pend = &end;
        gettimeofday(pend, NULL);
    }

    secs_used=(pend->tv_sec - pstart->tv_sec); //avoid overflow by subtracting first
    total_micros_used = secs_used * 1000.0; // sec to ms
    total_micros_used += (pend->tv_usec - pstart->tv_usec) / 1000.0; // us to ms

    return total_micros_used / 1000.0; // express in seconds.milliseconds
}

/**
 * DELAY FOR # uS WITHOUT SLEEPING
 * Ref: http://www.raspberry-projects.com/pi/programming-in-c/timing/clock_gettime-for-acurate-timing
 *
 * Using delayMicroseconds lets the linux scheduler decide to jump to another process.  Using this function avoids letting the
 * scheduler know we are pausing and provides much faster operation if you are needing to use lots of delays.
 *
 * Fixup incase your building on for OSX, which does not implement clock_gettime()

    #ifdef __MACH__
    #include <sys/time.h>
    #define CLOCK_REALTIME 0

    // *
    // * clock_gettime is not implemented on OSX *
    int clock_gettime(int clk_id, struct timespec* t) {
        struct timeval now;
        int rv = gettimeofday(&now, NULL);
        if (rv) return rv;
        t->tv_sec  = now.tv_sec;
        t->tv_nsec = now.tv_usec * 1000;
        return 0;
    }
    #endif

    void skn_delay_microseconds (int delay_us)
    {
        long int start_time;
        long int time_difference;
        struct timespec gettime_now;

        clock_gettime(CLOCK_REALTIME, &gettime_now);
        start_time = gettime_now.tv_nsec;       //Get nS value
        while (1)
        {
            clock_gettime(CLOCK_REALTIME, &gettime_now);
            time_difference = gettime_now.tv_nsec - start_time;
            if (time_difference < 0)
                time_difference += 1000000000;              //(Rolls over every 1 second)
            if (time_difference > (delay_us * 1000))        //Delay for # nS
                break;
        }
    }
*/

/**
 * Setup effective and real userid
 */
uid_t skn_util_get_userids() {
    uid_t real_user_id = 0;
    uid_t effective_user_id = 0;
    struct passwd *userinfo = NULL;

    real_user_id = getuid();
    if (gd_pch_effective_userid != NULL) {
        userinfo = getpwnam(gd_pch_effective_userid);
        effective_user_id = userinfo->pw_uid;
    } else {
        effective_user_id = geteuid();
        userinfo = getpwuid(effective_user_id);
        gd_pch_effective_userid = userinfo->pw_name;
    }

    return real_user_id;
}

void skn_util_set_program_name_and_description(const char *name, const char *desc) {
    char hostName[SZ_CHAR_BUFF];
    char * phostName = hostName;

    strncpy(gd_ch_program_name, name, sizeof(gd_ch_program_name) - 1);
    strncpy(gd_ch_program_desc, desc, sizeof(gd_ch_program_desc) - 1);

    gethostname(hostName, sizeof(hostName) - 1);
        strncpy(gd_ch_hostName, hostName, sizeof(gd_ch_hostName) - 1);
        strsep(&phostName, ".");
        strncpy(gd_ch_hostShortName, hostName, sizeof(gd_ch_hostShortName) - 1);

}

/**
 * Remove Trailing and Leading Blanks
 * - caution: pointers from argv are readonly and segfault on 'alpha[end] = 0'
 */
char * skn_util_strip(char * alpha) {
    if (alpha == NULL || strlen(alpha) < 1)
        return alpha;

    int len = strlen(alpha);
    int end = len - 1;
    int start = 0;

    // use isgraph() or !isspace() vs isalnum() to allow ['|' ';' '%']
    while (isgraph(alpha[end]) == 0 && end > 0) { // remove trailing non-alphanumeric chars
        alpha[end--] = 0;
    }

    len = strlen(alpha);
    while ((isalnum(alpha[start]) == 0) && start < len) { // find first non-alpha stricter
        start++;
    }
    if (start < len && start > 0) {  // move in place
        end = len - start;
        memmove(&alpha[0], &alpha[start], end);
        alpha[end] = 0;
    }

    return alpha;
}

int skn_util_logger(const char *level, const char *format, ...) {
    va_list args;
    char buffer[SZ_LINE_BUFF];
    const char *logLevel = SD_NOTICE;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (level != NULL) {
        logLevel = level;
    }
    return fprintf(stderr, "%s%s\n", logLevel, buffer);
}


void skn_util_get_default_interface_name_and_ipv4_address(char * intf, char * ipv4) {
    IPBroadcastArray aB;

    if (skn_util_get_broadcast_ip_array(&aB) != PLATFORM_ERROR) {
        memcpy(intf, aB.chDefaultIntfName, SZ_CHAR_BUFF);
        memcpy(ipv4, aB.ipAddrStr[aB.defaultIndex], SZ_CHAR_BUFF);
    } else {
        skn_util_logger(SD_ERR, "InterfaceName and Address: unable to access information.");
    }
}

/**
 * Collect IP and Broadcast Addresses for this machine
 *
 * - Affects the PIPBroadcastArray
 * - Return -1 on error, or count of interfaces
 * - contains this ipAddress in paB->ipAddrStr[paB->defaultIndex]
 */
int skn_util_get_broadcast_ip_array(PIPBroadcastArray paB) {
    struct ifaddrs * ifap;
    struct ifaddrs * p;
    int rc = 0;

    memset(paB, 0, sizeof(IPBroadcastArray));
    paB->count = 0;
    paB->defaultIndex = 0;
    strcpy(paB->cbName, "IPBroadcastArray");

    rc = skn_util_get_default_interface_name(paB->chDefaultIntfName);
    if (rc == EXIT_FAILURE) { // Alternate method for Mac: 'route -n -A inet'
        skn_util_logger(SD_ERR, "No Default Network Interfaces Found!.");
        paB->chDefaultIntfName[0] = 0;
    }
    rc = getifaddrs(&ifap);
    if (rc != 0) {
        skn_util_logger(SD_ERR, "No Network Interfaces Found at All ! %d:%d:%s", rc, errno, strerror(errno) );
        return (PLATFORM_ERROR);
    }
    p = ifap;

    while (p && paB->count < ARY_MAX_INTF) {
        if (p->ifa_addr != NULL && p->ifa_addr->sa_family == AF_INET && ((p->ifa_flags & IFF_BROADCAST) > 0)) {

            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_addr)->sin_addr, paB->ipAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_netmask)->sin_addr, paB->maskAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_broadaddr)->sin_addr, paB->broadAddrStr[paB->count], (SZ_CHAR_BUFF - 1));

            strncpy(paB->ifNameStr[paB->count], p->ifa_name, (SZ_CHAR_BUFF -1));

            /* Take match as the default */
            if (strcmp(paB->chDefaultIntfName, p->ifa_name) == 0) {
                paB->defaultIndex = paB->count;
            }

            paB->count++;
        }
        p = p->ifa_next;
    } // end while
    freeifaddrs(ifap);

    return paB->count;
}

/**
 * Retrieves default internet interface name into param
 * - absolute best way to do this, but not supported on Darwin(i.e OSX)
 * return EXIT_SUCCESS or EXIT_FAILURE
 *
 * [jscott@jscott5365m OSX]$ route -n get 0.0.0.0
 *   route to: default
 *destination: default
 *       mask: default
 *    gateway: 10.21.1.254
 *  interface: en3
 *      flags: <UP,GATEWAY,DONE,STATIC,PRCLONING>
 * recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire
 *       0         0         0         0         0         0      1500         0
 *
*/
int skn_util_get_default_interface_name(char *pchDefaultInterfaceName) {
    FILE *f_route;
    char line[SZ_INFO_BUFF], *dRoute = NULL, *iName = NULL;

    f_route = fopen("/proc/net/route", "r");
    if (f_route != NULL) {
        while (fgets(line, SZ_INFO_BUFF - 1, f_route)) {
            iName = strtok(line, "\t");
            dRoute = strtok(NULL, "\t");

            if (iName != NULL && dRoute != NULL) {
                if (strcmp(dRoute, "00000000") == 0) {
                    strncpy(pchDefaultInterfaceName, iName, (SZ_INFO_BUFF - 1));
                    break;
                }
            }
        }
        fclose(f_route);

        return EXIT_SUCCESS;
    }
    skn_util_logger(SD_ERR, "Opening ProcFs for RouteInfo Failed: %d:%s, Alternate method will be attempted.", errno, strerror(errno));

    f_route = popen("route -n get 0.0.0.0", "r"); // for linux 'route -n -A inet', with interface at line_word[7]
    if (f_route != NULL) {
        while (fgets(line, SZ_INFO_BUFF - 1, f_route)) {
            dRoute = strtok(line, ":");
            iName = strtok(NULL, "\n");
            if (strcmp(skn_util_strip(dRoute), "interface") == 0) {
                strncpy(pchDefaultInterfaceName, skn_util_strip(iName), (SZ_INFO_BUFF - 1));
                break;
            }
        }
        fclose(f_route);

        return EXIT_SUCCESS;
    } else {
        skn_util_logger(SD_ERR, "Alternate method to get RouteInfo Failed: %d:%s", errno, strerror(errno));
        return EXIT_FAILURE;
    }

}

/**************************************************************************
 Function: Print Usage

 Description:
 Output the command-line options for this daemon.

 Params:
 @argc - Standard argument count
 @argv - Standard argument array

 Returns:
 returns void always
 **************************************************************************/
static void skn_locator_service_print_usage() {
    skn_util_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
    skn_util_logger(" ", "\tSkoona Development <skoona@gmail.com>");
    if (strcmp(gd_ch_program_name, "udp_locator_client") == 0) {
        skn_util_logger(" ", "Usage:\n  %s [-v] [-m 'any text msg'] [-u] [-h|--help]", gd_ch_program_name);
        skn_util_logger(" ", "\nOptions:");
        skn_util_logger(" ", "  -u, --unique-registry\t List unique entries from all responses.");
        skn_util_logger(" ", "  -m, --message\tAny text to send; 'stop' cause service to terminate.");
    } else if (strcmp(gd_ch_program_name, "udp_locator_service") == 0) {
        skn_util_logger(" ", "Usage:\n  %s [-s] [-v] [-m '<delimited-response-message-string>'] [-a 'my_service_name'] [-h|--help]", gd_ch_program_name);
        skn_util_logger(" ", "  Format: name=<service-name>,ip=<service-ipaddress>ddd.ddd.ddd.ddd,port=<service-portnumber>ddddd <line-delimiter>");
        skn_util_logger(" ", "  REQUIRED   <line-delimiter> is one of these '|', '%', ';'");
        skn_util_logger(" ", "  example: -m 'name=rpi_locator_service,ip=192.168.1.15,port=48028|name=lcd_display_service, ip=192.168.1.15, port=48029|'");
        skn_util_logger(" ", "\nOptions:");
        skn_util_logger(" ", "  -a, --alt-service-name=my_service_name");
        skn_util_logger(" ", "                       lcd_display_service is default, use this to change name.");
        skn_util_logger(" ", "  -s, --include-display-service\tInclude DisplayService entry in default registry.");
    } else if (strcmp(gd_ch_program_name, "lcd_display_client") == 0) {
        skn_util_logger(" ", "Usage:\n  %s [-v] [-m 'message for display'] [-n 1|300] [-a 'my_service_name'] [-h|--help]", gd_ch_program_name);
        skn_util_logger(" ", "\nOptions:");
        skn_util_logger(" ", "  -a, --alt-service-name=my_service_name");
        skn_util_logger(" ", "                       lcd_display_service is default, use this to change name.");
        skn_util_logger(" ", "  -m, --message\tRequest message to send.");
        skn_util_logger(" ", "  -n, --non-stop=DD\tContinue to send updates every DD seconds until ctrl-break.");
    } else if (strcmp(gd_ch_program_name, "a2d_display_client") == 0) {
        skn_util_logger(" ", "Usage:\n  %s [-v] [-n 1|300] [-i ddd] [-a 'my_service_name'] [-h|--help]", gd_ch_program_name);
        skn_util_logger(" ", "\nOptions:");
        skn_util_logger(" ", "  -a, --alt-service-name=my_service_name");
        skn_util_logger(" ", "                       lcd_display_service is default, use this to change target.");
        skn_util_logger(" ", "  -i, --i2c-address=ddd\tI2C decimal address. | [0x27=39, 0x20=32]");
        skn_util_logger(" ", "  -n, --non-stop=DD\tContinue to send updates every DD seconds until ctrl-break.");
    }
    skn_util_logger(" ", "  -v, --version\tVersion printout.");
    skn_util_logger(" ", "  -h, --help\t\tShow this help screen.");
}

/* *****************************************************
 *  Parse out the command line options from argc,argv
 *  results go to globals
 *  EXIT_FAILURE on any error, or version and help
 *  EXIT_SUCCESS on normal completion
 */
int skn_locator_client_command_line_parse(int argc, char **argv) {
    int opt = 0;
    int longindex = 0;
    struct option longopts[] = { { "include-display-service", 0, NULL, 's' }, /* set true if present */
                                 { "alt-service-name", 1, NULL, 'a' }, /* set true if present */
                                 { "unique-registry", 0, NULL, 'u' }, /* set true if present */
                                 { "non-stop", 1, NULL, 'n' }, /* required param if */
                                 { "debug", 1, NULL, 'd' }, /* required param if */
                                 { "message", 1, NULL, 'm' }, /* required param if */
                                 { "i2c-address", 1, NULL, 'i' }, /* required param if */
                                 { "version", 0, NULL, 'v' }, /* set true if present */
                                 { "help", 0, NULL, 'h' }, /* set true if present */
                                 { 0, 0, 0, 0 } };

    while ((opt = getopt_long(argc, argv, "d:m:n:i:a:usvh", longopts, &longindex)) != -1) {
        switch (opt) {
            case 'u':
                gd_i_unique_registry = 1;
                break;
            case 's':
                gd_i_display = 1;
                break;
            case 'd':
                if (optarg) {
                    gd_i_debug = atoi(optarg);
                } else {
                    skn_util_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'm':
                if (optarg) {
                    gd_pch_message = strdup(optarg);
                } else {
                    skn_util_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'n':
                if (optarg) {
                    gd_i_update = atoi(optarg);
                } else {
                    skn_util_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'a':
                if (optarg) {
                    gd_pch_service_name = strdup(optarg);
                } else {
                    skn_util_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'i':
                if (optarg) {
                    gd_i_i2c_address = atoi(optarg);
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'v':
                skn_util_logger(SD_WARNING, "\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n", gd_ch_program_name, PACKAGE_VERSION);
                return (EXIT_FAILURE);
                break;
            case '?':
                skn_util_logger(SD_WARNING, "%s: unknown input param! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                skn_locator_service_print_usage();
                return (EXIT_FAILURE);
                break;
            default: /* help and default */
                skn_locator_service_print_usage();
                return (EXIT_FAILURE);
                break;
        }
    }

    return EXIT_SUCCESS;
}

/**************************************************************************
 Function: Print Usage for lcd_display_service only

 Description:
 Output the command-line options for this daemon.

 Returns:
 returns void always
 **************************************************************************/
static void skn_display_service_print_usage() {
    skn_util_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
    skn_util_logger(" ", "\tSkoona Development <skoona@gmail.com>");
    skn_util_logger(" ", "Usage:\n  %s [-v] [-m 'Welcome Message'] [-r 4|2] [-c 20|16] [-i 39|32] [-t pcf|mcp|mc7|ser] [-p string] [-h|--help]", gd_ch_program_name);
    skn_util_logger(" ", "\nOptions:");
    skn_util_logger(" ", "  -r, --rows=dd\t\tNumber of rows in physical display.");
    skn_util_logger(" ", "  -c, --cols=dd\t\tNumber of columns in physical display.");
    skn_util_logger(" ", "  -m, --message\tWelcome Message for line 1.");
    skn_util_logger(" ", "  -p, --serial-port=string\tSerial port.      | ['/dev/ttyACM0']");
    skn_util_logger(" ", "  -i, --i2c-address=ddd\tI2C decimal address. | [0x27=39, 0x20=32]");
    skn_util_logger(" ", "  -t, --i2c-chipset=pcf\tI2C Chipset.         | [pcf|mc7|mcp|ser]");
    skn_util_logger(" ", "  -v, --version\tVersion printout.");
    skn_util_logger(" ", "  -h, --help\t\tShow this help screen.");
}

/* *****************************************************
 *  Parse out the command line options from argc,argv
 *  results go to globals
 *  EXIT_FAILURE on any error, or version and help
 *  EXIT_SUCCESS on normal completion
 */
int skn_display_service_command_line_parse(int argc, char **argv) {
    int opt = 0;
    int longindex = 0;
    struct option longopts[] = {
            { "debug", 1, NULL, 'd' }, /* required param if */
            { "rows", 1, NULL, 'r' }, /* required param if */
            { "cols", 1, NULL, 'c' }, /* required param if */
            { "message", 1, NULL, 'm' }, /* required param if */
            { "i2c-address", 1, NULL, 'i' }, /* required param if */
            { "12c-chipset", 1, NULL, 't' }, /* required param if */
            { "serial-port", 1, NULL, 'p' }, /* required param if */
            { "version", 0, NULL, 'v' }, /* set true if present */
            { "help", 0, NULL, 'h' }, /* set true if present */
            { 0, 0, 0, 0 } };

    while ((opt = getopt_long(argc, argv, "d:m:r:c:i:t:p:vh", longopts, &longindex)) != -1) {
        switch (opt) {
            case 'd':
                if (optarg) {
                    gd_i_debug = atoi(optarg);
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'r':
                if (optarg) {
                    gd_i_rows = atoi(optarg);
                    switch (gd_i_rows) {
                        case 2:
                        case 4:
                            break;
                        default:
                            gd_i_rows = 4;
                            skn_util_logger(SD_WARNING, "%s: input param was invalid! (default of 4 used) %c[%d:%d:%d]\n", gd_ch_program_name,
                                            (char) opt, longindex, optind, opterr);

                    }
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'c':
                if (optarg) {
                    gd_i_cols = atoi(optarg);
                    switch (gd_i_cols) {
                        case 16:
                        case 20:
                            break;
                        default:
                            gd_i_cols = 20;
                            skn_util_logger(SD_WARNING, "%s: input param was invalid! (default of 20 used) %c[%d:%d:%d]\n", gd_ch_program_name,
                                            (char) opt, longindex, optind, opterr);

                    }
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'm':
                if (optarg) {
                    gd_pch_message = strdup(optarg);
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'i':
                if (optarg) {
                    gd_i_i2c_address = atoi(optarg);
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 't':
                if (optarg) {
                    gd_pch_device_name = strdup(optarg);
                    if ( (strcmp(gd_pch_device_name, "mcp") != 0) &&
                         (strcmp(gd_pch_device_name, "mc7") != 0) &&
                         (strcmp(gd_pch_device_name, "pcf") != 0) &&
                         (strcmp(gd_pch_device_name, "ser") != 0)) {
                        skn_util_logger(SD_ERR, "%s: unsupported option was invalid! %c[%d:%d:%d] %s\n", gd_ch_program_name, (char) opt, longindex, optind, opterr, gd_pch_device_name);
                        return EXIT_FAILURE;
                    }
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'p':
                if (optarg) {
                    gd_pch_serial_port = strdup(optarg);
                    if (strlen(gd_pch_serial_port) < 5) {
                        skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                        return EXIT_FAILURE;
                    }
                } else {
                    skn_util_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'v':
                skn_util_logger(SD_ERR, "\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n", gd_ch_program_name,
                                PACKAGE_VERSION);
                return (EXIT_FAILURE);
                break;
            case '?':
                skn_util_logger(SD_ERR, "%s: unknown input param! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                skn_display_service_print_usage();
                return (EXIT_FAILURE);
                break;
            default: /* help and default */
                skn_display_service_print_usage();
                return (EXIT_FAILURE);
                break;
        }
    }

    return EXIT_SUCCESS;
}
