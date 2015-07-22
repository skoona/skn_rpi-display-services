/**
 * sknNetworkHelpers.c
*/

#include "skn_network_helpers.h"


/*
 * Global Exit Flag -- set by signal handler
 * atomic to survive signal handlers
*/
sig_atomic_t gi_exit_flag = SKN_RUN_MODE_RUN;  // will hold the signal which cause exit
char *gd_pch_message = NULL;

signed int gd_i_debug   = 0;
char gd_ch_program_name[SZ_LINE_BUFF];
char gd_ch_program_desc[SZ_LINE_BUFF];
char *gd_pch_effective_userid= NULL;
int gd_i_socket = -1;
char gd_ch_ipAddress[NI_MAXHOST];
char gd_ch_intfName[SZ_CHAR_BUFF];



static void skn_locator_print_usage(int argc, char *argv[]);
static void exit_handler(int sig);

static int skn_signal_manager_process_signals(siginfo_t *signal_info);
static void *skn_signal_manager_handler_thread(void *real_user_id);

static void * service_registry_entry_create_helper(char *key, char **name, char **ip, char **port);
static PServiceRegistry service_registry_create();
static int service_registry_entry_create(PServiceRegistry psreg, char *name, char *ip, char *port, int *errors);
static int service_registry_response_parse(PServiceRegistry psreg, const char *response, int *errors);

/**
 * Setup effective and real userid
 */
uid_t skn_get_userids() {
	uid_t real_user_id = 0;
	uid_t effective_user_id = 0;
	struct passwd *userinfo= NULL;

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
	skn_logger(SD_NOTICE, "Program Exiting, from signal=%d:%s\n",sig, strsignal(sig));
}

void signals_init() {
    signal(SIGINT,  exit_handler);  // Ctrl-C
    signal(SIGQUIT, exit_handler);  // Quit
    signal(SIGTERM, exit_handler);  // Normal kill command
}

void signals_cleanup(int sig) {
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

    if (gi_exit_flag > SKN_RUN_MODE_RUN) {  // exit caused by some interrupt -- otherwise it would be exactly 0
    	kill(getpid(), sig);
    }
}

/*
 * process_signals()
 *
 * Handle/Process linux signals for the whole multi-threaded application.
 *
 * Params:
 *    sig -- current linux signal
 *
 * Returns/Affects:
 *   returns current value of the atomic int gi_exit_flag
 *   returns true (or current value) if nothing needs done
 *   returns 0 or false if exit is required
 */
static int skn_signal_manager_process_signals(siginfo_t *signal_info)
{
  int rval = gi_exit_flag; /* use existing value */
  int sig = 0;
  char *pch = "<unknown>";

  assert (signal_info != NULL);

  sig = signal_info->si_signo;

  /*
   * look to see what signal has been caught
   */
  switch (sig)
  {
    case SIGHUP: /* often used to reload configuration */
//      rval = 33; /* flag a reload of the ip address info */
      skn_logger(SD_NOTICE, "%s received: Requesting IP Address Info reload => [pid=%d, uid=%d]",
      strsignal(sig), signal_info->si_pid,signal_info->si_uid);
      break;
    case SIGUSR1: /* Any user function */
      switch (signal_info->si_code)
      {
        case SI_USER:
          pch="kill(2) or raise(3)";
          break;
        case SI_KERNEL:
          pch="Sent by the kernel.";
          break;
        case SI_QUEUE:
          pch="sigqueue(2)";
          break;
        case SI_TIMER:
          pch="POSIX timer expired";
          break;
        case SI_MESGQ:
          pch="POSIX message queue state changed";
          break;
        case SI_ASYNCIO:
          pch="AIO completed";
          break;
        case SI_SIGIO:
          pch="queued SIGIO";
          break;
        case SI_TKILL:
          pch="tkill(2) or tgkill(2)";
          break;
        default:
          pch = "<unknown>";
          break;
      }
      skn_logger(SD_NOTICE, "%s received from => %s ?[pid=%d, uid=%d]{Ignored}",
      strsignal(sig), pch, signal_info->si_pid,signal_info->si_uid);
      break;
    case SIGCHLD: /* some child ended */
      switch (signal_info->si_code)
      {
        case CLD_EXITED:
          pch = "child has exited";
          break;
        case CLD_KILLED:
          pch = "child was killed";
          break;
        case CLD_DUMPED:
          pch = "child terminated abnormally";
          break;
        case CLD_TRAPPED:
          pch = "traced child has trapped";
          break;
        case CLD_STOPPED:
          pch = "child has stopped";
          break;
        case CLD_CONTINUED:
          pch = "stopped child has continued";
          break;
        default:
          pch = "<unknown>";
          break;
      }
      skn_logger(SD_NOTICE, "%s received for pid => %d, w/rc => %d for this reason => %s {Ignored}",
      strsignal(sig), signal_info->si_pid, signal_info->si_status, pch);
      break;
    case SIGQUIT: /* often used to signal an orderly shutdown */
    case SIGINT: /* often used to signal an orderly shutdown */
    case SIGPWR: /* Power Failure */
    case SIGKILL: /* Fatal Exit flag */
    case SIGTERM: /* Immediately Fatal Exit flag */
    default:
      switch (signal_info->si_code)
      {
        case SI_USER:
          pch="kill(2) or raise(3)";
          break;
        case SI_KERNEL:
          pch="Sent by the kernel.";
          break;
        case SI_QUEUE:
          pch="sigqueue(2)";
          break;
        case SI_TIMER:
          pch="POSIX timer expired";
          break;
        case SI_MESGQ:
          pch="POSIX message queue state changed";
          break;
        case SI_ASYNCIO:
          pch="AIO completed";
          break;
        case SI_SIGIO:
          pch="queued SIGIO";
          break;
        case SI_TKILL:
          pch="tkill(2) or tgkill(2)";
          break;
        default:
          pch = "<unknown>";
          break;
      }
      skn_logger(SD_NOTICE, "%s received from => %s ?[pid=%d, uid=%d]{Exiting}",
      strsignal(sig), pch, signal_info->si_pid,signal_info->si_uid);
      rval = sig;
      break;
  } /* end switch */

  return rval;
}


/**
 *  handler_thread()
 *
 *  Trap linux signals for the whole multi-threaded application.
 *
 *  Params:
 *    real_user_id  -- demos passing a ptr or value into thread context.
 *
 *  Returns/Affects:
 *      returns and/or set the atomic gint gi_exit_flag
 *      returns last signal
 */
static void *skn_signal_manager_handler_thread(void *real_user_id)
{
  sigset_t signal_set;
  siginfo_t signal_info;
  int sig = 0;
  int rval = 0;

  sigfillset(&signal_set);
  skn_logger(SD_NOTICE, "signal handler: startup successful");

  while (gi_exit_flag == SKN_RUN_MODE_RUN)
  {
    /* wait for any and all signals */
    /* OLD: sigwait (&signal_set, &sig); */
    sig = sigwaitinfo(&signal_set, &signal_info);
    if (!sig)
    {
      skn_logger(SD_WARNING,
      "signal handler: sigwaitinfo() returned an error => {%s}", strerror(errno));
      if (errno == EAGAIN) {
    	  continue;
      }
    }
    /* when we get this far, we've  caught a signal */
    rval = skn_signal_manager_process_signals( &signal_info);
    gi_exit_flag = rval;

  } /* end-while */

  pthread_sigmask(SIG_UNBLOCK, &signal_set, NULL);

  skn_logger(SD_NOTICE, "signal handler: shutdown complete");

  pthread_exit( (void *)(long int)sig );

  return NULL;
}

/**
 * Final step
 * - may send trapped signal to app, so requester knows it was honored
 */
int skn_signal_manager_shutdown(pthread_t sig_thread, sigset_t *psignal_set) {
	void *trc= NULL;
	int rc = EXIT_SUCCESS;

	if (gi_exit_flag <= SKN_RUN_MODE_STOP){
		gi_exit_flag = SKN_RUN_MODE_STOP; /* shut down the system -- work is done */
		// need to force theads down or interrupt them
		skn_logger(SD_WARNING, "shutdown caused by application!");
		pthread_cancel(sig_thread);
		sleep(1);
		skn_logger(SD_WARNING, "Collecting (cleanup) threads.");
		pthread_join(sig_thread, &trc);
	} else {
		rc = EXIT_FAILURE;
		skn_logger(SD_NOTICE, "Collecting signal thread's return code.");
		pthread_join(sig_thread, &trc);
		skn_logger(SD_NOTICE, "Signal thread was ended by a %d:%s signal.",
				gi_exit_flag, strsignal((int)(long int)trc));
	}
	pthread_sigmask(SIG_UNBLOCK, psignal_set, NULL);

	return rc;
}

/**
 * Initialize signal manager
 */
int skn_signal_manager_startup(pthread_t *psig_thread, sigset_t *psignal_set, uid_t *preal_user_id) {
	int i_thread_rc = 0; // EXIT_SUCCESS

	sigfillset(psignal_set);
	pthread_sigmask(SIG_BLOCK, psignal_set, NULL);

	i_thread_rc = pthread_create( psig_thread, NULL, skn_signal_manager_handler_thread, (void*) preal_user_id);
	if (i_thread_rc == -1) {
		skn_logger(SD_WARNING, "Create signal thread failed: %s", strerror(errno));
		i_thread_rc = EXIT_FAILURE;
	}
	sleep(1); // give thread a chance to start

	return i_thread_rc;
}

void get_default_interface_name_and_ipv4_address(char * intf, char * ipv4) {
	IPBroadcastArray aB;

	get_broadcast_ip_array(&aB);
	    memcpy(intf, aB.chDefaultIntfName, SZ_CHAR_BUFF);
	    memcpy(ipv4, aB.ipAddrStr[aB.defaultIndex], SZ_CHAR_BUFF);
}

/**
 * Collect IP and Broadcast Addresses for this machine
 *
 * - Affects the PIPBroadcastArray
 * - Return -1 on error, or count of interfaces
 * - contains this ipAddress in paB->ipAddrStr[paB->defaultIndex]
 */
int get_broadcast_ip_array(PIPBroadcastArray paB)
{
	struct ifaddrs * ifap;
    struct ifaddrs * p;
    int rc = 0;

    memset(paB, 0, sizeof(IPBroadcastArray));
    paB->count = 0;
    paB->defaultIndex = 0;
    strcpy(paB->cbName, "Net Broadcast IPs");

    rc = get_default_interface_name(paB->chDefaultIntfName);
    if(rc == EXIT_FAILURE) {
    	skn_logger(SD_ERR, "No Default Network Interfaces Found!. Count=%d", rc);
    	return -1;
    }
    rc = getifaddrs(&ifap);
	if (rc != 0) {
		skn_logger(SD_ERR, "No Network Interfaces Found at All ! %d:%d:%s", rc, errno, strerror(errno));
		return(-1);
	}
	p = ifap;

    while(p && paB->count < ARY_MAX_INTF) {
	  if (p->ifa_addr != NULL &&
			  p->ifa_addr->sa_family == AF_INET &&
			  	  ((p->ifa_flags & IFF_BROADCAST) > 0)) {

			inet_ntop(p->ifa_addr->sa_family,
				&((struct sockaddr_in *)p->ifa_addr)->sin_addr,
				paB->ipAddrStr[paB->count], (SZ_CHAR_BUFF -1)
			);
			inet_ntop(p->ifa_addr->sa_family,
				&((struct sockaddr_in *)p->ifa_netmask)->sin_addr,
				paB->maskAddrStr[paB->count], (SZ_CHAR_BUFF -1)
			);
			inet_ntop(p->ifa_addr->sa_family,
				&((struct sockaddr_in *)p->ifa_broadaddr)->sin_addr,
				paB->broadAddrStr[paB->count], (SZ_CHAR_BUFF -1)
			);

			strncpy(paB->ifNameStr[paB->count], p->ifa_name, (SZ_CHAR_BUFF -1) );

  	        if(strcmp(paB->chDefaultIntfName, p->ifa_name) == 0 ) {
  	        	paB->defaultIndex = paB->count;
  	        }
  	        paB->count ++;
      }
      p = p->ifa_next;
    } // end while
	freeifaddrs(ifap);

	return paB->count;
}

/**
 * Retrieves default internet interface name into param
 * return EXIT_SUCCESS or EXIT_FAILURE
 */
int get_default_interface_name(char *pchDefaultInterfaceName)
{
    FILE *f;
    char line[SZ_LINE_BUFF] , *dRoute = NULL, *iName = NULL;;

    f = fopen("/proc/net/route" , "r");
    if (f == NULL) {
    	skn_logger(SD_ERR, "Opening RouteInfo Failed: %d:%s", errno, strerror(errno));
    	return EXIT_FAILURE;
    }

    while(fgets(line , SZ_LINE_BUFF, f))
    {
    	iName = strtok(line, "\t");
    	dRoute = strtok(NULL, "\t");

        if(iName != NULL && dRoute != NULL)
        {
            if(strcmp(dRoute , "00000000") == 0)
            {
                strncpy(pchDefaultInterfaceName, iName, (SZ_CHAR_BUFF -1) );
                break;
            }
        }
    }
	fclose(f);

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
static void skn_locator_print_usage(int argc, char *argv[])
{
  if (argc >=1)
  {
	  skn_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
	  skn_logger(" ", "\tSkoona Development <skoona@gmail.com>");
	  skn_logger(" ", "Usage:\n  %s [-v] [-m 'text msg'][-d 1|88] [-h|--help]", gd_ch_program_name);
	  skn_logger(" ", "Options:");
	  skn_logger(" ", "  -d 1|88 --debug=1\tDebug mode=1, Debug & no demonize=88.");
	  skn_logger(" ", "  -m, --message\tRequest/Response message to send.");
	  skn_logger(" ", "  -v, --version\tVersion printout.");
	  skn_logger(" ", "  -h, --help\t\tShow this help screen.");
  }
}


/* *****************************************************
 *  Parse out the command line options from argc,argv
 *  results go to globals
 *  EXIT_FAILURE on any error, or version and help
 *  EXIT_SUCCESS on normal completion
 */
int skn_handle_locator_command_line(int argc, char **argv)
{
  int opt = 0;
  int longindex =0;
  struct option longopts[] = {
	  { "debug", 	1, NULL, 'd' }, /* required param if */
	  { "message", 	1, NULL, 'm' }, /* required param if */
	  { "version", 	0, NULL, 'v' }, /* set true if present */
	  { "help", 	0, NULL, 'h' }, /* set true if present */
	  { 0, 0, 0, 0 }
  };

  /*
   * Get commandline options
   *  longindex is the current index into longopts
   *  optind is current/last index into argv
   *  optarg is value attached(-d88) or next element(-d 88) of argv
   *  opterr flags a scanning error
   */
  while ( (opt = getopt_long(argc, argv, "d:m:vh", longopts, &longindex)) != -1)
  {
    switch (opt)
    {
      case 'd':
        if (optarg)
        {
          gd_i_debug = atoi(optarg);
        }
        else
        {
        	fprintf(stderr, "<5>%s: input param was invalid! %c[%d:%d:%d]\n",
        			gd_ch_program_name, (char)opt, longindex, optind, opterr);
          return (EXIT_FAILURE);
        }
        break;
      case 'm':
        if (optarg)
        {
        	gd_pch_message = strdup(optarg);
        }
        else
        {
        	fprintf(stderr, "<5>%s: input param was invalid! %c[%d:%d:%d]\n",
        			gd_ch_program_name, (char)opt, longindex, optind, opterr);
          return (EXIT_FAILURE);
        }
        break;
      case 'v':
    	  fprintf(stderr, "<5>\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n",
    			  gd_ch_program_name, PACKAGE_VERSION);
        return (EXIT_FAILURE);
        break;
      case '?':
    	  fprintf(stderr, "<5>%s: unknown input param! %c[%d:%d:%d]\n",
    			  gd_ch_program_name, (char)opt, longindex, optind, opterr);
    	  skn_locator_print_usage(argc, argv);
        return (EXIT_FAILURE);
        break;
      default: /* help and default */
    	  skn_locator_print_usage(argc, argv);
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
	while ((((worker = strsep(&parser, "|")) != NULL) ||
			((worker = strsep(&parser, "%")) != NULL) ||
			((worker = strsep(&parser, ";")) != NULL)) &&
			(strlen(worker) > 16)) {
		skn_logger(SD_NOTICE, "[%s]", worker);
	}
	free(base);
}

void skn_program_name_and_description_set(const char *name, const char *desc) {
	strncpy(gd_ch_program_name, name, sizeof(gd_ch_program_name) -1 );
	strncpy(gd_ch_program_desc, desc, sizeof(gd_ch_program_desc) -1 );
}

/**
 * Remove Trailing and Leading Blanks
 * - caution: pointers from argv are readonly and segfault on 'alpha[end] = 0'
 */
char * skn_strip(char * alpha) {
	if(alpha == NULL || strlen(alpha) < 1) return alpha;

	int len = strlen(alpha);
	int end = len - 1;
	int start = 0;

	// use isgraph() or !isspace() vs isalnum() to allow ['|' ';' '%']
	while(isgraph(alpha[end]) == 0 && end > 0) {  // remove trailing non-alphanumeric chars
		alpha[end--] = 0;
	}

	len = strlen(alpha);
	while((isalnum(alpha[start]) == 0) && start < len) { // find first non-alpha stricter
		start++;
	}
	if (start < len && start > 0) {  // move in place
		end = len - start;
		memmove(&alpha[0], &alpha[start], end );
		alpha[end] = 0;
	}

	return alpha;
}

/*
	To access the unnamed arguments, one must declare a variable of type va_list in 
	the variadic function. The macro va_start is then called with two arguments: the 
	first is the variable declared of the type va_list, the second is the name of 
	the last named parameter of the function. After this, each invocation of the 
	va_arg macro yields the next argument. The first argument to va_arg is the 
	va_list and the second is the type of the next argument passed to the 
	function. Finally, the va_end macro must be called on the va_list before the 
	function returns. (It is not required to read in all the arguments.)

	There is no mechanism defined for determining the number or types 
	of the unnamed arguments passed to the function. The function is 
	simply required to know or determine this somehow, the means of 
	which vary. Common conventions include:

	Use of a printf or scanf-like format string with embedded specifiers that indicate argument types.
		A sentinel value at the end of the variadic arguments.
		A count argument indicating the number of variadic arguments.
*/

/* print all args one at a time until a negative argument is seen;
   all args are assumed to be of int type */
int skn_logger(const char *level, const char *format, ...)
{
  va_list args;
  char buffer[SZ_COMM_BUFF];  
  const char *logLevel = SD_NOTICE;

  va_start(args, format); 
  	vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  if (level != NULL) {
	  logLevel = level;
  }
  return fprintf(stderr, "%s%s\n", logLevel, buffer);
}

int host_socket_init(int port, int rcvTimeout) {
    struct sockaddr_in addr;
    int fd, broadcastEnable=1;
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
	    skn_logger(SD_EMERG, "Create Socket error=%d, etext=%s", errno, strerror(errno) );
		return(EXIT_FAILURE);
	}
	if ((setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) < 0) {
	    skn_logger(SD_EMERG, "Set Socket Broadcast Option error=%d, etext=%s", errno, strerror(errno) );
		return(EXIT_FAILURE);
	}

	struct timeval tv;
	tv.tv_sec = rcvTimeout;
	tv.tv_usec = 0;
	if ((setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
	    skn_logger(SD_EMERG, "Set Socket RcvTimeout Option error=%d, etext=%s", errno, strerror(errno) );
		return(EXIT_FAILURE);
	}

	/* set up local address */
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port=htons(port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	    skn_logger(SD_EMERG, "Bind to local Socket error=%d, etext=%s", errno, strerror(errno) );
		return(EXIT_FAILURE);
	}

	return fd;
}

int service_registry_provider(int i_socket, char *response) {
    struct sockaddr_in remaddr;           /* remote address */
    socklen_t addrlen = sizeof(remaddr);  /* length of addresses */
    IPBroadcastArray aB;
    char request[SZ_COMM_BUFF];
    char recvHostName[NI_MAXHOST];
    signed int rLen = 0, rc = 0;
    int exit_code = EXIT_SUCCESS;

    memset(request, 0, sizeof(request));
    memset(recvHostName, 0, sizeof(recvHostName));

    rc = get_broadcast_ip_array(&aB);
    if(rc == -1) {
    	return EXIT_FAILURE;
    }

	if (strlen(response) < 16) {
		snprintf(response, SZ_COMM_BUFF -1 ,
				"name=rpi_locator_service,ip=%s,port=%d|"
				"name=lcd_display_service,ip=%s,port=%d|",
				aB.ipAddrStr[aB.defaultIndex], SKN_FIND_RPI_PORT,
				aB.ipAddrStr[aB.defaultIndex], SKN_RPI_DISPLAY_SERVICE_PORT );
	}
	log_response_message(response);

	skn_logger(SD_DEBUG, "Socket Bound to %s:%s", aB.chDefaultIntfName, aB.ipAddrStr[aB.defaultIndex]);

    while (gi_exit_flag == SKN_RUN_MODE_RUN)
    {
        memset(&remaddr,0,sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port=htons(SKN_FIND_RPI_PORT);
        remaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    	addrlen = sizeof(remaddr);

        if ((rLen = recvfrom(i_socket, request, sizeof(request) - 1, 0,(struct sockaddr *) &remaddr, &addrlen)) < 0)
        {
        	if (errno == EAGAIN) {
        		continue;
        	}
		    skn_logger(SD_ERR, "RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno) );
		    exit_code = EXIT_FAILURE;
            break;
        }
        request[rLen] = 0;

        if(strcmp("stop",request) == 0) {
		    skn_logger(SD_NOTICE, "Shutdown Requested! exit code=%d", gi_exit_flag );
			break;
		}

        rc = getnameinfo( ((struct sockaddr *)&remaddr), sizeof(struct sockaddr_in),
        				 recvHostName , NI_MAXHOST , NULL , 0 , NI_DGRAM);
        if (rc != 0)
        {
		    skn_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno) );
		    exit_code = EXIT_FAILURE;
            break;
        }
        skn_logger(SD_NOTICE, "Received request from %s @ %s:%d",
        				recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));
		skn_logger(SD_NOTICE, "Request data: [%s]\n" , request);

        if (sendto(i_socket, response, strlen(response), 0,(struct sockaddr *) &remaddr, addrlen) < 0)
        {
		    skn_logger(SD_EMERG, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno) );
		    exit_code = EXIT_FAILURE;
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

	psreg = (PServiceRegistry)malloc( sizeof(ServiceRegistry) );
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
		if (errors != NULL) (*errors)++;
		return EXIT_FAILURE;
	}

	if (psreg->count >= psreg->computedMax ) {
		skn_logger(SD_WARNING, "Capacity Error: Too many! New entry %d:%s exceeds maximum of %d allowed! ",
				psreg->count, name, psreg->computedMax);
		if (errors != NULL) (*errors)++;
		return EXIT_FAILURE;
	}

	/* update or create entry */
	prent = service_registry_find_entry(psreg, name);
	if(prent == NULL) {
		prent = (PRegistryEntry)malloc( sizeof(RegistryEntry) );
		if (prent != NULL ) {
			psreg->entry[psreg->count] = prent;
			psreg->count++;
		}
	}

	if (prent != NULL ) {
		memset(prent, 0, sizeof(RegistryEntry));
		strcpy(prent->cbName, "PRegistryEntry");
		strcpy(prent->name, name);
		strcpy(prent->ip, ip);
		prent->port = atoi(port);
	} else {
		skn_logger(SD_WARNING, "Internal Memory Error: Could not allocate memory for entry %d:%s !",
				name, psreg->count);
		if (errors != NULL) (*errors)++;
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

PRegistryEntry service_registry_find_entry(PServiceRegistry psreg, char *serviceName) {
	PRegistryEntry prent = NULL;
	int index = 0;

	if (psreg == NULL) return NULL;

	while(index < psreg->count) {
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
		skn_logger(SD_INFO, "RegistryEntry(%02d) name=%s, ip=%s, port=%d", index,
				psr->entry[index]->name, psr->entry[index]->ip, psr->entry[index]->port);
	}
	return index;
}

/**
 * compute the address of each field on demand, from struct
 */
void * service_registry_get_entry_ref(PRegistryEntry prent, char *field) {
	int  index = 0;
	void * result = NULL;
	char * names[4] = {
			"name",
			"ip",
			"port",
			NULL
				    };
	int offsets[4] = {
			offsetof(RegistryEntry, name),
			offsetof(RegistryEntry, ip),
			offsetof(RegistryEntry, port),
			0
					 };
	for(index = 0; names[index] != NULL; index++) {
		if (strcmp(names[index], field) == 0) {
			result = (void *)prent + offsets[index];
			break;
		}
	}

	return result;
}

/**
 * compute the address of each field on demand, from local vars
 */
static void * service_registry_entry_create_helper(char *key, char **name, char **ip, char **port) {
	int  index = 0;
	char * guess = NULL;
	void * result = NULL;
	char * names[4] = { "name", "ip", "port", NULL };
	void * offsets[] = { name, ip, port, NULL };

	skn_strip(key); // cleanup first
	for(index = 0; names[index] != NULL; index++) { // find direct match
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
	char *base = NULL,
		 *psep = NULL,
		 *resp = NULL,
		 *line = NULL,
		 *keypair = NULL,
		 *element = NULL,
		 *name = NULL,
		 *ip = NULL,
		 *pport = NULL,
		 **meta = NULL;

	base = resp = strdup(response);

	if (strstr(response, "|")) {
		psep = "|";
	} else if (strstr(response, "%")) {
		psep = "%";
	} else if (strstr(response, ";")) {
		psep = ";";
	}

	while((line = strsep(&resp, psep)) != NULL) {
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
				if(meta != NULL && ( element[1] != 0 )) {
					*meta = skn_strip( ++element );
				} else {
					control = 0;
					if (errors != NULL) (*errors)++;
					skn_logger(SD_WARNING,
							"Response format failure: for name=%s, ip=%s, port=%s, first failing entry: [%s]",
							name, ip, pport, keypair);
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
    struct sockaddr_in remaddr;           /* remote address */
    socklen_t addrlen = sizeof(remaddr);  /* length of addresses */
    IPBroadcastArray aB;
    int vIndex = 0;
    char response[SZ_COMM_BUFF];
    char recvHostName[NI_MAXHOST];
    signed int rLen = 0;

    memset(response, 0, sizeof(response));
    memset(recvHostName, 0, sizeof(recvHostName));

	get_broadcast_ip_array(&aB);
    skn_logger(SD_NOTICE, "Socket Bound to %s", aB.ipAddrStr[aB.defaultIndex]);

    for (vIndex = 0; vIndex < aB.count; vIndex++ ) {
		memset(&remaddr,0,sizeof(remaddr));
		remaddr.sin_family = AF_INET;
		remaddr.sin_addr.s_addr = inet_addr(aB.broadAddrStr[vIndex]);
		remaddr.sin_port=htons(SKN_FIND_RPI_PORT);

		if (sendto(i_socket, request, strlen(request), 0,(struct sockaddr *) &remaddr, addrlen) < 0)
		{
		    skn_logger(SD_WARNING, "SendTo() Timed out; Failure code=%d, etext=%s", errno, strerror(errno) );
			break;
		}
		skn_logger(SD_NOTICE, "Message Broadcasted on %s:%s:%d",
				aB.ifNameStr[vIndex], aB.broadAddrStr[vIndex], SKN_FIND_RPI_PORT);
    }

    if (strcmp("stop", request) == 0) {
    	return NULL;
    }

    PServiceRegistry psr = service_registry_create();
    skn_logger(SD_DEBUG, "Waiting 5 seconds for all responses\n");
    while (gi_exit_flag == SKN_RUN_MODE_RUN) {  // depends on a socket timeout of 15 seconds

    	rLen = recvfrom(i_socket, response, sizeof(response) -1, 0,(struct sockaddr *) &remaddr, &addrlen);
		if (rLen < 1)
		{
			break;
		}
		response[rLen] = 0;

		rLen = getnameinfo( ((struct sockaddr *)&remaddr), sizeof(struct sockaddr_in),
        				 recvHostName , NI_MAXHOST , NULL , 0 , NI_DGRAM);
        if (rLen != 0)
        {
			skn_logger(SD_EMERG, "getnameinfo() failed: %s\n", gai_strerror(rLen));
            break;
        }

        skn_logger(SD_INFO, "Response() received from %s @ %s:%d",
				recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));
        service_registry_response_parse(psr, response, NULL);
    }

    return(psr);
}

void service_registry_destroy(PServiceRegistry psreg) {
	int index = 0;

	if (psreg == NULL) return;

	while(index < psreg->count) {
		free( psreg->entry[index++] );
	}
	free(psreg);
}
