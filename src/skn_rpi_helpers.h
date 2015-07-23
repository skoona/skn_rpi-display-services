/*
 * skn_rpi_helpers.h
*/

#ifndef SKN_RPI_HELPERS_H__
#define SKN_RPI_HELPERS_H__

#include "skn_common_headers.h"

#include <sys/utsname.h>

#include <wiringPi.h>
#include <pcf8574.h>
#include <lcd.h>

/*
 * Defines for the 'YwRobot Arduino LCM1602 IIC V1'
 * - which use the I2C controller PCF8574
*/

#define	AF_BASE		100
#define	AF_BACKLIGHT (AF_BASE + 3)

#define	AF_E		(AF_BASE + 2)
#define	AF_RW		(AF_BASE + 1)
#define	AF_RS		(AF_BASE + 0)

#define	AF_DB4		(AF_BASE + 4)
#define	AF_DB5		(AF_BASE + 5)
#define	AF_DB6		(AF_BASE + 6)
#define	AF_DB7		(AF_BASE + 7)


/*
 * Display Manager Routines */
extern PDisplayManager skn_get_display_manager_ref();
extern int skn_display_manager_do_work(char * client_request_message);
extern PDisplayLine skn_display_manager_add_line(PDisplayManager pdmx, char * client_request_message);
extern int skn_scroller_scroll_lines(PDisplayLine pdl, int lcd_handle, int line);
extern char * skn_scroller_pad_right(char *buffer);
extern char * skn_scroller_wrap_blanks(char *buffer);

/*
 * Display Manager Communications Routines */
extern int skn_signal_manager_startup(pthread_t *psig_thread, sigset_t *psignal_set, uid_t *preal_user_id);
extern int skn_signal_manager_shutdown(pthread_t sig_thread, sigset_t *psignal_set);


/* WiringPi LCD Interfaces
*/
extern void skn_lcd_backlight_set(int state);
extern int skn_pcf8574LCDSetup (PDisplayManager pdm, int backLight);

/* General Utilities
*/
extern long skn_get_number_of_cpu_cores();
extern int skn_handle_display_command_line(int argc, char **argv);

/* Scrolling Display Info Messages
*/
extern int generate_rpi_model_info(char *msg);
extern int generate_uname_info(char *msg);
extern int generate_datetime_info(char *msg);
extern int generate_cpu_temps_info(char *msg);

/*
 *  Global lcd handle:
*/
extern int gd_i_rows;
extern int gd_i_cols;
extern PDisplayManager gp_structure_pdm;

#endif // SKN_RPI_HELPERS_H__
