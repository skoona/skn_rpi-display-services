/*
 * skn_display_manager.h
 *
 *  Created on: Jun 3, 2016
 *      Author: jscott
 */


#ifndef SKN_DISPLAY_MANAGER_H_
#define SKN_DISPLAY_MANAGER_H_

#include "skn_network_helpers.h"

typedef struct _DISPLAY_LINE {
    char cbName[SZ_CHAR_BUFF];
    int  active;
    char ch_display_msg[SZ_INFO_BUFF];
    int  msg_len;
    int  display_pos;
    int  scroll_enabled;
    void * next;
    void * prev;
} DisplayLine, *PDisplayLine;

typedef struct _DISPLAY_MANAGER {
    char cbName[SZ_CHAR_BUFF];
    char ch_welcome_msg[SZ_INFO_BUFF];
    int  msg_len;
    int  display_pos;
    int  dsp_rows;
    int  dsp_cols;
    PDisplayLine pdsp_collection[ARY_MAX_DM_LINES]; // all available lines
    int  current_line; // top of display
    int  next_line;  // actual index  -- should be within display_lines of current
    int  lcd_handle;
    pthread_t dm_thread;   // new message thread
    long thread_complete;
    int  i_socket;
    LCDDevice lcd;  // selected device
} DisplayManager, *PDisplayManager;

extern int gd_i_rows;
extern int gd_i_cols;

extern int    skn_display_manager_message_consumer_startup(PDisplayManager pdm);
extern void   skn_display_manager_message_consumer_shutdown(PDisplayManager pdm);
extern int    skn_display_manager_do_work(char * client_request_message);

extern int    skn_display_manager_scroller_scroll_lines(PDisplayLine pdl, int lcd_handle, int line);
extern char * skn_display_manager_scroller_pad_right(char *buffer);
extern char * skn_display_manager_scroller_wrap_blanks(char *buffer);
extern PDisplayManager skn_display_manager_get_ref();
extern PDisplayLine skn_display_manager_add_line(PDisplayManager pdmx, char * client_request_message);

#endif /* SKN_DISPLAY_MANAGER_H_ */
