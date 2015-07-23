/**
 * skn_rpi_helpers.c
*/

#include "skn_network_helpers.h"
#include "skn_rpi_helpers.h"


/*
 *  Global lcd handle:
*/
int gd_i_rows = 4;
int gd_i_cols = 20;
PDisplayManager gp_structure_pdm = NULL;



static void skn_display_print_usage(int argc, char *argv[]);
static PDisplayManager skn_display_manager_create(char * welcome);
static void skn_display_manager_destroy(PDisplayManager pdm);
static void * skn_display_manager_message_consumer_thread(void * ptr);


/*
 * Utility Methods
*/
long skn_get_number_of_cpu_cores() {
	return sysconf(_SC_NPROCESSORS_ONLN);
}

/**
 * skn_scroller_pad_right
 * - fills remaining with spaces and 0 terminates
 * - buffer   message to adjust
 */
char * skn_scroller_pad_right(char *buffer) {
	int hIndex = 0;

	for(hIndex = strlen(buffer); hIndex < gd_i_cols; hIndex++) {
			buffer[hIndex] = ' ';
	}
	buffer[gd_i_cols-1] = 0;

	return buffer;
}

/**
 * skn_scroller_wrap_blanks
 *  - builds str with 20 chars in front, and 20 at right end
 */
char * skn_scroller_wrap_blanks(char *buffer) {
	char worker[SZ_INFO_BUFF];

	if (buffer == NULL || strlen(buffer) > SZ_INFO_BUFF) {
		return NULL;
	}

	snprintf(worker, (SZ_INFO_BUFF - 1), "%20s%s%20s", " ", buffer, " ");
	memmove(buffer, worker, SZ_INFO_BUFF-1);
	buffer[SZ_INFO_BUFF-1] = 0;

	return buffer;
}

/**
 * Return CPU Temp as millicentigrade
*/
long getCpuTemps(PCpuTemps temps) {
	long lRaw = 0;
	int rc = 0;

	FILE *sysFs = fopen ("/sys/class/thermal/thermal_zone0/temp","r");
	if(sysFs == NULL) {
		skn_logger(SD_WARNING, "Warning: Failed to OPEN CPU temperature: %d:%s\n", errno, strerror(errno));
		return -1;
	}

	rc = fscanf(sysFs, "%ld", &lRaw);
	fclose(sysFs);

	if (rc != 1 || lRaw < 0) {
		skn_logger(SD_WARNING, "Warning: Failed to READ CPU temperature: %d:%s\n", strerror(errno));
		return -1;
	}

	if (temps != NULL) { // populate struct
		snprintf(temps->c, sizeof(temps->c), "%3.1f%cC", (double)(lRaw/1000.0), 223);
		snprintf(temps->f, sizeof(temps->f), "%3.1f%cF", (double)(lRaw/1000.0*9/5+32), 223);
		temps->raw = lRaw;
		strncpy(temps->cbName, "CpuTemps", sizeof(temps->cbName) -1 );
	}

	return lRaw;
}

/*
 * Message Builders exclusively for Raspberry Pis
*/
int generate_rpi_model_info(char *msg)
{
  int model = 0, rev = 0, mem = 0, maker = 0, overVolted = 0, mLen = 0 ;
  char * message = "Device has an unknown model type.\n";

  piBoardId (&model, &rev, &mem, &maker, &overVolted) ;
  if (model == PI_MODEL_UNKNOWN) {
	  mLen = snprintf(msg, SZ_INFO_BUFF -1 , "%s", message) ;
  } else {
	  mLen = snprintf(msg, SZ_INFO_BUFF -1 ,
				"Device Type: %s, Cpus: %ld, Revision: %s, Memory: %dMB, Maker: %s %s, %s:%s",
				piModelNames[model],
				skn_get_number_of_cpu_cores(),
				piRevisionNames[rev],
				mem,
				piMakerNames[maker],
				overVolted ? "[OV]" : "",
				gd_ch_intfName,
				gd_ch_ipAddress
					);
  }
  return mLen;
}

int generate_uname_info(char *msg)
{
  struct utsname info;

  int mLen = 0;
  char * message = "uname() api failed.";

  if (uname(&info) != 0) {
	  mLen = snprintf(msg, SZ_INFO_BUFF -1 , "%s", message) ;
  } else {
	  mLen = snprintf(msg, SZ_INFO_BUFF -1 , "%s:%s %s, %s %s",
                    info.nodename,
					info.sysname,
					info.release,
					info.version,
					info.machine
                  );
  }
  return mLen;
}

int generate_datetime_info(char *msg)
{
	int mLen = 0;
	struct tm *t;
	time_t tim;

	tim = time(NULL);
	t = localtime(&tim);

	mLen = snprintf(msg, SZ_INFO_BUFF -1 ,
					"%02d:%02d:%04d %02d:%02d:%02d",
					t->tm_mon + 1, t->tm_mday, t->tm_year + 1900,
					((t->tm_hour - TZ_ADJUST) < 0 ? (t->tm_hour - TZ_ADJUST + 12) : (t->tm_hour - TZ_ADJUST)),
					t->tm_min, t->tm_sec);

	return mLen;
}

int generate_cpu_temps_info(char *msg)
{
  static CpuTemps cpuTemp;
  int mLen = 0;

  memset(&cpuTemp, 0, sizeof(CpuTemps));
  getCpuTemps(&cpuTemp);

  mLen = snprintf(msg, SZ_INFO_BUFF-1, "CPU: %s %s", cpuTemp.c, cpuTemp.f) ;

  return mLen;
}

/*
 * setBacklightState: */
void skn_lcd_backlight_set (int state)
{
  digitalWrite (AF_BACKLIGHT,  state) ;
}


/*
 * pcf8574LCDSetup:
 *	Setup the pcf8574 board by making sure the additional pins are
 *	set to the correct modes, etc.
 *********************************************************************************
*/
int skn_pcf8574LCDSetup (PDisplayManager pdm, int backLight)
{
/*
 * Initial I2C Services */
  wiringPiSetupSys () ;
  pcf8574Setup (AF_BASE, 0x27) ;

//	Backlight LEDs
  pinMode (AF_BACKLIGHT,  OUTPUT) ;
  skn_lcd_backlight_set (backLight) ;

// Control signals
  pinMode (AF_RW, OUTPUT) ; digitalWrite (AF_RW, LOW) ;	// Not used with wiringPi - always in write mode

// The other control pins are initialised with lcdInit ()
  pdm->lcd_handle = lcdInit (gd_i_rows, gd_i_cols, 4, AF_RS, AF_E, AF_DB4,AF_DB5,AF_DB6,AF_DB7, 0,0,0,0) ;

  if (pdm->lcd_handle < 0)
  {
    skn_logger(SD_ERR, "I2C Services failed to initialize. lcdInit(%d)", pdm->lcd_handle);
  } else {
	  lcdClear (pdm->lcd_handle) ;
  }

  return pdm->lcd_handle;
}

/**
 * Scrolls a single line across the lcd display
 * - best when wrapped in 20 chars
*/
int skn_scroller_scroll_lines (PDisplayLine pdl, int lcd_handle, int line) // int position, int line, const char *msg, int wait)
{
  char buf[40] ;
  signed int hAdjust = 0, mLen = 0, mfLen = 0;

  mLen = strlen( &(pdl->ch_display_msg[pdl->display_pos]) );
  if (gd_i_cols < mLen) {
	  mfLen = pdl->msg_len;
	  hAdjust = (mfLen - gd_i_cols);
  }

  snprintf(buf, sizeof(buf) -1 , "%s", &(pdl->ch_display_msg[pdl->display_pos])) ;
  skn_scroller_pad_right(buf);

  lcdPosition (lcd_handle, 0, line) ;
  lcdPuts (lcd_handle, buf) ;

  if (++pdl->display_pos > hAdjust) {
	  pdl->display_pos = 0 ;
  }

  return pdl->display_pos;
}


/**
 * skn_display_manager_select_set
 * - adds newest to top of double-linked-list
 * - only keeps 8 in collections
 * - reuses in top down fashion
 */
static PDisplayManager skn_display_manager_create(char * welcome) {
	int index = 0, next = 0, prev = 0;
	PDisplayManager pdm = NULL;
	PDisplayLine pdl = NULL;

	pdm = (PDisplayManager)malloc( sizeof(DisplayManager) );
	if (pdm == NULL) {
		skn_logger(SD_ERR, "Display Manager cannot acquire needed resources. %d:%s", errno, strerror(errno));
		return NULL;
	}

	memset(pdm, 0, sizeof(DisplayManager));
	strcpy(pdm->cbName, "PDisplayManager");
	memmove(pdm->ch_welcome_msg, welcome, SZ_INFO_BUFF-1);
	pdm->msg_len = strlen(welcome);

	pdm->dsp_cols = gd_i_cols;
	pdm->dsp_rows = gd_i_rows;
	pdm->current_line = 0;
	pdm->next_line = gd_i_rows;

	for (index = 0; index < ARY_MAX_INTF; index++) {
		pdl = pdm->pdsp_collection[index] = (PDisplayLine)malloc(sizeof(DisplayLine)); // line x
		memset(pdl, 0, sizeof(DisplayLine));
		strcpy(pdl->cbName, "PDisplayLine");
		memmove(pdl->ch_display_msg, welcome, (SZ_INFO_BUFF -1));  // load all with welcome message
		pdl->ch_display_msg[(SZ_INFO_BUFF-1)] = 0;    // terminate string in case
		pdl->msg_len = pdm->msg_len; // strlen(welcome);
		pdl->active = 1;
		if (pdl->msg_len > gd_i_cols) {
			pdl->scroll_enabled = 1;
		}
	}
	for (index = 0; index < ARY_MAX_INTF; index++) {               // enable link list routing
		next = (((index + 1) == ARY_MAX_INTF) ?  0 : (index + 1));
		prev = (((index - 1) == -1) ?  (ARY_MAX_INTF-1) : (index - 1));
		pdm->pdsp_collection[index]->next = pdm->pdsp_collection[next];
		pdm->pdsp_collection[index]->prev = pdm->pdsp_collection[prev];
	}

	return pdm;
}
PDisplayLine skn_display_manager_add_line(PDisplayManager pdmx, char * client_request_message) {
	PDisplayLine pdl = NULL;
	PDisplayManager pdm = NULL;

	pdm = ((pdmx == NULL) ? skn_get_display_manager_ref() : pdmx);
	if (pdm == NULL || client_request_message == NULL) {
		return NULL;
	}

	/*
	 * manage next index */
	pdl = pdm->pdsp_collection[pdm->next_line++]; // manage next index
	if (pdm->next_line == ARY_MAX_INTF) {
		pdm->next_line = 0; // roll it
	}

	/*
	 * load new message */
	memmove(pdl->ch_display_msg, client_request_message, (SZ_INFO_BUFF-1));
	pdl->ch_display_msg[SZ_INFO_BUFF-1] = 0;    // terminate string in case
	pdl->msg_len = strlen(pdl->ch_display_msg);
	pdl->active = 1;
	pdl->display_pos = 0;
	if (pdl->msg_len > gd_i_cols) {
		pdl->scroll_enabled = 1;
		skn_scroller_wrap_blanks(pdl->ch_display_msg);
		pdl->ch_display_msg[(SZ_INFO_BUFF-1)] = 0;    // terminate string in case
	} else {
		pdl->scroll_enabled = 0;
	}

	/*
	 * manage current_line */
	pdm->pdsp_collection[pdm->current_line]->active = 0;
	pdm->pdsp_collection[pdm->current_line++]->msg_len = 0;
	if (pdm->current_line == ARY_MAX_INTF) {
		pdm->current_line = 0;
	}

	skn_logger(SD_DEBUG, "DM Added msg=%d:%d:[%s]", pdm->next_line -1, pdl->msg_len, pdl->ch_display_msg);


	/* return this line's pointer */
	return pdl;
}
PDisplayManager skn_get_display_manager_ref() {
	return gp_structure_pdm;
}
int skn_display_manager_do_work(char * client_request_message) {
	int index = 0, dsp_line_number = 0;
	PDisplayManager pdm = NULL;
	PDisplayLine pdl = NULL;
    char ch_lcd_message[3][SZ_INFO_BUFF];

	gp_structure_pdm = pdm = skn_display_manager_create(client_request_message);
	if (pdm == NULL) {
		gi_exit_flag = SKN_RUN_MODE_STOP;
		skn_logger(SD_ERR, "Display Manager cannot acquire needed resources. DMCreate()");
		return gi_exit_flag;
	}
	generate_cpu_temps_info(ch_lcd_message[0]);
	generate_datetime_info (ch_lcd_message[1]);
	generate_rpi_model_info(ch_lcd_message[2]);
	skn_display_manager_add_line(pdm, ch_lcd_message[0]);
	skn_display_manager_add_line(pdm, ch_lcd_message[1]);
	skn_display_manager_add_line(pdm, ch_lcd_message[2]);

	if (skn_pcf8574LCDSetup(pdm, HIGH) == -1) {
		gi_exit_flag = SKN_RUN_MODE_STOP;
		skn_logger(SD_ERR, "Display Manager cannot acquire needed resources: lcdSetup().");
		skn_display_manager_destroy(pdm);
		return gi_exit_flag;
	}

	if (skn_display_manager_message_consumer_startup(pdm) == EXIT_FAILURE) {
		gi_exit_flag = SKN_RUN_MODE_STOP;
		skn_logger(SD_ERR, "Display Manager cannot acquire needed resources: Consumer().");
		skn_display_manager_destroy(pdm);
		return gi_exit_flag;
	}


	/*
	 *  Do the Work
	 */
	while(gi_exit_flag == SKN_RUN_MODE_RUN) {
		dsp_line_number = 0;
        pdl = pdm->pdsp_collection[pdm->current_line];
		for (index = 0; index < pdm->dsp_rows; index++) {
		  if (pdl->active) {
			  skn_scroller_scroll_lines(pdl, pdm->lcd_handle, dsp_line_number++);
		      delay(250);
		  }
		  pdl = (PDisplayLine)pdl->next;
		}
	}

	lcdClear (pdm->lcd_handle) ;
    skn_lcd_backlight_set (LOW) ;

    /*
     * Stop UDP Listener
     */
    skn_display_manager_message_consumer_shutdown(pdm);

	skn_display_manager_destroy(pdm);
	gp_structure_pdm = pdm = NULL;

	return gi_exit_flag;
}
static void skn_display_manager_destroy(PDisplayManager pdm) {
	int index = 0;

	// free collection
	for (index = 0; index < ARY_MAX_INTF; index++) {
		if (pdm->pdsp_collection[index] != NULL) {
			free(pdm->pdsp_collection[index]);
		}
	}
	// free manager
	if (pdm != NULL)
		free(pdm);
}


/**
 * skn_display_manager_message_consumer(PDisplayManager pdm)
 * - returns Socket or EXIT_FAILURE
 */
int skn_display_manager_message_consumer_startup(PDisplayManager pdm) {
    /*
	 * Start UDP Listener */
    pdm->i_socket = host_socket_init(SKN_RPI_DISPLAY_SERVICE_PORT, 15);
	if (pdm->i_socket == EXIT_FAILURE) {
		skn_logger(SD_EMERG, "DisplayManager: Host Init Failed!");
    	return EXIT_FAILURE;
	}

	/*
	 * Create Thread  */
	int i_thread_rc = pthread_create( &pdm->dm_thread, NULL, skn_display_manager_message_consumer_thread, (void *)pdm);
	if (i_thread_rc == -1) {
		skn_logger(SD_WARNING, "DisplayManager: Create thread failed: %s", strerror(errno));
		close(pdm->i_socket);
		return EXIT_FAILURE;
	}
	sleep(1); // give thread a chance to start

    skn_logger(SD_NOTICE, "DisplayManager: Thread startup successful");

	return pdm->i_socket;
}
/**
 * skn_display_manager_message_consumer(PDisplayManager pdm) */
void skn_display_manager_message_consumer_shutdown(PDisplayManager pdm) {
	void *trc= NULL;

	skn_logger(SD_WARNING, "DisplayManager: Canceling thread.");
	pthread_cancel(pdm->dm_thread);
	sleep(1);
	pthread_join(pdm->dm_thread, &trc);
	close(pdm->i_socket);
	skn_logger(SD_NOTICE, "DisplayManager: Thread ended:(%ld)", (long int)trc);
}

/**
 * skn_display_manager_message_consumer_thread(PDisplayManager pdm)
 */

static void * skn_display_manager_message_consumer_thread(void * ptr) {
	PDisplayManager pdm = (PDisplayManager)ptr;
    struct sockaddr_in remaddr;           /* remote address */
    socklen_t addrlen = sizeof(remaddr);  /* length of addresses */
    IPBroadcastArray aB;
    char request[SZ_INFO_BUFF];
    char recvHostName[NI_MAXHOST];
    signed int rLen = 0, rc = 0;
    long int exit_code = EXIT_SUCCESS;

    memset(request, 0, sizeof(request));
    memset(recvHostName, 0, sizeof(recvHostName));

    rc = get_broadcast_ip_array(&aB);
    if(rc == -1) {
    	exit_code = rc;
    	pthread_exit( (void *)exit_code );
    }

    while (gi_exit_flag == SKN_RUN_MODE_RUN)
    {
        memset(&remaddr,0,sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port=htons(SKN_RPI_DISPLAY_SERVICE_PORT);
        remaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    	addrlen = sizeof(remaddr);

        if ((rLen = recvfrom(pdm->i_socket, request, sizeof(request) - 1, 0,(struct sockaddr *) &remaddr, &addrlen)) < 0)
        {
        	if (errno == EAGAIN) {
        		continue;
        	}
		    skn_logger(SD_ERR, "DisplayManager: RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno) );
		    exit_code = errno;
            break;
        }
        request[rLen] = 0;

        rc = getnameinfo( ((struct sockaddr *)&remaddr), sizeof(struct sockaddr_in),
        				 recvHostName , NI_MAXHOST , NULL , 0 , NI_DGRAM);
        if (rc != 0)
        {
		    skn_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno) );
            break;
        }
        skn_logger(SD_NOTICE, "Received request from %s @ %s:%d",
        				recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));
		skn_logger(SD_NOTICE, "Request data: [%s]\n" , request);

		skn_display_manager_add_line(NULL, request);

        if (sendto(pdm->i_socket, "200 Accepted", strlen("200 Accepted"), 0,(struct sockaddr *) &remaddr, addrlen) < 0)
        {
		    skn_logger(SD_EMERG, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno) );
            break;
        }

    }

	skn_logger(SD_NOTICE, "Display Managager Thread: shutdown complete: (%ld)", exit_code);

	pthread_exit( (void *)exit_code );

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
static void skn_display_print_usage(int argc, char *argv[])
{
  if (argc >=1)
  {
	  skn_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
	  skn_logger(" ", "\tSkoona Development <skoona@gmail.com>");

	  if (strcmp(gd_ch_program_name, "lcd_display_client") != 0) {
		  skn_logger(" ", "Usage:\n  %s [-v] [-m 'text msg'] [-r 4|2] [-c 20|16] [-d 1|88] [-h|--help]", gd_ch_program_name);
		  skn_logger(" ", "Options:");
		  skn_logger(" ", "  -d 1|88 --debug=1\tDebug mode=1.");
		  skn_logger(" ", "  -r, --rows\t\tNumber of rows in physical display.");
		  skn_logger(" ", "  -c, --cols\t\tNumber of columns in physical display.");
	  } else {
		  skn_logger(" ", "Usage:\n  %s [-v] [-m 'message for display'][-d 1|88] [-h|--help]", gd_ch_program_name);
		  skn_logger(" ", "Options:");
		  skn_logger(" ", "  -d 1|88 --debug=1\tDebug mode=1.");
	  }
	  skn_logger(" ", "  -m, --message\t Message for line 1.");
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
int skn_handle_display_command_line(int argc, char **argv)
{
  int opt = 0;
  int longindex =0;
  struct option longopts[] = {
	  { "debug", 	1, NULL, 'd' }, /* required param if */
	  { "rows", 	1, NULL, 'r' }, /* required param if */
	  { "cols", 	1, NULL, 'c' }, /* required param if */
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
  while ( (opt = getopt_long(argc, argv, "d:m:r:c:vh", longopts, &longindex)) != -1)
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
        	skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n",
        			gd_ch_program_name, (char)opt, longindex, optind, opterr);
          return (EXIT_FAILURE);
        }
        break;
      case 'r':
        if (optarg)
        {
          gd_i_rows = atoi(optarg);
          switch (gd_i_rows) {
          case 2:
          case 4:
        	  break;
          default:
        	  gd_i_rows = 4;
           	  skn_logger(SD_WARNING, "%s: input param was invalid! (default of 4 used) %c[%d:%d:%d]\n",
          			  gd_ch_program_name, (char)opt, longindex, optind, opterr);

          }
        }
        else
        {
        	skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n",
        			gd_ch_program_name, (char)opt, longindex, optind, opterr);
          return (EXIT_FAILURE);
        }
        break;
      case 'c':
        if (optarg)
        {
          gd_i_cols = atoi(optarg);
          switch (gd_i_cols) {
          case 16:
          case 20:
        	  break;
          default:
        	  gd_i_cols = 20;
           	  skn_logger(SD_WARNING, "%s: input param was invalid! (default of 20 used) %c[%d:%d:%d]\n",
          			  gd_ch_program_name, (char)opt, longindex, optind, opterr);

          }
        }
        else
        {
        	skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n",
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
        	skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n",
        			gd_ch_program_name, (char)opt, longindex, optind, opterr);
          return (EXIT_FAILURE);
        }
        break;
      case 'v':
    	  skn_logger(SD_ERR, "\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n",
    			  gd_ch_program_name, PACKAGE_VERSION);
        return (EXIT_FAILURE);
        break;
      case '?':
    	  skn_logger(SD_ERR, "%s: unknown input param! %c[%d:%d:%d]\n",
    			  gd_ch_program_name, (char)opt, longindex, optind, opterr);
    	  skn_display_print_usage(argc, argv);
        return (EXIT_FAILURE);
        break;
      default: /* help and default */
    	  skn_display_print_usage(argc, argv);
        return (EXIT_FAILURE);
        break;
    }
  }

  return EXIT_SUCCESS;
}
