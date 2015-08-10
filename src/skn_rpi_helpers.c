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
int gd_i_i2c_address = 0x27;
char *gd_pch_serial_port;
char *gd_pch_device_name = "pcf";
PDisplayManager gp_structure_pdm = NULL;

static void skn_display_print_usage();
static PDisplayManager skn_display_manager_create(char * welcome);
static void skn_display_manager_destroy(PDisplayManager pdm);
static void * skn_display_manager_message_consumer_thread(void * ptr);
static PLCDDevice skn_device_manager_init_i2c(PDisplayManager pdm);

/*
 * Device Methods
*/
PLCDDevice skn_device_manager_SerialPort(PDisplayManager pdm) {
    PLCDDevice plcd =  NULL;

    if (pdm == NULL) {
        skn_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#SerialPort", SZ_CHAR_BUFF-1);
    if (gd_pch_serial_port != NULL) {
        strncpy(plcd->ch_serial_port_name, gd_pch_serial_port, SZ_CHAR_BUFF-1);
    } else {
        strncpy(plcd->ch_serial_port_name, "/dev/ttyACM0", SZ_CHAR_BUFF-1);
    }

    skn_logger(SD_NOTICE, "DeviceManager using  device [%s](%s)", plcd->cbName, gd_pch_serial_port);

    pdm->lcd_handle = plcd->lcd_handle =  serialOpen (plcd->ch_serial_port_name, 9600);
    if (plcd->lcd_handle == PLATFORM_ERROR) {
        skn_logger(SD_ERR, "DeviceManager failed to acquire needed resources: SerialPort=%s %d:%s",
                   plcd->ch_serial_port_name, errno, strerror(errno));
        return NULL;
    }

    // set backlight & clear screen
    char display_on[] = { 0xfe, 0x42 };
    char  cls[]   = { 0xfe, 0x58 };
    char home[]  = { 0xfe, 0x48 };
//    char set_cols_rows[] = {0xfe, 0xd1, gd_i_cols, gd_i_rows };
    char set_contrast[] = {0xfe, 0x50, 0xdc};
    char cursor_off[] = {0xfe, 0x4B };

    write(pdm->lcd_handle, set_contrast, sizeof(set_contrast));
        sleep(1);
    write(pdm->lcd_handle, home, sizeof(home));
        sleep(1);
    write(pdm->lcd_handle, cursor_off, sizeof(cursor_off));
        sleep(1);
    write(pdm->lcd_handle, cls, sizeof(cls));
        sleep(1);
    write(pdm->lcd_handle, display_on, sizeof(display_on));
        sleep(1);

    return plcd;
}
PLCDDevice skn_device_manager_MCP23017(PDisplayManager pdm) {
    PLCDDevice plcd =  NULL;
    int base = 0;

    if (pdm == NULL) {
        skn_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#MCP23017", SZ_CHAR_BUFF-1);
    if (gd_i_i2c_address != 0) {
        plcd->i2c_address = gd_i_i2c_address;
    } else {
        plcd->i2c_address = 0x20;
    }

    skn_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

    base = plcd->af_base = 100;
    plcd->af_backlight = base + 8;
    plcd->af_red = base + 6;
    plcd->af_green = base + 7;
    plcd->af_blue = base + 8;

    plcd->af_e  = base + 13;
    plcd->af_rs = base + 15;
    plcd->af_rw = base + 14;

    plcd->af_db4 = base + 12;
    plcd->af_db5 = base + 11;
    plcd->af_db6 = base + 10;
    plcd->af_db7 = base + 9;

    plcd->setup = &mcp23017Setup; //   mcp23017Setup(AF_BASE, 0x20);

    return skn_device_manager_init_i2c(pdm);
}
PLCDDevice skn_device_manager_MCP23008(PDisplayManager pdm) {
    PLCDDevice plcd =  NULL;
    int base = 0;

    if (pdm == NULL) {
        skn_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#MCP23008", SZ_CHAR_BUFF-1);
    if (gd_i_i2c_address != 0) {
        plcd->i2c_address = gd_i_i2c_address;
    } else {
        plcd->i2c_address = 0x20;
    }

    skn_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

    base = plcd->af_base = 100;
    plcd->af_backlight = base + 7;
    plcd->af_e = base + 2;
    plcd->af_rs = base + 1;
    plcd->af_rw = base + 0;

    plcd->af_db4 = base + 3;
    plcd->af_db5 = base + 4;
    plcd->af_db6 = base + 5;
    plcd->af_db7 = base + 6;

    plcd->setup = &mcp23008Setup; //   mcp23008Setup(AF_BASE, 0x20);

    return skn_device_manager_init_i2c(pdm);
}

PLCDDevice skn_device_manager_PCF8574(PDisplayManager pdm) {
    PLCDDevice plcd =  NULL;
    int base = 0;

    if (pdm == NULL) {
        skn_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    memset(plcd, 0, sizeof(LCDDevice));
    strncpy(plcd->cbName, "LCDDevice#PCF8574", SZ_CHAR_BUFF-1);
    if (gd_i_i2c_address != 0) {
        plcd->i2c_address = gd_i_i2c_address;
    } else {
        plcd->i2c_address = 0x27;
    }

    skn_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

    base = plcd->af_base = 100;
    plcd->af_backlight = base + 3;
    plcd->af_e = base + 2;
    plcd->af_rs = base + 0;
    plcd->af_rw = base + 1;

    plcd->af_db4 = base + 4;
    plcd->af_db5 = base + 5;
    plcd->af_db6 = base + 6;
    plcd->af_db7 = base + 7;


    plcd->setup = &pcf8574Setup; // pcf8574Setup(AF_BASE, 0x27);

    return skn_device_manager_init_i2c(pdm);
}

/*
 * setBacklightState: */
void skn_device_manager_backlight(int af_backlight, int state) {
    digitalWrite(af_backlight, state);
}

static PLCDDevice skn_device_manager_init_i2c(PDisplayManager pdm) {
    PLCDDevice plcd = (PLCDDevice)&pdm->lcd;

    // call the initializer
    plcd->setup(plcd->af_base, plcd->i2c_address);

// Control signals
    pinMode(plcd->af_rw, OUTPUT);
    digitalWrite(plcd->af_rw, LOW); // Not used with wiringPi - always in write mode

    //  Backlight LEDs
    if (strcmp(gd_pch_device_name, "mc7") == 0) {
        pinMode(plcd->af_red, OUTPUT);
        skn_device_manager_backlight(plcd->af_red, HIGH);
        pinMode(plcd->af_green, OUTPUT);
        skn_device_manager_backlight(plcd->af_green, HIGH);
        pinMode(plcd->af_blue, OUTPUT);
        skn_device_manager_backlight(plcd->af_blue, HIGH);
    } else {
        pinMode(plcd->af_backlight, OUTPUT);
        skn_device_manager_backlight(plcd->af_backlight, HIGH);
    }



    // The other control pins are initialised with lcdInit ()
    plcd->lcd_handle = pdm->lcd_handle = lcdInit(pdm->dsp_rows, pdm->dsp_cols, 4,
                                                 plcd->af_rs, plcd->af_e,
                                                 plcd->af_db4, plcd->af_db5, plcd->af_db6, plcd->af_db7,
                                                 plcd->af_db0, plcd->af_db1, plcd->af_db2, plcd->af_db3);

    if (pdm->lcd_handle == PLATFORM_ERROR) {
        skn_logger(SD_ERR, "I2C Services failed to initialize. lcdInit(%d)", pdm->lcd_handle);
        return NULL;
    } else {
        lcdClear(pdm->lcd_handle);
    }

    return plcd;
}
/*
 * LCDSetup:
 *  Setup the pcf8574 or mcp23008 lcd by making sure the additional pins are
 *  set to the correct modes, etc.
 *********************************************************************************
 */
int skn_device_manager_LCD_setup(PDisplayManager pdm, char *device_name) {
    PLCDDevice rc = NULL;

    /*
     * Initial I2C Services */
    wiringPiSetupSys();

    if (strcmp(device_name, "mcp") == 0) {
        rc = skn_device_manager_MCP23008(pdm);
    } else if (strcmp(device_name, "mc7") == 0) {
        rc = skn_device_manager_MCP23017(pdm);
    } else if (strcmp(device_name, "ser") == 0) {
        rc = skn_device_manager_SerialPort(pdm);
    } else { // PCF8574
        rc = skn_device_manager_PCF8574(pdm);
    }

    if (rc == NULL) {
        return PLATFORM_ERROR;
    } else {
        return pdm->lcd_handle;
    }
}
int skn_device_manager_LCD_shutdown(PDisplayManager pdm) {
    if (strcmp("ser", gd_pch_device_name) == 0) {
        char display_off[] = { 0xfe, 0x46 };
        char cls[]   = { 0xfe, 0x58 };

        write(pdm->lcd_handle, display_off, sizeof(display_off));
            sleep(1);
        write(pdm->lcd_handle, cls, sizeof(cls));

        serialClose(pdm->lcd_handle);
    } else {
        lcdClear(pdm->lcd_handle);
        if (strcmp(gd_pch_device_name, "mc7") == 0) {
            skn_device_manager_backlight(pdm->lcd.af_red, LOW);
            skn_device_manager_backlight(pdm->lcd.af_green, LOW);
            skn_device_manager_backlight(pdm->lcd.af_blue, LOW);
        } else {
            skn_device_manager_backlight(pdm->lcd.af_backlight, LOW);
        }
    }
    return EXIT_SUCCESS;
}

/*
 * Utility Methods
*/

/**
 * DO NOT USE THIS IN MODULES THAT HANDLE A I2C Based LCD
 * RPi cannot handle I2C and GetCpuTemp() without locking the process
 * in an uniterrupted sleep; forcing a power cycle.
*/
long getCpuTemps(PCpuTemps temps) {
    long lRaw = 0;
    int rc = 0;

    FILE *sysFs = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (sysFs == NULL) {
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
        snprintf(temps->c, sizeof(temps->c), "%3.1f%cC", (double )(lRaw / 1000.0), 223);
        snprintf(temps->f, sizeof(temps->f), "%3.1f%cF", (double )(lRaw / 1000.0 * 9 / 5 + 32), 223);
        temps->raw = lRaw;
        strncpy(temps->cbName, "CpuTemps", sizeof(temps->cbName) - 1);
    }

    return lRaw;
}

/*
 * Message Builders exclusively for Raspberry Pis
 */
int generate_rpi_model_info(char *msg) {
    int model = 0, rev = 0, mem = 0, maker = 0, overVolted = 0, mLen = 0;
    char * message = "Device has an unknown model type.\n";

    piBoardId(&model, &rev, &mem, &maker, &overVolted);
    if (model == PI_MODEL_UNKNOWN) {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s", message);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "Device: %s, Cpus: %ld, Rev: %s, Mem: %dMB, Maker: %s %s, %s:%s", piModelNames[model],
                        skn_get_number_of_cpu_cores(), piRevisionNames[rev], mem, piMakerNames[maker], overVolted ? "[OV]" : "", gd_ch_intfName,
                        gd_ch_ipAddress);
    }
    return mLen;
}

/**
 * DO NOT USE THIS IN MODULES THAT HANDLE A I2C Based LCD
 * RPi cannot handle I2C and GetCpuTemp() without locking the process
 * in an uniterrupted sleep; forcing a power cycle.
*/
int generate_cpu_temps_info(char *msg) {
    static CpuTemps cpuTemp;
    int mLen = 0;

    memset(&cpuTemp, 0, sizeof(CpuTemps));
    getCpuTemps(&cpuTemp);

    mLen = snprintf(msg, SZ_INFO_BUFF-1, "CPU: %s %s", cpuTemp.c, cpuTemp.f);

    return mLen;
}

/**
 * skn_scroller_pad_right
 * - fills remaining with spaces and 0 terminates
 * - buffer   message to adjust
 */
char * skn_scroller_pad_right(char *buffer) {
    int hIndex = 0;

    for (hIndex = strlen(buffer); hIndex < gd_i_cols; hIndex++) {
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
    char col_width_padding[16];

    if (buffer == NULL || strlen(buffer) > SZ_INFO_BUFF) {
        return NULL;
    }

    snprintf(col_width_padding, 16, "%%%ds%%s%%%ds", gd_i_cols, gd_i_cols);
    snprintf(worker, (SZ_INFO_BUFF - 1), col_width_padding, " ", buffer, " ");
    memmove(buffer, worker, SZ_INFO_BUFF-1);
    buffer[SZ_INFO_BUFF - 1] = 0;

    return buffer;
}

/**
 * Scrolls a single line across the lcd display
 * - best when wrapped in column chars on each side
 */
int skn_scroller_scroll_lines(PDisplayLine pdl, int lcd_handle, int line)
{
    char buf[40];
    signed int hAdjust = 0, mLen = 0, mfLen = 0;
    char set_col_row_position[] = {0xfe, 0x47, 0x01, 0x01};

    mLen = strlen(&(pdl->ch_display_msg[pdl->display_pos]));
    if (gd_i_cols < mLen) {
        mfLen = pdl->msg_len;
        hAdjust = (mfLen - gd_i_cols);
    }

    snprintf(buf, sizeof(buf) - 1, "%s", &(pdl->ch_display_msg[pdl->display_pos]));
    skn_scroller_pad_right(buf);

    if (strcmp("ser", gd_pch_device_name) == 0 ) {
        set_col_row_position[3] = (unsigned int)line + 1;
        write(lcd_handle, set_col_row_position, sizeof(set_col_row_position));
        skn_time_delay(0.2); // delay(200);
        write(lcd_handle, buf, gd_i_cols - 1);
    } else {
        lcdPosition(lcd_handle, 0, line);
        lcdPuts(lcd_handle, buf);
    }
    if (++pdl->display_pos > hAdjust) {
        pdl->display_pos = 0;
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

    pdm = (PDisplayManager) malloc(sizeof(DisplayManager));
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

    for (index = 0; index < ARY_MAX_DM_LINES; index++) {
        pdl = pdm->pdsp_collection[index] = (PDisplayLine) malloc(sizeof(DisplayLine)); // line x
        memset(pdl, 0, sizeof(DisplayLine));
        strcpy(pdl->cbName, "PDisplayLine");
        strncpy(pdl->ch_display_msg, welcome, (SZ_INFO_BUFF -1));  // load all with welcome message
        pdl->ch_display_msg[(SZ_INFO_BUFF - 1)] = 0;    // terminate string in case
        pdl->msg_len = pdm->msg_len;
        pdl->active = 1;
        if (pdl->msg_len > gd_i_cols) {
            pdl->scroll_enabled = 1;
            skn_scroller_wrap_blanks(pdl->ch_display_msg);
            pdl->ch_display_msg[(SZ_INFO_BUFF - 1)] = 0;    // terminate string in case
            pdl->msg_len = strlen(pdl->ch_display_msg);
        }
    }
    for (index = 0; index < ARY_MAX_DM_LINES; index++) {               // enable link list routing
            next = (((index + 1) == ARY_MAX_DM_LINES) ? 0 : (index + 1));
            prev = (((index - 1) == -1) ? (ARY_MAX_DM_LINES - 1) : (index - 1));
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
    if (pdm->next_line == ARY_MAX_DM_LINES) {
        pdm->next_line = 0; // roll it
    }

    /*
     * load new message */
    strncpy(pdl->ch_display_msg, client_request_message, (SZ_INFO_BUFF-1));
    pdl->ch_display_msg[SZ_INFO_BUFF - 1] = 0;    // terminate string in case
    pdl->msg_len = strlen(pdl->ch_display_msg);
    pdl->active = 1;
    pdl->display_pos = 0;
    if (pdl->msg_len > gd_i_cols) {
        pdl->scroll_enabled = 1;
        skn_scroller_wrap_blanks(pdl->ch_display_msg);
        pdl->ch_display_msg[(SZ_INFO_BUFF - 1)] = 0;    // terminate string in case
        pdl->msg_len = strlen(pdl->ch_display_msg);
    } else {
        pdl->scroll_enabled = 0;
    }

    /*
     * manage current_line */
    pdm->pdsp_collection[pdm->current_line]->active = 0;
    pdm->pdsp_collection[pdm->current_line++]->msg_len = 0;
    if (pdm->current_line == ARY_MAX_DM_LINES) {
        pdm->current_line = 0;
    }

    skn_logger(SD_DEBUG, "DM Added msg=%d:%d:[%s]", ((pdm->next_line - 1) < 0 ? 0 : (pdm->next_line - 1)), pdl->msg_len, pdl->ch_display_msg);

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
    char ch_lcd_message[4][SZ_INFO_BUFF];
    long host_update_cycle = 0;

    gp_structure_pdm = pdm = skn_display_manager_create(client_request_message);
    if (pdm == NULL) {
        gi_exit_flag = SKN_RUN_MODE_STOP;
        skn_logger(SD_ERR, "Display Manager cannot acquire needed resources. DMCreate()");
        return gi_exit_flag;
    }
    generate_datetime_info (ch_lcd_message[0]);
    generate_rpi_model_info(ch_lcd_message[1]);
//    generate_cpu_temps_info(ch_lcd_message[2]);
    generate_uname_info    (ch_lcd_message[2]);
    generate_loadavg_info  (ch_lcd_message[3]);
    skn_display_manager_add_line(pdm, ch_lcd_message[0]);
    skn_display_manager_add_line(pdm, ch_lcd_message[1]);
    skn_display_manager_add_line(pdm, ch_lcd_message[2]);
    skn_display_manager_add_line(pdm, ch_lcd_message[3]);

    if (skn_device_manager_LCD_setup(pdm, gd_pch_device_name) == PLATFORM_ERROR) {
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

    skn_logger(SD_NOTICE, "Application Active... ");

    /*
     *  Do the Work
     */
    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        dsp_line_number = 0;
        pdl = pdm->pdsp_collection[pdm->current_line];
	
        for (index = 0; index < pdm->dsp_rows; index++) {
            if (pdl->active == 1) {
                skn_scroller_scroll_lines(pdl, pdm->lcd_handle, dsp_line_number++);
                skn_time_delay(0.18); // delay(180);
            }
            pdl = (PDisplayLine) pdl->next;
        }

        if ((host_update_cycle++ % 900) == 0) {  // roughly every fifteen minutes
            generate_datetime_info (ch_lcd_message[0]);
//            generate_cpu_temps_info(ch_lcd_message[2]);
            generate_loadavg_info  (ch_lcd_message[3]);
            skn_display_manager_add_line(pdm, ch_lcd_message[0]);
            skn_display_manager_add_line(pdm, ch_lcd_message[1]);
            skn_display_manager_add_line(pdm, ch_lcd_message[2]);
            skn_display_manager_add_line(pdm, ch_lcd_message[3]);
            host_update_cycle = 1;
        }
    }

    skn_device_manager_LCD_shutdown(pdm);

    skn_logger(SD_NOTICE, "Application InActive...");

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
    for (index = 0; index < ARY_MAX_DM_LINES; index++) {
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
    pdm->i_socket = skn_udp_host_create_regular_socket(SKN_RPI_DISPLAY_SERVICE_PORT, 30.0);
    if (pdm->i_socket == EXIT_FAILURE) {
        skn_logger(SD_EMERG, "DisplayManager: Host Init Failed!");
        return EXIT_FAILURE;
    }

    /*
     * Create Thread  */
    int i_thread_rc = pthread_create(&pdm->dm_thread, NULL, skn_display_manager_message_consumer_thread, (void *) pdm);
    if (i_thread_rc == -1) {
        skn_logger(SD_WARNING, "DisplayManager: Create thread failed: %s", strerror(errno));
        close(pdm->i_socket);
        return EXIT_FAILURE;
    }
    sleep(1); // give thread a chance to start

    skn_logger(SD_NOTICE, "DisplayManager: Thread startup successful... ");

    return pdm->i_socket;
}
/**
 * skn_display_manager_message_consumer(PDisplayManager pdm) */
void skn_display_manager_message_consumer_shutdown(PDisplayManager pdm) {
    void *trc = NULL;

    if (pdm->thread_complete != 0) {
        skn_logger(SD_WARNING, "DisplayManager: Canceling thread.");
        pthread_cancel(pdm->dm_thread);
        sleep(1);
    } else {
        skn_logger(SD_WARNING, "DisplayManager: Thread was already stopped.");
    }
    pthread_join(pdm->dm_thread, &trc);
    close(pdm->i_socket);
    skn_logger(SD_NOTICE, "DisplayManager: Thread ended:(%ld)", (long int) trc);
}

/**
 * skn_display_manager_message_consumer_thread(PDisplayManager pdm)
 */

static void * skn_display_manager_message_consumer_thread(void * ptr) {
    PDisplayManager pdm = (PDisplayManager) ptr;
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    IPBroadcastArray aB;
    char strPrefix[SZ_INFO_BUFF];
    char request[SZ_INFO_BUFF];
    char recvHostName[SZ_INFO_BUFF];
    char *pch = NULL;
    signed int rLen = 0, rc = 0;
    long int exit_code = EXIT_SUCCESS;

    bzero(request, sizeof(request));
    memset(recvHostName, 0, sizeof(recvHostName));

    rc = get_broadcast_ip_array(&aB);
    if (rc == -1) {
        exit_code = rc;
        pthread_exit((void *) exit_code);
    }

    pdm->thread_complete = 1;

    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port = htons(SKN_RPI_DISPLAY_SERVICE_PORT);
        remaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        addrlen = sizeof(remaddr);
//                                                                        V- MSG_DONTWAIT or O_NONBLOCK flag with the F_SETFL fcntl(2)
        if ((rLen = recvfrom(pdm->i_socket, request, sizeof(request) - 1, 0, (struct sockaddr *) &remaddr, &addrlen)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            skn_logger(SD_ERR, "DisplayManager: RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }
        request[rLen] = 0;

        rc = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, sizeof(recvHostName) - 1, NULL, 0, NI_DGRAM);
        if (rc != 0) {
            skn_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }
        skn_logger(SD_NOTICE, "Received request from %s @ %s:%d", recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));

        /*
         * Add receive data to display set */
        pch = strtok(recvHostName, ".");
        snprintf(strPrefix, sizeof(strPrefix) -1 , "%s|%s", pch, request);
        skn_display_manager_add_line(pdm, strPrefix);

        if (sendto(pdm->i_socket, "200 Accepted", strlen("200 Accepted"), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_logger(SD_ERR, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }

        /*
         * Shutdown by command */
        if (strcmp("QUIT!", request) == 0) {
            exit_code = 0;
            gi_exit_flag == SKN_RUN_MODE_STOP;  // shutdown
            skn_logger(SD_NOTICE, "COMMAND: Shutdown Requested! exit code=%d", exit_code);
            break;
        }

    }
    gi_exit_flag == SKN_RUN_MODE_STOP;  // shutdown
//    kill(getpid(), SIGUSR1); // cause a shutdown
    skn_time_delay(0.5);

    skn_logger(SD_NOTICE, "Display Manager Thread: shutdown complete: (%ld)", exit_code);

    pdm->thread_complete = 0;

    pthread_exit((void *) exit_code);

}

/**************************************************************************
 Function: Print Usage for lcd_display_service only

 Description:
 Output the command-line options for this daemon.

 Returns:
 returns void always
 **************************************************************************/
static void skn_display_print_usage() {
    skn_logger(" ", "%s -- %s", gd_ch_program_name, gd_ch_program_desc);
    skn_logger(" ", "\tSkoona Development <skoona@gmail.com>");
    skn_logger(" ", "Usage:\n  %s [-v] [-m 'Welcome Message'] [-r 4|2] [-c 20|16] [-i 39|32] [-t pcf|mcp|mc7|ser] [-p string] [-h|--help]", gd_ch_program_name);
    skn_logger(" ", "\nOptions:");
    skn_logger(" ", "  -r, --rows=dd\t\tNumber of rows in physical display.");
    skn_logger(" ", "  -c, --cols=dd\t\tNumber of columns in physical display.");
    skn_logger(" ", "  -m, --message\tWelcome Message for line 1.");
    skn_logger(" ", "  -p, --serial-port=string\tSerial port.      | ['/dev/ttyACM0']");
    skn_logger(" ", "  -i, --i2c-address=ddd\tI2C decimal address. | [0x27=39, 0x20=32]");
    skn_logger(" ", "  -t, --i2c-chipset=pcf\tI2C Chipset.         | [pcf|mc7|mcp|ser]");
    skn_logger(" ", "  -v, --version\tVersion printout.");
    skn_logger(" ", "  -h, --help\t\tShow this help screen.");
}

/* *****************************************************
 *  Parse out the command line options from argc,argv
 *  results go to globals
 *  EXIT_FAILURE on any error, or version and help
 *  EXIT_SUCCESS on normal completion
 */
int skn_handle_display_command_line(int argc, char **argv) {
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

    /*
     * Get commandline options
     *  longindex is the current index into longopts
     *  optind is current/last index into argv
     *  optarg is value attached(-d88) or next element(-d 88) of argv
     *  opterr flags a scanning error
     */
    while ((opt = getopt_long(argc, argv, "d:m:r:c:i:t:p:vh", longopts, &longindex)) != -1) {
        switch (opt) {
            case 'd':
                if (optarg) {
                    gd_i_debug = atoi(optarg);
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
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
                            skn_logger(SD_WARNING, "%s: input param was invalid! (default of 4 used) %c[%d:%d:%d]\n", gd_ch_program_name,
                                            (char) opt, longindex, optind, opterr);

                    }
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
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
                            skn_logger(SD_WARNING, "%s: input param was invalid! (default of 20 used) %c[%d:%d:%d]\n", gd_ch_program_name,
                                            (char) opt, longindex, optind, opterr);

                    }
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'm':
                if (optarg) {
                    gd_pch_message = strdup(optarg);
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'i':
                if (optarg) {
                    gd_i_i2c_address = atoi(optarg);
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
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
                        skn_logger(SD_ERR, "%s: unsupported option was invalid! %c[%d:%d:%d] %s\n", gd_ch_program_name, (char) opt, longindex, optind, opterr, gd_pch_device_name);
                        return EXIT_FAILURE;
                    }
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind,
                                    opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'p':
                if (optarg) {
                    gd_pch_serial_port = strdup(optarg);
                    if (strlen(gd_pch_serial_port) < 5) {
                        skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                        return EXIT_FAILURE;
                    }
                } else {
                    skn_logger(SD_ERR, "%s: input param was invalid! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                    return (EXIT_FAILURE);
                }
                break;
            case 'v':
                skn_logger(SD_ERR, "\n\tProgram => %s\n\tVersion => %s\n\tSkoona Development\n\t<skoona@gmail.com>\n", gd_ch_program_name,
                                PACKAGE_VERSION);
                return (EXIT_FAILURE);
                break;
            case '?':
                skn_logger(SD_ERR, "%s: unknown input param! %c[%d:%d:%d]\n", gd_ch_program_name, (char) opt, longindex, optind, opterr);
                skn_display_print_usage();
                return (EXIT_FAILURE);
                break;
            default: /* help and default */
                skn_display_print_usage();
                return (EXIT_FAILURE);
                break;
        }
    }

    return EXIT_SUCCESS;
}
