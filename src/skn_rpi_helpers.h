/*
 * skn_rpi_helpers.h
*/

#ifndef SKN_RPI_HELPERS_H__
#define SKN_RPI_HELPERS_H__

#include <sys/utsname.h>

#include <wiringPi.h>
#include <pcf8574.h>
#include <mcp23008.h>
#include <mcp23017.h>
#include <wiringSerial.h>   // http://wiringpi.com/reference/serial-library/
#include <lcd.h>
#include "skn_commons.h"


/*
 *  Global Defines */
extern int gd_i_i2c_address;
extern char *gd_pch_serial_port;
extern char *gd_pch_device_name;
extern PDisplayManager gp_structure_pdm;

/*
 * Display Manager Routines */
extern PLCDDevice skn_device_manager_SerialPort(PDisplayManager pdm);
extern PLCDDevice skn_device_manager_MCP23008(PDisplayManager pdm);
extern PLCDDevice skn_device_manager_MCP23017(PDisplayManager pdm);
extern PLCDDevice skn_device_manager_PCF8574(PDisplayManager pdm);


/* WiringPi LCD Interfaces
*/
extern void skn_device_manager_backlight(int af_backlight, int state);
extern int skn_device_manager_LCD_setup (PDisplayManager pdm, char *device_name);
extern int skn_device_manager_LCD_shutdown(PDisplayManager pdm);

/* General Utilities
*/
extern int skn_display_service_command_line_parse(int argc, char **argv);

/* Scrolling Display Info Messages
*/
extern int skn_util_generate_rpi_model_info(char *msg);
extern int skn_util_generate_cpu_temp_info(char *msg);

#endif // SKN_RPI_HELPERS_H__
