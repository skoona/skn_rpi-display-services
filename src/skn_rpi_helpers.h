/*
 * skn_rpi_helpers.h
*/

#ifndef SKN_RPI_HELPERS_H__
#define SKN_RPI_HELPERS_H__

#include "skn_common_headers.h"

#include <sys/utsname.h>

#include <wiringPi.h>
#include <pcf8574.h>
#include <mcp23008.h>
#include <mcp23017.h>
#include <wiringSerial.h>   // http://wiringpi.com/reference/serial-library/
#include <lcd.h>


/*
 *  Global Defines */
extern int gd_i_rows;
extern int gd_i_cols;
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
extern PDisplayManager skn_get_display_manager_ref();
extern int skn_display_manager_do_work(char * client_request_message);
extern PDisplayLine skn_display_manager_add_line(PDisplayManager pdmx, char * client_request_message);
extern int skn_scroller_scroll_lines(PDisplayLine pdl, int lcd_handle, int line);
extern char * skn_scroller_pad_right(char *buffer);
extern char * skn_scroller_wrap_blanks(char *buffer);

/*
 * Display Manager Communications Routines */
extern int skn_signal_manager_startup(pthread_t *psig_thread, sigset_t *psignal_set, long *l_thread_complete);
extern int skn_signal_manager_shutdown(pthread_t sig_thread, sigset_t *psignal_set, long *l_thread_complete);


/* WiringPi LCD Interfaces
*/
extern void skn_device_manager_backlight(int af_backlight, int state);
extern int skn_device_manager_LCD_setup (PDisplayManager pdm, char *device_name);
extern int skn_device_manager_LCD_shutdown(PDisplayManager pdm);

/* General Utilities
*/
extern int skn_handle_display_command_line(int argc, char **argv);

/* Scrolling Display Info Messages
*/
extern int generate_rpi_model_info(char *msg);
extern int generate_cpu_temps_info(char *msg);

#endif // SKN_RPI_HELPERS_H__
