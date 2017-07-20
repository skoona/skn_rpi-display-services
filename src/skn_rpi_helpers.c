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
char *gd_pch_serial_port;
char *gd_pch_device_name = "pcf";


static PLCDDevice skn_device_manager_init_i2c(PDisplayManager pdm);


/*
 * Device Methods
*/
PLCDDevice skn_device_manager_SerialPort(PDisplayManager pdm) {
    PLCDDevice plcd =  NULL;

    if (pdm == NULL) {
        skn_util_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#SerialPort", SZ_CHAR_BUFF-1);
    if (gd_pch_serial_port != NULL) {
        strncpy(plcd->ch_serial_port_name, gd_pch_serial_port, SZ_CHAR_BUFF-1);
    } else {
        strncpy(plcd->ch_serial_port_name, "/dev/ttyACM0", SZ_CHAR_BUFF-1);
    }

    skn_util_logger(SD_NOTICE, "DeviceManager using  device [%s](%s)", plcd->cbName, gd_pch_serial_port);

    pdm->lcd_handle = plcd->lcd_handle =  serialOpen (plcd->ch_serial_port_name, 9600);
    if (plcd->lcd_handle == PLATFORM_ERROR) {
        skn_util_logger(SD_ERR, "DeviceManager failed to acquire needed resources: SerialPort=%s %d:%s",
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
        skn_util_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#MCP23017", SZ_CHAR_BUFF-1);
    if (gd_i_i2c_address != 0) {
        plcd->i2c_address = gd_i_i2c_address;
    } else {
        plcd->i2c_address = 0x20;
    }

    skn_util_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

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
        skn_util_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
        return NULL;
    }

    plcd = (PLCDDevice)&pdm->lcd;
    strncpy(plcd->cbName, "LCDDevice#MCP23008", SZ_CHAR_BUFF-1);
    if (gd_i_i2c_address != 0) {
        plcd->i2c_address = gd_i_i2c_address;
    } else {
        plcd->i2c_address = 0x20;
    }

    skn_util_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

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
        skn_util_logger(SD_ERR, "DeviceManager failed to acquire needed resources. %d:%s", errno, strerror(errno));
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

    skn_util_logger(SD_NOTICE, "DeviceManager using device [%s](0x%02x)", plcd->cbName, plcd->i2c_address);

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
        skn_util_logger(SD_ERR, "I2C Services failed to initialize. lcdInit(%d)", pdm->lcd_handle);
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
 *
 * Redhat/Centos: /sys/class/hwmon/hwmon0/device/temp1_input
 * Ubuntu/Debian: /sys/class/thermal/thermal_zone0/temp
 *
*/
long skn_util_get_cpu_temp(PCpuTemps temps) {
    long lRaw = 0;
    int rc = 0;

    FILE *sysFs = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (sysFs == NULL) {
        skn_util_logger(SD_WARNING, "Warning: Failed to open Debian CPU temperature: %d:%s\n", errno, strerror(errno));
        sysFs = fopen("/sys/class/hwmon/hwmon0/device/temp1_input", "r");
        if (sysFs == NULL) {
            skn_util_logger(SD_WARNING, "Warning: Failed to open Centos CPU temperature: %d:%s\n", errno, strerror(errno));
            return -1;
        }
    }

    rc = fscanf(sysFs, "%ld", &lRaw);
    fclose(sysFs);

    if (rc != 1 || lRaw < 0) {
        skn_util_logger(SD_WARNING, "Warning: Failed to READ CPU temperature: %d:%s\n", strerror(errno));
        return -1;
    }

    if (temps != NULL) { // populate struct
        snprintf(temps->c, sizeof(temps->c), "%3.1fC", (double )(lRaw / 1000.0));
        snprintf(temps->f, sizeof(temps->f), "%3.1fF", (double )(lRaw / 1000.0 * 9 / 5 + 32));
        temps->raw = lRaw;
        strncpy(temps->cbName, "CpuTemps", sizeof(temps->cbName) - 1);
    }

    return lRaw;
}

/*
 * Message Builders exclusively for Raspberry Pis
 */
int skn_util_generate_rpi_model_info(char *msg) {
    int model = 0, rev = 0, mem = 0, maker = 0, overVolted = 0, mLen = 0;
    char * message = "Device has an unknown model type.\n";

    piBoardId(&model, &rev, &mem, &maker, &overVolted);
    if (model == PI_MODEL_UNKNOWN) {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "%s", message);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF -1, "Device: %s, Cpus: %ld, Rev: %s, Mem: %dMB, Maker: %s %s, %s:%s", piModelNames[model],
                        skn_util_generate_number_cpu_cores(), piRevisionNames[rev], mem, piMakerNames[maker], overVolted ? "[OV]" : "", gd_ch_intfName,
                        gd_ch_ipAddress);
    }
    return mLen;
}

/**
 * DO NOT USE THIS IN MODULES THAT HANDLE A I2C Based LCD
 * RPi cannot handle I2C and GetCpuTemp() without locking the process
 * in an uninterrupted sleep; forcing a power cycle.
*/
int skn_util_generate_cpu_temp_info(char *msg) {
    static CpuTemps cpuTemp;
    int mLen = 0;

    memset(&cpuTemp, 0, sizeof(CpuTemps));
    if ( skn_util_get_cpu_temp(&cpuTemp) != -1 ) {
        mLen = snprintf(msg, SZ_INFO_BUFF-1, "CPU: %s %s", cpuTemp.c, cpuTemp.f);
    } else {
        mLen = snprintf(msg, SZ_INFO_BUFF-1, "Temp: N/A");
    }

    return mLen;
}
