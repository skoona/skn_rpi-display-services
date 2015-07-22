/*
 * skn_rpi_helpers.h
*/

#ifndef SKN_RPI_HELPERS_H__
#define SKN_RPI_HELPERS_H__

#include <sys/utsname.h>

#include <wiringPi.h>
#include <pcf8574.h>
#include <lcd.h>


#ifndef	TRUE
#  define	TRUE	(1==1)
#  define	FALSE	(1==2)
#endif

#define SCROLL_WAIT    1
#define SCROLL_NOWAIT  0
#define MAX_DISPLAY_ROWS 4
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
 * Char bufs for cpu temperature
*/
#define TZ_ADJUST 4
typedef struct _temps {
	char cbName[SZ_CHAR_LABEL];
	char c[SZ_CHAR_LABEL];
	char f[SZ_CHAR_LABEL];
	long raw;
} CpuTemps, *PCpuTemps;




/* WiringPi LCD Interfaces
*/
extern void skn_lcd_backlight_set(int state);
extern int skn_pcf8574LCDSetup (int backLight);
extern int skn_scroller_scroll_line(int position, int line, int columns, const char *msg, int wait);

/* General Utilities
*/
extern long skn_get_number_of_cpu_cores();
extern char * skn_scroller_pad_right(char *buffer);
extern char * skn_scroller_wrap_blanks(char *buffer);
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
extern int gd_i_lcd_handle ;
extern int gd_i_rows;
extern int gd_i_cols;

#endif // SKN_RPI_HELPERS_H__
