/*
 * skn_display_manager.c
 *
 *  Created on: Jun 3, 2016
 *      Author: jscott
 */

#include "skn_display_manager.h"

PDisplayManager gp_structure_pdm = NULL;


static PDisplayManager skn_display_manager_create(char * welcome);
static void   skn_display_manager_destroy(PDisplayManager pdm);
static void * skn_display_manager_message_consumer_thread(void * ptr);


/**
 * skn_display_manager_scroller_pad_right
 * - fills remaining with spaces and 0 terminates
 * - buffer   message to adjust
 */
char * skn_display_manager_scroller_pad_right(char *buffer) {
    int hIndex = 0;

    for (hIndex = strlen(buffer); hIndex < gd_i_cols; hIndex++) {
        buffer[hIndex] = ' ';
    }
    buffer[gd_i_cols] = 0;

    return buffer;
}

/**
 * skn_display_manager_scroller_wrap_blanks
 *  - builds str with 20 chars in front, and 20 at right end
 */
char * skn_display_manager_scroller_wrap_blanks(char *buffer) {
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
int skn_display_manager_scroller_scroll_lines(PDisplayLine pdl, int lcd_handle, int line)
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
    skn_display_manager_scroller_pad_right(buf);

    if (strcmp("ser", gd_pch_device_name) == 0 ) {
        set_col_row_position[3] = (unsigned int)line + 1;
        write(lcd_handle, set_col_row_position, sizeof(set_col_row_position));
        skn_util_time_delay_ms(0.2); // delay(200);
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
        skn_util_logger(SD_ERR, "Display Manager cannot acquire needed resources. %d:%s", errno, strerror(errno));
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
            skn_display_manager_scroller_wrap_blanks(pdl->ch_display_msg);
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

    pdm = ((pdmx == NULL) ? skn_display_manager_get_ref() : pdmx);
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
        skn_display_manager_scroller_wrap_blanks(pdl->ch_display_msg);
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

    skn_util_logger(SD_DEBUG, "DM Added msg=%d:%d:[%s]", ((pdm->next_line - 1) < 0 ? 0 : (pdm->next_line - 1)), pdl->msg_len, pdl->ch_display_msg);

    /* return this line's pointer */
    return pdl;
}

PDisplayManager skn_display_manager_get_ref() {
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
        skn_util_logger(SD_ERR, "Display Manager cannot acquire needed resources. DMCreate()");
        return gi_exit_flag;
    }
    skn_util_generate_datetime_info (ch_lcd_message[0]);
    skn_util_generate_rpi_model_info(ch_lcd_message[1]);
//  skn_generate_cpu_temp_info(ch_lcd_message[2]);
    skn_util_generate_uname_info    (ch_lcd_message[2]);
    skn_util_generate_loadavg_info  (ch_lcd_message[3]);
    skn_display_manager_add_line(pdm, ch_lcd_message[0]);
    skn_display_manager_add_line(pdm, ch_lcd_message[1]);
    skn_display_manager_add_line(pdm, ch_lcd_message[2]);
    skn_display_manager_add_line(pdm, ch_lcd_message[3]);

    if (skn_device_manager_LCD_setup(pdm, gd_pch_device_name) == PLATFORM_ERROR) {
        gi_exit_flag = SKN_RUN_MODE_STOP;
        skn_util_logger(SD_ERR, "Display Manager cannot acquire needed resources: lcdSetup().");
        skn_display_manager_destroy(pdm);
        return gi_exit_flag;
    }

    if (skn_display_manager_message_consumer_startup(pdm) == EXIT_FAILURE) {
        gi_exit_flag = SKN_RUN_MODE_STOP;
        skn_util_logger(SD_ERR, "Display Manager cannot acquire needed resources: Consumer().");
        skn_display_manager_destroy(pdm);
        return gi_exit_flag;
    }

    skn_util_logger(SD_NOTICE, "Application Active... ");

    /*
     *  Do the Work
     */
    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        dsp_line_number = 0;
        pdl = pdm->pdsp_collection[pdm->current_line];

        for (index = 0; index < pdm->dsp_rows; index++) {
            if (pdl->active == 1) {
                skn_display_manager_scroller_scroll_lines(pdl, pdm->lcd_handle, dsp_line_number++);
                skn_util_time_delay_ms(0.18); // delay(180);
            }
            pdl = (PDisplayLine) pdl->next;
        }

        if ((host_update_cycle++ % 900) == 0) {  // roughly every fifteen minutes
            skn_util_generate_datetime_info (ch_lcd_message[0]);
//          skn_util_generate_cpu_temp_info(ch_lcd_message[2]);
            skn_util_generate_loadavg_info  (ch_lcd_message[3]);
            skn_display_manager_add_line(pdm, ch_lcd_message[0]);
            skn_display_manager_add_line(pdm, ch_lcd_message[1]);
            skn_display_manager_add_line(pdm, ch_lcd_message[2]);
            skn_display_manager_add_line(pdm, ch_lcd_message[3]);
            host_update_cycle = 1;
        }
    }

    skn_device_manager_LCD_shutdown(pdm);

    skn_util_logger(SD_NOTICE, "Application InActive...");

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
        skn_util_logger(SD_EMERG, "DisplayManager: Host Init Failed!");
        return EXIT_FAILURE;
    }

    /*
     * Create Thread  */
    int i_thread_rc = pthread_create(&pdm->dm_thread, NULL, skn_display_manager_message_consumer_thread, (void *) pdm);
    if (i_thread_rc == -1) {
        skn_util_logger(SD_WARNING, "DisplayManager: Create thread failed: %s", strerror(errno));
        close(pdm->i_socket);
        return EXIT_FAILURE;
    }
    sleep(1); // give thread a chance to start

    skn_util_logger(SD_NOTICE, "DisplayManager: Thread startup successful... ");

    return pdm->i_socket;
}

/**
 * skn_display_manager_message_consumer(PDisplayManager pdm) */
void skn_display_manager_message_consumer_shutdown(PDisplayManager pdm) {
    void *trc = NULL;

    if (pdm->thread_complete != 0) {
        skn_util_logger(SD_WARNING, "DisplayManager: Canceling thread.");
        pthread_cancel(pdm->dm_thread);
        sleep(1);
    } else {
        skn_util_logger(SD_WARNING, "DisplayManager: Thread was already stopped.");
    }
    pthread_join(pdm->dm_thread, &trc);
    close(pdm->i_socket);
    skn_util_logger(SD_NOTICE, "DisplayManager: Thread ended:(%ld)", (long int) trc);
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

    rc = skn_util_get_broadcast_ip_array(&aB);
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

        if ((rLen = recvfrom(pdm->i_socket, request, sizeof(request) - 1, 0, (struct sockaddr *) &remaddr, &addrlen)) < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            skn_util_logger(SD_ERR, "DisplayManager: RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }
        request[rLen] = 0;

        rc = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, sizeof(recvHostName) - 1, NULL, 0, NI_DGRAM);
        if (rc != 0) {
            skn_util_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }
        skn_util_logger(SD_NOTICE, "Received request from %s @ %s:%d", recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));

        /*
         * Add receive data to display set */
        pch = strtok(recvHostName, ".");
        snprintf(strPrefix, sizeof(strPrefix) -1 , "%s|%s", pch, request);
        skn_display_manager_add_line(pdm, strPrefix);

        if (sendto(pdm->i_socket, "202 Accepted", strlen("202 Accepted"), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_util_logger(SD_ERR, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = errno;
            break;
        }

        /*
         * Shutdown by command */
        if (strcmp("QUIT!", request) == 0) {
            exit_code = 0;
            gi_exit_flag = SKN_RUN_MODE_STOP;  // shutdown
            skn_util_logger(SD_NOTICE, "COMMAND: Shutdown Requested! exit code=%d", exit_code);
            break;
        }

    }
    gi_exit_flag = SKN_RUN_MODE_STOP;  // shutdown
    skn_util_time_delay_ms(0.5);

    skn_util_logger(SD_NOTICE, "Display Manager Thread: shutdown complete: (%ld)", exit_code);

    pdm->thread_complete = 0;

    pthread_exit((void *) exit_code);

}
