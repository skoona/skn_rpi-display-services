/**
 * sknNetworkHelpers.c
 */

#include "skn_network_helpers.h"

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
char * gd_pch_service_name;


static void skn_locator_print_usage();
static void exit_handler(int sig);


static void * service_registry_entry_create_helper(char *key, char **name, char **ip, char **port);
static PServiceRegistry service_registry_create();
static int service_registry_entry_create(PServiceRegistry psreg, char *name, char *ip, char *port, int *errors);
static int service_registry_response_parse(PServiceRegistry psreg, const char *response, int *errors);

/*
 * General System Information Utils */
long skn_get_number_of_cpu_cores() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

int generate_uname_info(char *msg) {
    struct utsname info;

    int mLen = 0;
    char * message = "uname() api failed.";

    if (uname(&info) != 0) {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s", message);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s:%s %s, %s %s", info.nodename, info.sysname, info.release, info.version, info.machine);
    }
    return mLen;
}

int generate_datetime_info(char *msg) {
    int mLen = 0;
    struct tm *t;
    time_t tim;

    tim = time(NULL);
    t = localtime(&tim);

    mLen = snprintf(msg, SZ_INFO_BUFF -1, "%02d:%02d:%04d %02d:%02d:%02d", t->tm_mon + 1, t->tm_mday, t->tm_year + 1900,
                    ((t->tm_hour - TZ_ADJUST) < 0 ? (t->tm_hour - TZ_ADJUST + 12) : (t->tm_hour - TZ_ADJUST)), t->tm_min, t->tm_sec);

    return mLen;
}

/**
 * Setup effective and real userid
 */
uid_t skn_get_userids() {
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
/**
 * Control-C Program exit
 * ref: http://www.cons.org/cracauer/sigint.html
 *      http://www.chemie.fu-berlin.de/chemnet/use/info/libc/libc_21.html#SEC361
 */
static void exit_handler(int sig) {
    gi_exit_flag = sig;
    skn_logger(SD_NOTICE, "Program Exiting, from signal=%d:%s\n", sig, strsignal(sig));
}

void signals_init() {
    signal(SIGINT, exit_handler);  // Ctrl-C
    signal(SIGQUIT, exit_handler);  // Quit
    signal(SIGTERM, exit_handler);  // Normal kill command
}

void signals_cleanup(int sig) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    if (gi_exit_flag > SKN_RUN_MODE_RUN) { // exit caused by some interrupt -- otherwise it would be exactly 0
        kill(getpid(), sig);
    }
}


void get_default_interface_name_and_ipv4_address(char * intf, char * ipv4) {
    IPBroadcastArray aB;

    if (get_broadcast_ip_array(&aB) != PLATFORM_ERROR) {
        memcpy(intf, aB.chDefaultIntfName, SZ_CHAR_BUFF);
        memcpy(ipv4, aB.ipAddrStr[aB.defaultIndex], SZ_CHAR_BUFF);
    } else {
        skn_logger(SD_ERR, "InterfaceName and Address: unable to access information.");
    }
}

/**
 * Collect IP and Broadcast Addresses for this machine
 *
 * - Affects the PIPBroadcastArray
 * - Return -1 on error, or count of interfaces
 * - contains this ipAddress in paB->ipAddrStr[paB->defaultIndex]
 */
int get_broadcast_ip_array(PIPBroadcastArray paB) {
    struct ifaddrs * ifap;
    struct ifaddrs * p;
    int rc = 0;

    memset(paB, 0, sizeof(IPBroadcastArray));
    paB->count = 0;
    paB->defaultIndex = 0;
    strcpy(paB->cbName, "Net Broadcast IPs");

//    rc = get_default_interface_name(paB->chDefaultIntfName);
//    if (rc == EXIT_FAILURE) {
//        skn_logger(SD_ERR, "No Default Network Interfaces Found!. Count=%d", rc);
//        return PLATFORM_ERROR;
//    }
    rc = getifaddrs(&ifap);
    if (rc != 0) {
        skn_logger(SD_ERR, "No Network Interfaces Found at All ! %d:%d:%s", rc, errno, strerror(errno));
        return (PLATFORM_ERROR);
    }
    p = ifap;

    while (p && paB->count < ARY_MAX_INTF) {
        if (p->ifa_addr != NULL && p->ifa_addr->sa_family == AF_INET && ((p->ifa_flags & IFF_BROADCAST) > 0)) {

            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_addr)->sin_addr, paB->ipAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_netmask)->sin_addr, paB->maskAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_broadaddr)->sin_addr, paB->broadAddrStr[paB->count], (SZ_CHAR_BUFF - 1));

            strncpy(paB->ifNameStr[paB->count], p->ifa_name, (SZ_CHAR_BUFF -1));

            /* Take first one to be the default */
            if ((strcmp(paB->maskAddrStr[paB->count], "255.0.0.0") != 0) && (paB->chDefaultIntfName[0] != 0)) {
                strncpy(paB->chDefaultIntfName, p->ifa_name, (SZ_CHAR_BUFF -1));
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
 */
int get_default_interface_name(char *pchDefaultInterfaceName) {
    FILE *f_route;
    char line[SZ_INFO_BUFF], *dRoute = NULL, *iName = NULL;

    f_route = fopen("/proc/net/route", "r");
    if (f_route == NULL) {
        skn_logger(SD_ERR, "Opening RouteInfo Failed: %d:%s", errno, strerror(errno));
        return EXIT_FAILURE;
    }

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
static void skn_locator_print_usage() {
    skn_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
    skn_logger(" ", "\tSkoona Development <skoona@gmail.com>");
    if (strcmp(gd_ch_program_name, "udp_locator_client") == 0) {
        skn_logger(" ", "Usage:\n  %s [-v] [-m 'any text msg'] [-u] [-h|--help]", gd_ch_program_name);
        skn_logger(" ", "\nOptions:");
        skn_logger(" ", "  -u, --unique-registry\t List unique entries from all responses.");
        skn_logger(" ", "  -m, --message\tAny text to send; 'stop' cause service to terminate.");
    } else if (strcmp(gd_ch_program_name, "udp_locator_service") == 0) {
        skn_logger(" ", "Usage:\n  %s [-s] [-v] [-m '<delimited-response-message-string>'] [-a 'my_service_name'] [-h|--help]", gd_ch_program_name);
        skn_logger(" ", "  Format: name=<service-name>,ip=<service-ipaddress>ddd.ddd.ddd.ddd,port=<service-portnumber>ddddd <line-delimiter>");
        skn_logger(" ", "  REQUIRED   <line-delimiter> is one of these '|', '%', ';'");
        skn_logger(" ", "  example: -m 'name=rpi_locator_service,ip=192.168.1.15,port=48028|name=lcd_display_service, ip=192.168.1.15, port=48029|'");
        skn_logger(" ", "\nOptions:");
        skn_logger(" ", "  -a, --alt-service-name=my_service_name");
        skn_logger(" ", "                       lcd_display_service is default, use this to change name.");
        skn_logger(" ", "  -s, --include-display-service\tInclude DisplayService entry in default registry.");
    } else if (strcmp(gd_ch_program_name, "lcd_display_client") == 0) {
        skn_logger(" ", "Usage:\n  %s [-v] [-m 'message for display'] [-n 1|300] [-a 'my_service_name'] [-h|--help]", gd_ch_program_name);
        skn_logger(" ", "\nOptions:");
        skn_logger(" ", "  -a, --alt-service-name=my_service_name");
        skn_logger(" ", "                       lcd_display_service is default, use this to change name.");
        skn_logger(" ", "  -m, --message\tRequest message to send.");
        skn_logger(" ", "  -n, --non-stop=DD\tContinue to send updates every DD seconds until ctrl-break.");
    }
    skn_logger(" ", "  -v, --version\tVersion printout.");
    skn_logger(" ", "  -h, --help\t\tShow this help screen.");
}

/* *****************************************************
 *  Parse out the command line options from argc,argv
 *  results go to globals
 *  EXIT_FAILURE on any error, or version and help
 *  EXIT_SUCCESS on normal completion
 */
int skn_handle_locator_command_line(int argc, char **argv) {
    int opt = 0;
    int longindex = 0;
    struct option longopts[] = { { "include-display-service", 0, NULL, 's' }, /* set true if present */
                                 { "alt-service-name", 1, NULL, 'a' }, /* set true if present */
                                 { "unique-registry", 0, NULL, 'u' }, /* set true if present */
                                 { "non-stop", 1, NULL, 'n' }, /* required param if */
                                 { "debug", 1, NULL, 'd' }, /* required param if */
                                 { "message", 1, NULL, 'm' }, /* required param if */
                                 { "version", 0, NULL, 'v' }, /* set true if present */
                                 { "help", 0, NULL, 'h' }, /* set true if present */
                                 { 0, 0, 0, 0 } };

    /*
     * Get commandline options
     *  longindex is the current index into longopts
     *  optind is current/last index into argv
     *  optarg is value attached(-d88) or next element(-d 88) of argv
     *  opterr flags a scanning error
     */
    while ((opt = getopt_long(argc, argv, "d:m:n:a:usvh", longopts, &longindex)) != -1) {
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
                    skn_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'm':
                if (optarg) {
                    gd_pch_message = strdup(optarg);
                } else {
                    skn_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'n':
                if (optarg) {
                    gd_i_update = atoi(optarg);
                } else {
                    skn_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'a':
                if (optarg) {
                    gd_pch_service_name = strdup(optarg);
                } else {
                    skn_logger(SD_WARNING, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'v':
                skn_logger(SD_WARNING, "\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n", gd_ch_program_name, PACKAGE_VERSION);
                return (EXIT_FAILURE);
                break;
            case '?':
                skn_logger(SD_WARNING, "%s: unknown input param! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                skn_locator_print_usage();
                return (EXIT_FAILURE);
                break;
            default: /* help and default */
                skn_locator_print_usage();
                return (EXIT_FAILURE);
                break;
        }
    }

    return EXIT_SUCCESS;
}

void log_response_message(const char * response) {
    char * worker = NULL, *parser = NULL, *base = strdup(response);
    parser = base;
    skn_logger(SD_NOTICE, "Response Message:");
    while ((((worker = strsep(&parser, "|")) != NULL) || ((worker = strsep(&parser, "%")) != NULL) || ((worker = strsep(&parser, ";")) != NULL))
                    && (strlen(worker) > 16)) {
        skn_logger(SD_NOTICE, "[%s]", worker);
    }
    free(base);
}

void skn_program_name_and_description_set(const char *name, const char *desc) {
    strncpy(gd_ch_program_name, name, sizeof(gd_ch_program_name) - 1);
    strncpy(gd_ch_program_desc, desc, sizeof(gd_ch_program_desc) - 1);
}

/**
 * Remove Trailing and Leading Blanks
 * - caution: pointers from argv are readonly and segfault on 'alpha[end] = 0'
 */
char * skn_strip(char * alpha) {
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

int skn_logger(const char *level, const char *format, ...) {
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

/**
 * skn_udp_host_create_regular_socket()
 * - creates a dgram socket without broadcast enabled
 *
 * - returns i_socket | EXIT_FAILURE
 */
int skn_udp_host_create_regular_socket(int port, int rcvTimeout) {
    struct sockaddr_in addr;
    int i_socket, reuseEnable = 1;

    if ((i_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        skn_logger(SD_EMERG, "Create Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    if ((setsockopt(i_socket, SOL_SOCKET, SO_REUSEADDR, &reuseEnable, sizeof(reuseEnable))) < 0) {
        skn_logger(SD_EMERG, "Set Socket Reuse Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = rcvTimeout;
    tv.tv_usec = 0;
    if ((setsockopt(i_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        skn_logger(SD_EMERG, "Set Socket RcvTimeout Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    /* set up local address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(i_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        skn_logger(SD_EMERG, "Bind to local Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    return i_socket;
}

/**
 * skn_udp_host_create_broadcast_socket()
 * - creates a dgram socket with broadcast enabled
 *
 * - returns i_socket | EXIT_FAILURE
 */
int skn_udp_host_create_broadcast_socket(int port, int rcvTimeout) {
    struct sockaddr_in addr;
    int i_socket, broadcastEnable = 1;

    if ((i_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        skn_logger(SD_EMERG, "Create Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }
    if ((setsockopt(i_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) < 0) {
        skn_logger(SD_EMERG, "Set Socket Broadcast Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = rcvTimeout;
    tv.tv_usec = 0;
    if ((setsockopt(i_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        skn_logger(SD_EMERG, "Set Socket RcvTimeout Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    /* set up local address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(i_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        skn_logger(SD_EMERG, "Bind to local Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    return i_socket;
}

PServiceRequest skn_service_request_create(PRegistryEntry pre, int host_socket, char *request) {
    PServiceRequest psr = NULL;

    if (pre == NULL) {
        return NULL;
    }
    psr = (PServiceRequest) malloc(sizeof(ServiceRequest));
    memset(psr, 0, sizeof(ServiceRequest));
    strcpy(psr->cbName, "PServiceRequest");
    psr->socket = host_socket;
    psr->pre = pre;
    strncpy(psr->request, request, SZ_INFO_BUFF-1);
    return psr;
}

/**
 * skn_duration_in_microseconds()
 * - expressed in %1.6us  seconds.miroseconds
 */
double skn_duration_in_milliseconds(struct timeval *pstart, struct timeval *pend) {
    long secs_used = 0;
    double total_micros_used = 0.0;
    struct timeval end;

    if (pend == NULL) {   // calc running duration
        pend = &end;
        gettimeofday(pend, NULL);
    }

    secs_used=(pend->tv_sec - pstart->tv_sec); //avoid overflow by subtracting first
    total_micros_used= ((secs_used*1000000) + pend->tv_usec) - (pstart->tv_usec);

    return total_micros_used / 1000000000; // express in seconds.milliseconds
}

/**
 * skn_udp_service_request()
 * - side effects: none
 *
 * - returns EXIT_SUCCESS | EXIT_FAILURE
 */
int skn_udp_service_request(PServiceRequest psr) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    signed int vIndex = 0;
    struct timeval start, end;

    memset(&remaddr, 0, sizeof(remaddr));
    remaddr.sin_family = AF_INET;
    remaddr.sin_addr.s_addr = inet_addr(psr->pre->ip);
    remaddr.sin_port = htons(psr->pre->port);

    /*
     * SEND */
    gettimeofday(&start, NULL);
    if (sendto(psr->socket, psr->request, strlen(psr->request), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
        skn_logger(SD_WARNING, "ServiceRequest: SendTo() Timed out; Failure code=%d, etext=%s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    skn_logger(SD_NOTICE, "ServiceRequest sent to %s:%s:%d", psr->pre->name, psr->pre->ip, psr->pre->port);

    /*
     * RECEIVE */
    vIndex = recvfrom(psr->socket, psr->response, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen);
    if (vIndex == PLATFORM_ERROR) {
        skn_logger(SD_WARNING, "ServiceRequest: recvfrom() Failure code=%d, etext=%s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    psr->response[vIndex] = 0;
    gettimeofday(&end, NULL);

    skn_logger(SD_INFO, "Response(%1.6Fs) received from [%s] %s:%d",
                    skn_duration_in_milliseconds(&start,&end),
                    psr->response,
                    inet_ntoa(remaddr.sin_addr),
                    ntohs(remaddr.sin_port)
              );

    if (strcmp(psr->response, "QUIT!") == 0) {
        skn_logger(SD_NOTICE, "Shutdown Requested!");
        return EXIT_FAILURE;
    }

    return (EXIT_SUCCESS);
}

int service_registry_provider(int i_socket, char *response) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    IPBroadcastArray aB;
    char request[SZ_INFO_BUFF];
    char recvHostName[SZ_INFO_BUFF];
    signed int rLen = 0, rc = 0;
    int exit_code = EXIT_SUCCESS, i_response_len = 0;


    memset(request, 0, sizeof(request));
    memset(recvHostName, 0, sizeof(recvHostName));

    rc = get_broadcast_ip_array(&aB);
    if (rc == PLATFORM_ERROR) {
        return EXIT_FAILURE;
    }

    if (gd_pch_service_name == NULL) {
        gd_pch_service_name = "lcd_display_service";
    }

    if (strlen(response) < 16) {
        if (gd_i_display) {
            i_response_len = snprintf(response, (SZ_INFO_BUFF - 1),
                     "name=rpi_locator_service,ip=%s,port=%d|"
                     "name=%s,ip=%s,port=%d|",
                     aB.ipAddrStr[aB.defaultIndex], SKN_FIND_RPI_PORT,
                     gd_pch_service_name,
                     aB.ipAddrStr[aB.defaultIndex], SKN_RPI_DISPLAY_SERVICE_PORT);
        } else {
            i_response_len = snprintf(response, (SZ_INFO_BUFF - 1),
                            "name=rpi_locator_service,ip=%s,port=%d|",
                            aB.ipAddrStr[aB.defaultIndex], SKN_FIND_RPI_PORT);
        }
    }
    log_response_message(response);

    skn_logger(SD_DEBUG, "Socket Bound to %s:%s", aB.chDefaultIntfName, aB.ipAddrStr[aB.defaultIndex]);

    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port = htons(SKN_FIND_RPI_PORT);
        remaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        addrlen = sizeof(remaddr);

        if ((rLen = recvfrom(i_socket, request, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen)) < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            skn_logger(SD_ERR, "RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }
        request[rLen] = 0;

        rc = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, (SZ_INFO_BUFF-1), NULL, 0, NI_DGRAM);
        if (rc != 0) {
            skn_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }
        skn_logger(SD_NOTICE, "Received request from %s @ %s:%d", recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));
        skn_logger(SD_NOTICE, "Request data: [%s]\n", request);

        /*
         * Add new registry entry by command */
        if ((strncmp("ADD ", request, sizeof("ADD")) == 0) &&
            (service_registry_valiadate_response_format(&request[4]) == EXIT_SUCCESS)) {
            if ((response[i_response_len-1] == '|') ||
                (response[i_response_len-1] == '%') ||
                (response[i_response_len-1] == ';')) {
                strncpy(&response[i_response_len], &request[4], ((SZ_COMM_BUFF - 1) - (strlen(response) + strlen(&request[4]))) );
                i_response_len += (rLen - 4);
                skn_logger(SD_NOTICE, "COMMAND: Add New RegistryEntry Request Accepted!");
            }
        }


        if (sendto(i_socket, response, strlen(response), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_logger(SD_EMERG, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }

        /*
         * Shutdown by command */
        if (strcmp("QUIT!", request) == 0) {
            skn_logger(SD_NOTICE, "COMMAND: Shutdown Requested! exit code=%d", gi_exit_flag);
            break;
        }
    }

    return exit_code;
}

/**
 * Remote Service Registry routines
 */
static PServiceRegistry service_registry_create() {
    PServiceRegistry psreg = NULL;

    psreg = (PServiceRegistry) malloc(sizeof(ServiceRegistry));
    if (psreg != NULL) {
        memset(psreg, 0, sizeof(ServiceRegistry));
        strcpy(psreg->cbName, "PServiceRegistry");
        psreg->computedMax = (sizeof(psreg->entry) / sizeof(void *));
        psreg->count = 0;
    }

    return psreg;
}
static int service_registry_entry_create(PServiceRegistry psreg, char *name, char *ip, char *port, int *errors) {
    PRegistryEntry prent = NULL;

    if ((psreg == NULL) || (name == NULL) || (ip == NULL) || (port == NULL)) {
        skn_logger(SD_DEBUG, "Parse failure missing value: (%s,%s,%s)", name, ip, port);
        if (errors != NULL)
            (*errors)++;
        return EXIT_FAILURE;
    }

    if (psreg->count >= psreg->computedMax) {
        skn_logger(SD_WARNING, "Capacity Error: Too many! New entry %d:%s exceeds maximum of %d allowed! ", psreg->count, name, psreg->computedMax);
        if (errors != NULL)
            (*errors)++;
        return EXIT_FAILURE;
    }

    /* update or create entry */
    if (gd_i_unique_registry) {
        prent = service_registry_find_entry(psreg, name);
    }
    if (prent == NULL) {
        prent = (PRegistryEntry) malloc(sizeof(RegistryEntry));
        if (prent != NULL) {
            psreg->entry[psreg->count] = prent;
            psreg->count++;
        }
    }

    if (prent != NULL) {
        memset(prent, 0, sizeof(RegistryEntry));
        strcpy(prent->cbName, "PRegistryEntry");
        strcpy(prent->name, name);
        strcpy(prent->ip, ip);
        prent->port = atoi(port);
    } else {
        skn_logger(SD_WARNING, "Internal Memory Error: Could not allocate memory for entry %d:%s !", name, psreg->count);
        if (errors != NULL)
            (*errors)++;

        return EXIT_FAILURE;
    }

    return psreg->count;
}

/**
 * Validate the command line response format
 */
int service_registry_valiadate_response_format(const char *response) {
    int errors = 0; // false

    PServiceRegistry psr = service_registry_create();
    service_registry_response_parse(psr, response, &errors);
    service_registry_destroy(psr);

    if (errors > 0) {
        return EXIT_FAILURE; // false
    } else {
        return EXIT_SUCCESS; // true
    }
}
/**
 * Validate the command line response format and return registry
 */
PServiceRegistry service_registry_valiadated_registry(const char *response) {
    int errors = 0;

    PServiceRegistry psr = service_registry_create();
    service_registry_response_parse(psr, response, &errors);
    if (errors > 0) {
        free(psr);
        return NULL; // false
    }

    return psr;
}

PRegistryEntry service_registry_find_entry(PServiceRegistry psreg, char *serviceName) {
    PRegistryEntry prent = NULL;
    int index = 0;

    if (psreg == NULL)
        return NULL;

    while (index < psreg->count) {
        if (strcmp(serviceName, psreg->entry[index]->name) == 0) {
            prent = psreg->entry[index];
            break;
        }
        index++;
    }

    return prent;
}

int service_registry_entry_count(PServiceRegistry psr) {
    return psr->count;
}

int service_registry_list_entries(PServiceRegistry psr) {
    int index = 0;

    for (index = 0; index < psr->count; index++) {
        skn_logger(SD_INFO, "RegistryEntry(%02d) name=%s, ip=%s, port=%d", index, psr->entry[index]->name, psr->entry[index]->ip, psr->entry[index]->port);
    }
    return index;
}

/**
 * compute the address of each field on demand, from struct
 */
void * service_registry_get_entry_ref(PRegistryEntry prent, char *field) {
    int index = 0;
    void * result = NULL;
    char * names[4] = { "name", "ip", "port",
    NULL };
    int offsets[4] = { offsetof(RegistryEntry, name), offsetof(RegistryEntry, ip), offsetof(RegistryEntry, port), 0 };
    for (index = 0; names[index] != NULL; index++) {
        if (strcmp(names[index], field) == 0) {
            result = (void *) prent + offsets[index];
            break;
        }
    }

    return result;
}

/**
 * compute the address of each field on demand, from local vars
 */
static void * service_registry_entry_create_helper(char *key, char **name, char **ip, char **port) {
    int index = 0;
    char * guess = NULL;
    void * result = NULL;
    char * names[4] = { "name", "ip", "port", NULL };
    void * offsets[] = { name, ip, port, NULL };

    skn_strip(key); // cleanup first
    for (index = 0; names[index] != NULL; index++) { // find direct match
        if (strcmp(names[index], key) == 0) {
            result = offsets[index];
            break;
        }
    }
    if (result == NULL) { // try to guess what the key is
        if ((guess = strstr(key, "e")) != NULL) {
            result = offsets[0];
        } else if ((guess = strstr(key, "i")) != NULL) {
            result = offsets[1];
        } else if ((guess = strstr(key, "t")) != NULL) {
            result = offsets[2];
        }
    }

    return result;
}

/**
 * Parse this response message, guess spellings, skip invalid, export error count
 *
 name=rpi_locator_service,ip=10.100.1.19,port=48028|
 name=lcd_display_service,ip=10.100.1.19,port=48029|
 *  the vertical bar char '|' is the line separator, % and ; are also supported
 *
 */
static int service_registry_response_parse(PServiceRegistry psreg, const char *response, int *errors) {
    int control = 1;
    char *base = NULL, *psep = NULL, *resp = NULL, *line = NULL, *keypair = NULL, *element = NULL, *name = NULL, *ip = NULL, *pport = NULL, **meta = NULL;

    base = resp = strdup(response);

    if (strstr(response, "|")) {
        psep = "|";
    } else if (strstr(response, "%")) {
        psep = "%";
    } else if (strstr(response, ";")) {
        psep = ";";
    }

    while ((line = strsep(&resp, psep)) != NULL) {
        if (strlen(line) < 16) {
            continue;
        }

        pport = ip = name = NULL;
        while ((keypair = strsep(&line, ",")) != NULL) {
            if (strlen(keypair) < 1) {
                continue;
            }

            control = 1;
            element = strstr(keypair, "=");
            if (element != NULL) {
                element[0] = 0;
                meta = service_registry_entry_create_helper(keypair, &name, &ip, &pport);
                if (meta != NULL && (element[1] != 0)) {
                    *meta = skn_strip(++element);
                } else {
                    control = 0;
                    if (errors != NULL)
                        (*errors)++;
                    skn_logger(SD_WARNING, "Response format failure: for name=%s, ip=%s, port=%s, first failing entry: [%s]", name, ip, pport, keypair);
                    break;
                }
            }
        } // end while line

        if (control == 1) { // catch a breakout caused by no value
            service_registry_entry_create(psreg, name, ip, pport, errors);
        }

    } // end while buffer

    free(base);
    return psreg->count;
}

PServiceRegistry service_registry_get_via_udp_broadcast(int i_socket, char *request) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    IPBroadcastArray aB;
    int vIndex = 0;
    char response[SZ_INFO_BUFF];
    char recvHostName[SZ_INFO_BUFF];
    signed int rLen = 0;
    struct timeval start;

    memset(response, 0, sizeof(response));
    memset(recvHostName, 0, sizeof(recvHostName));

    get_broadcast_ip_array(&aB);
    strncpy(gd_ch_intfName, aB.chDefaultIntfName, SZ_CHAR_BUFF);
    strncpy(gd_ch_ipAddress, aB.ipAddrStr[aB.defaultIndex], SZ_CHAR_BUFF);

    skn_logger(SD_NOTICE, "Socket Bound to %s", aB.ipAddrStr[aB.defaultIndex]);

    gettimeofday(&start, NULL);
    for (vIndex = 0; vIndex < aB.count; vIndex++) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_addr.s_addr = inet_addr(aB.broadAddrStr[vIndex]);
        remaddr.sin_port = htons(SKN_FIND_RPI_PORT);

        if (sendto(i_socket, request, strlen(request), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_logger(SD_WARNING, "SendTo() Timed out; Failure code=%d, etext=%s", errno, strerror(errno));
            break;
        }
        skn_logger(SD_NOTICE, "Message Broadcasted on %s:%s:%d", aB.ifNameStr[vIndex], aB.broadAddrStr[vIndex], SKN_FIND_RPI_PORT);
    }

    PServiceRegistry psr = service_registry_create();
    skn_logger(SD_DEBUG, "Waiting 5 seconds for all responses\n");
    while (gi_exit_flag == SKN_RUN_MODE_RUN) { // depends on a socket timeout of 5 seconds

        rLen = recvfrom(i_socket, response, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen);
        if (rLen == PLATFORM_ERROR) {  // EAGAIN
            break;
        }
        response[rLen] = 0;

        rLen = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, (SZ_INFO_BUFF -1), NULL, 0, NI_DGRAM);
        if (rLen != 0) {
            skn_logger(SD_EMERG, "getnameinfo() failed: %s\n", gai_strerror(rLen));
            break;
        }

        skn_logger(SD_INFO, "Response(%1.6fs) received from %s @ %s:%d",
                        skn_duration_in_milliseconds(&start, NULL),
                        recvHostName,
                        inet_ntoa(remaddr.sin_addr),
                        ntohs(remaddr.sin_port)
                  );
        service_registry_response_parse(psr, response, NULL);
    }

    return (psr);
}

void service_registry_destroy(PServiceRegistry psreg) {
    int index = 0;

    if (psreg == NULL)
        return;

    while (index < psreg->count) {
        free(psreg->entry[index++]);
    }
    free(psreg);
}
