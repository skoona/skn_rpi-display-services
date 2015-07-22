/**
 * skn_rpi_helpers.c
*/

#include "skn_network_helpers.h"
#include "skn_rpi_helpers.h"


/*
 *  Global lcd handle:
*/
int gd_i_lcd_handle = 0;
int gd_i_rows = 4;
int gd_i_cols = 20;



static void skn_display_print_usage(int argc, char *argv[]);


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
	buffer[gd_i_cols] = 0;

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
	strcpy(buffer, worker);

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

  mLen = snprintf(msg, SZ_INFO_BUFF, "CPU: %s %s", cpuTemp.c, cpuTemp.f) ;

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
int skn_pcf8574LCDSetup (int backLight)
{
	int iwork = 0;

//	Backlight LEDs
  pinMode (AF_BACKLIGHT,  OUTPUT) ;
  skn_lcd_backlight_set (backLight) ;

// Control signals
  pinMode (AF_RW, OUTPUT) ; digitalWrite (AF_RW, LOW) ;	// Not used with wiringPi - always in write mode

// The other control pins are initialised with lcdInit ()
  iwork = gd_i_lcd_handle = lcdInit (gd_i_rows, gd_i_cols, 4, AF_RS, AF_E, AF_DB4,AF_DB5,AF_DB6,AF_DB7, 0,0,0,0) ;

  if (gd_i_lcd_handle < 0)
  {
    fprintf (stderr, "lcdInit failed\n") ;
    iwork = EXIT_FAILURE;
  }

  return iwork;
}

/**
 * Scrolls a single line across the lcd display
 * - best when wrapped in 20 chars
*/
int skn_scroller_scroll_line (int position, int line, int columns, const char *msg, int wait)
{
  char buf[40] ;
  signed int hAdjust = 0, mLen = 0, mfLen = 0;

  if (columns >= sizeof(buf)) {
	  columns = gd_i_cols;
  }

  mLen = strlen(&msg[position]);
  if (columns < mLen) {
	  mfLen = strlen(msg);
	  hAdjust = (mfLen - columns);
  }

  if (wait == SCROLL_WAIT) {
    delay(250);
  }

  snprintf(buf, sizeof(buf) -1 , "%s", &msg[position]) ;
  skn_scroller_pad_right(buf);

  lcdPosition (gd_i_lcd_handle, 0, line) ;
  lcdPuts (gd_i_lcd_handle, buf) ;

  if (++position > hAdjust) {
    position = 0 ;
  }

  return position;
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
	  skn_logger(" ", "Usage:\n  %s [-v] [-m 'text msg'][-d 1|88] [-h|--help]", gd_ch_program_name);
	  skn_logger(" ", "Options:");
	  skn_logger(" ", "  -d 1|88 --debug=1\tDebug mode=1.");
	  skn_logger(" ", "  -r, --rows\t\tNumber of rows in physical display.");
	  skn_logger(" ", "  -c, --cols\t\tNumber of columns in physical display.");
	  skn_logger(" ", "  -m, --message\tWelcome Message for line 1.");
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
