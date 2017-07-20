/**
 * cmdDC.c
 * IOT/RaspberryPi message display client
 *  gcc -v `pkg-config --cflags --libs glib-2.0 gio-2.0` -O3 -Wall -o cmdDC cmdDC.c
 *
 * Program Flow:
 *  1. Initialize    -> parse_options
 *  2. upd_broad     -> loop -> receive_broadcast_message_and_update_regitry
 *  2. udp_timeout   -> loop -> find_display_service_in_registry -> intialize_message_sender
 *  3. upd_socket    -> loop -> receive_message -> log_to__console
 *  3. unix_signal   -> loop -> close_loop
 *  4. Cleanup       -> exit
*/

/*
 * Program Standards passed from compiler
 */
#ifndef PACKAGE_VERSION
    #define PACKAGE_VERSION "0.9.0"
#endif
#ifndef PACKAGE_NAME
    #define PACKAGE_NAME "cmdDC"
#endif
#ifndef PACKAGE_DESCRIPTION
    #define PACKAGE_DESCRIPTION "Send Message to Raspberry Pi's on the network."
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gio/gnetworking.h>
#include <ifaddrs.h>

#ifndef G_OPTION_FLAG_NONE
    #define G_OPTION_FLAG_NONE 0
#endif


#define UDP_DISPLAY_COMM_PORT 48029
#define UDP_BROADCAST_PORT    48028
#define UDP_REGULAR_PORT      48027
#define UDP_CLIENTS_PORT      48026
#define MS_TEN_MINUTES       600000
#define MSG_DELAY_INTERVAL       10

#define SZ_TIMESTAMP_BUFF 32
#define SZ_PARAMS_BUFF    64
#define SZ_RMTADDR_BUFF  256
#define SZ_MESSAGE_BUFF  512
#define SZ_RESPONSE_BUFF 256

#define SZ_CHAR_LABEL 48
#define SZ_INFO_BUFF 256
#define SZ_CHAR_BUFF 128
#define SZ_LINE_BUFF 512
#define SZ_COMM_BUFF 256

#define SKN_UDP_ANY_PORT 0
#define ARY_MAX_INTF 8
#define PLATFORM_ERROR -1

typedef struct _ipBroadcastArray {
    char cbName[SZ_CHAR_BUFF];
    char chDefaultIntfName[SZ_CHAR_BUFF];
    char ifNameStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char ipAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char maskAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    char broadAddrStr[ARY_MAX_INTF][SZ_CHAR_BUFF];
    int  defaultIndex;
    int  count; // index = count - 1
} IPBroadcastArray, *PIPBroadcastArray;

typedef struct _signalData {
    GMainLoop *loop;
    gchar * signalName;
} USignalData, *PUSignalData;

typedef struct _registryData {
  gchar ch_timestamp[SZ_TIMESTAMP_BUFF];
  gchar ch_from[SZ_RMTADDR_BUFF];
  gchar ch_name[SZ_RMTADDR_BUFF];
  gchar ch_ip[SZ_PARAMS_BUFF];
  gchar ch_port[SZ_PARAMS_BUFF];
  gchar pch_message[SZ_MESSAGE_BUFF];
} RegData, *PRegData, **PPRegData;

typedef struct _controlData {
    GMainLoop *loop;
    USignalData sigTerm;
    USignalData sigInt;
    USignalData sigHup;
    GSource *gCommSource;
    GSocket *gSock;
    GSocket *gBroadcastSocket;
    GSocketAddress *gsDSAddr;
    GSocketAddress *gsAddr;
    GResolver *resolver;
    GList *glRegistry;
    PIPBroadcastArray paB;
    guint gRegistryQueries;
    guint gRegCount;
    guint gErrorCount;
    guint gMsgDelay;
    gboolean gReady;
    gchar ch_intfName[SZ_RMTADDR_BUFF];
    gchar ch_this_ip[SZ_RMTADDR_BUFF];
    gchar ch_display_service_name[SZ_RMTADDR_BUFF];
    gchar ch_message[SZ_MESSAGE_BUFF];
    gchar ch_request[SZ_MESSAGE_BUFF];
    gchar ch_response[SZ_RESPONSE_BUFF];
} ControlData, *PControlData;


gchar  *  skn_get_timestamp();
gint      udp_registry_find_by_name(PRegData pr, gchar *pch_name);
PPRegData udp_registry_response_parser(PRegData msg, gchar *response);
gchar  *  skn_gio_condition_to_string(GIOCondition condition);
gboolean  udp_initialize_message_send(PControlData pctrl);

static gboolean cb_unix_signal_handler(PUSignalData psig);
static gboolean cb_udp_comm_request_handler(PControlData pctrl);
static gboolean cb_udp_comm_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl);
static gboolean cb_udp_broadcast_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl);
static gboolean cb_udp_registry_select_handler(PControlData pctrl);

PIPBroadcastArray skn_util_get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);
gint skn_util_get_broadcast_ip_array(PIPBroadcastArray paB);
gint skn_util_get_default_interface_name(char *pchDefaultInterfaceName);
gboolean skn_udp_network_broadcast_all_interfaces(GSocket *gSock, PIPBroadcastArray pab);
gchar * skn_util_strip(gchar * alpha);

/**
 * Remove Trailing and Leading Blanks
 * - caution: pointers from argv are readonly and segfault on 'alpha[end] = 0'
 */
gchar * skn_util_strip(gchar * alpha) {
    if (alpha == NULL || strlen(alpha) < 1)
        return(alpha);

    int len = strlen(alpha);
    int end = len - 1;
    int start = 0;

    // use isgraph() or !isspace() vs isalnum() to allow ['|' ';' '%']
    while ( !g_unichar_isgraph(alpha[end]) && end > 0 ) { // remove trailing non-alphanumeric chars
        alpha[end--] = 0;
    }

    len = strlen(alpha);
    while ( !g_unichar_isalnum(alpha[start]) && start < len ) { // find first non-alpha stricter
        start++;
    }
    if (start < len && start > 0) {  // move in place
        end = len - start;
        memmove(&alpha[0], &alpha[start], end);
        alpha[end] = 0;
    }

    return(alpha);
}

PIPBroadcastArray skn_util_get_default_interface_name_and_ipv4_address(gchar * intf, gchar * ipv4) {
    PIPBroadcastArray paB = g_new0(IPBroadcastArray, 1);

    if (skn_util_get_broadcast_ip_array(paB) != PLATFORM_ERROR) {
        g_utf8_strncpy(intf, paB->chDefaultIntfName, SZ_CHAR_BUFF);
        g_utf8_strncpy(ipv4, paB->ipAddrStr[paB->defaultIndex], SZ_CHAR_BUFF);
    } else {
        g_warning("[REGISTRY] InterfaceName and Address: unable to access information.");
        return(NULL);
    }
    return (paB);
}

/**
 * Collect IP and Broadcast Addresses for this machine
 *
 * - Affects the PIPBroadcastArray
 * - Return -1 on error, or count of interfaces
 * - contains this ipAddress in paB->ipAddrStr[paB->defaultIndex]
 */
gint skn_util_get_broadcast_ip_array(PIPBroadcastArray paB) {
    struct ifaddrs * ifap;
    struct ifaddrs * p;
    gint rc = 0;

    memset(paB, 0, sizeof(IPBroadcastArray));
    paB->count = 0;
    paB->defaultIndex = 0;
    strcpy(paB->cbName, "IPBroadcastArray");

    rc = skn_util_get_default_interface_name(paB->chDefaultIntfName);
    if (rc == EXIT_FAILURE) { // Alternate method for Mac: 'route -n -A inet'
        g_warning("[REGISTRY] No Default Network Interfaces Found!.");
        paB->chDefaultIntfName[0] = 0;
    }
    rc = getifaddrs(&ifap);
    if (rc != 0) {
        g_warning("[REGISTRY] No Network Interfaces Found at All ! %d:%d:%s", rc, errno, strerror(errno) );
        return (PLATFORM_ERROR);
    }
    p = ifap;

    while (p && (paB->count < ARY_MAX_INTF)) {
        if (p->ifa_addr != NULL && p->ifa_addr->sa_family == AF_INET && ((p->ifa_flags & IFF_BROADCAST) > 0)) {

            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_addr)->sin_addr, paB->ipAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_netmask)->sin_addr, paB->maskAddrStr[paB->count], (SZ_CHAR_BUFF - 1));
            inet_ntop(p->ifa_addr->sa_family, &((struct sockaddr_in *) p->ifa_broadaddr)->sin_addr, paB->broadAddrStr[paB->count], (SZ_CHAR_BUFF - 1));

            strncpy(paB->ifNameStr[paB->count], p->ifa_name, (SZ_CHAR_BUFF -1));

            /* Take match as the default */
            if (strcmp(paB->chDefaultIntfName, p->ifa_name) == 0) {
                paB->defaultIndex = paB->count;
            }

            paB->count++;
        }
        p = p->ifa_next;
    } // end while
    freeifaddrs(ifap);

    return (paB->count);
}

/**
 * Retrieves default internet interface name into param
 * - absolute best way to do this, but not supported on Darwin(i.e OSX)
 * return EXIT_SUCCESS or EXIT_FAILURE
 *
 * [jscott@jscott5365m OSX]$ route -n get 0.0.0.0
 *   route to: default
 *destination: default
 *       mask: default
 *    gateway: 10.21.1.254
 *  interface: en3
 *      flags: <UP,GATEWAY,DONE,STATIC,PRCLONING>
 * recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire
 *       0         0         0         0         0         0      1500         0
 *
*/
gint skn_util_get_default_interface_name(char *pchDefaultInterfaceName) {
    FILE *f_route;
    char line[SZ_INFO_BUFF], *dRoute = NULL, *iName = NULL;

    f_route = fopen("/proc/net/route", "r");
    if (f_route != NULL) {
        while (fgets(line, SZ_INFO_BUFF - 1, f_route)) {
            iName = strtok(line, "\t");
            dRoute = strtok(NULL, "\t");

            if (iName != NULL && dRoute != NULL) {
                if (strcmp(dRoute, "00000000") == 0) {
                    strncpy(pchDefaultInterfaceName, iName, (SZ_INFO_BUFF - 1));
                    break;
                }
            }
        }
        fclose(f_route);

        return (EXIT_SUCCESS);
    }
   g_print("[REGISTRY] Opening ProcFs for RouteInfo Failed: %d:%s, Alternate method will be attempted.\n", errno, strerror(errno));

    f_route = popen("route -n get 0.0.0.0", "r"); // for linux 'route -n -A inet', with interface at line_word[7]
    if (f_route != NULL) {
        while (fgets(line, SZ_INFO_BUFF - 1, f_route)) {
            dRoute = strtok(line, ":");
            iName = strtok(NULL, "\n");
            if (strcmp(skn_util_strip(dRoute), "interface") == 0) {
                strncpy(pchDefaultInterfaceName, skn_util_strip(iName), (SZ_INFO_BUFF - 1));
                break;
            }
        }
        fclose(f_route);

        return (EXIT_SUCCESS);
    } else {
        g_warning("[REGISTRY] Alternate method to get RouteInfo Failed: %d:%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

}
/**
 *  Send a registry request on the broadcast ip of all interfaces
 *
 * @param gSock         IN  GLib active/bound socket with broadcast enabled
 * @param ch_request    IN  Greetings message, anytext
 * @param ch_intfName   OUT Default Ethernet Interface Name
 * @param ch_ipAddress  OUT Default Ethernet IP Address
 * @return true on success,
 */
gboolean skn_udp_network_broadcast_all_interfaces(GSocket *gSock, PIPBroadcastArray paB) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    gchar *request = "urn:rpilocator - Rpi Where Are You?";
    gint vIndex = 0;
    gint i_socket = g_socket_get_fd(gSock);

    g_print("[REGISTRY] Socket Bound to %s\n", paB->ipAddrStr[paB->defaultIndex]);


    for (vIndex = 0; vIndex < paB->count; vIndex++) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_addr.s_addr = inet_addr(paB->broadAddrStr[vIndex]);
        remaddr.sin_port = htons(UDP_BROADCAST_PORT);

        if (sendto(i_socket, request, strlen(request), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            g_warning("SendTo() Timed out; Failure code=%d, etext=%s", errno, strerror(errno));
            break;
        }
        g_print("[REGISTRY] Query Broadcasted on %s:%s:%d\n", paB->ifNameStr[vIndex], paB->broadAddrStr[vIndex], UDP_BROADCAST_PORT);
    }

    return(TRUE);
}

gchar * skn_get_timestamp() {
    GDateTime   *stamp = g_date_time_new_now_local();
    gchar *response = g_date_time_format(stamp,"%F.%T");

    return(response);
}

gchar * skn_gio_condition_to_string(GIOCondition condition) {
    gchar *value = NULL;

    switch(condition) {
        case G_IO_IN:
            value = "There is data to read.";
            break;
        case G_IO_OUT:
            value = "Data can be written (without blocking).";
            break;
        case G_IO_PRI:
            value = "There is urgent data to read.";
            break;
        case G_IO_ERR:
            value = "Error condition.";
            break;
        case G_IO_HUP:
            value = "Hung up (the connection has been broken, usually for pipes and sockets).";
            break;
        case G_IO_NVAL:
            value = "Invalid request. The file descriptor is not open.";
            break;
        default:
            value = "Unknown GIOCondition!";
    }

    return (value);
}

/**
 *
 * Parse this response message
 *
 * Format: name=rpi_locator_service,ip=10.100.1.19,port=48028|
 *         name=lcd_display_service,ip=10.100.1.19,port=48029|
 *
 *  the vertical bar char '|' is the line separator, % and ; are also supported
*/
PPRegData udp_registry_response_parser(PRegData msg, gchar *response) {
    gboolean rc = FALSE;
    gboolean final_rc = FALSE;
    gchar ** lines = NULL;
    gchar *current_line = NULL;
    gchar ** entries = NULL;
    gchar *current_entry = NULL;
    gchar ** key_value = NULL;
    PRegData *msgs;
    PRegData preg = NULL;
    gint h_index = 0;
    gint v_index = 0;
    gint o_index = 0;
    gint a_count = 0;
    gint e_count = 0;

    if (g_utf8_strchr(response, -1, '|') == NULL) {  // must be a registry entry
        return (NULL);
    }

    lines = g_strsplit_set(response, "|;%", -1);  // get whole entries
    if ((NULL == lines) || (g_strv_length(lines) < 1)) {
        return(NULL);
    }

    a_count = g_strv_length(lines);
    msgs = g_new0(PRegData, a_count);
    for(o_index = 0; o_index < a_count; o_index += 1) {
        msgs[o_index] = g_new0(RegData, 1);
        memmove(msgs[o_index], msg, sizeof(RegData));
    }

    o_index = 0;
    current_line = lines[h_index];
    while ((NULL != current_line) && (h_index < a_count)) {   // do each entry
        if(g_utf8_strlen(current_line, -1) < 1) {
            current_line = lines[++h_index];
            continue;
        }

        v_index = 0;
        entries = g_strsplit_set(current_line, ",", -1);
        current_entry = entries[v_index];
        e_count = g_strv_length(entries);
        preg = msgs[o_index];
        rc = FALSE;

        while((NULL != current_entry) && (v_index < e_count)) {
            if(g_utf8_strlen(current_entry, -1) < 1) {
                current_entry = entries[++v_index];
                continue;
            }

            // get name, ip, port

            key_value = g_strsplit_set(current_entry, "=", -1);
            if((key_value != NULL) && (g_strv_length(key_value) > 0)) {
                if(g_strrstr(key_value[0], "a") != NULL) {
                    final_rc = rc = TRUE;
                    g_utf8_strncpy(preg->ch_name, key_value[1], SZ_RMTADDR_BUFF);
                }
                if(g_strrstr(key_value[0], "i") != NULL) {
                    final_rc = rc = TRUE;
                    g_utf8_strncpy(preg->ch_ip, key_value[1], SZ_RMTADDR_BUFF);
                }
                if(g_strrstr(key_value[0], "o") != NULL) {
                    final_rc = rc = TRUE;
                    g_utf8_strncpy(preg->ch_port, key_value[1], SZ_RMTADDR_BUFF);
                }
            }
            g_strfreev(key_value);
            current_entry = entries[++v_index];
        } // end entries

        if (rc && (g_utf8_strlen(preg->ch_ip, -1) > 6)) {   // only move if used and valid
            o_index += 1;
        }
        g_strfreev(entries);

        h_index +=1;
        current_line = lines[h_index];
    } // end lines
    g_strfreev(lines);

    if (final_rc) {
        while(o_index < a_count) {
            g_free(msgs[o_index]);
            msgs[o_index++] = NULL;
        }
        return(msgs);
    } else {
        while(o_index < a_count) {
            g_free(msgs[o_index]);
            msgs[o_index++] = NULL;
        }
        g_free(msgs);
        return(NULL);
    }
}

static gboolean cb_udp_comm_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl) {
    GError *error = NULL;
    gssize gss_receive = 0;
    gchar *stamp = skn_get_timestamp();
    gchar * rmtHost = NULL;
    GSocketAddress *gsRmtAddr = NULL;
    GInetAddress *gsAddr = NULL;
    gchar *converted = NULL;

    if ( NULL == pctrl) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("DisplayClient::cb_udp_comm_response_handler(error) Invalid Pointer");
        return ( G_SOURCE_REMOVE );
    }

    gss_receive = g_socket_receive_from(gSock, &gsRmtAddr, pctrl->ch_request, sizeof(pctrl->ch_request), NULL, &error);
    if (error != NULL) { // gss_receive = Number of bytes read, or 0 if the connection was closed by the peer, or -1 on error
        g_error("g_socket_receive() => %s", error->message);
        g_clear_error(&error);
    }
    if (gss_receive > 0 ) {
        pctrl->ch_request[gss_receive] = 0;
        if (G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) ) {
            gsAddr = g_inet_socket_address_get_address( G_INET_SOCKET_ADDRESS(gsRmtAddr) );
            if ( G_IS_INET_ADDRESS(gsAddr) ) {
                g_object_ref(gsAddr);
                rmtHost = g_resolver_lookup_by_address (pctrl->resolver, gsAddr, NULL, NULL);
                if (NULL == rmtHost) {
                    rmtHost = g_inet_address_to_string ( gsAddr);
                }
            }
        }

        /*
         * Convert to UTF8 */
        converted = g_convert (pctrl->ch_request, gss_receive, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        if (NULL != converted) {
            g_utf8_strncpy(pctrl->ch_request, converted, sizeof(pctrl->ch_request));
            g_free(converted);
        }

        g_snprintf(pctrl->ch_response, sizeof(pctrl->ch_response), "[%s]RESPONSE From=%s, Msg=%s", stamp, rmtHost, pctrl->ch_request);
        pctrl->gErrorCount = 0;  // reset send errors
    } else {
        g_snprintf(pctrl->ch_response, sizeof(pctrl->ch_response), "%s", "Error: Input not Usable!");
    }

    g_print("%s\n", pctrl->ch_response);
    g_free(stamp);

    if ( G_IS_INET_ADDRESS(gsAddr) )
        g_object_unref(gsAddr);

    if ( G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) )
        g_object_unref(gsRmtAddr);

    return (G_SOURCE_CONTINUE);
}

static gboolean cb_udp_comm_request_handler(PControlData pctrl) {
    GError *error = NULL;

    if ( NULL == pctrl) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("DisplayClient::cb_udp_comm_request_handler(error) Invalid Pointer");
        return ( G_SOURCE_REMOVE );
    }
	
	/*
	 * TODO
	 * 
	 * Add routine to change pctrl->ch_message to one of 
	 * many different status messages.  The <project>/src directory
	 * contains LibC routines for LoadAvg, CPU Util, Datetime, Cpu Temps, etc.
	 *
	 */

    g_socket_send_to (pctrl->gSock, pctrl->gsDSAddr, pctrl->ch_message, strlen(pctrl->ch_message), NULL, &error);
    if (error != NULL) {  // gss_send = Number of bytes written (which may be less than size ), or -1 on error
        g_error("g_socket_send_to() => %s", error->message);
        g_clear_error(&error);
        pctrl->gErrorCount += 1;
        if(pctrl->gErrorCount > 10) {
            g_message("DisplayClient::cb_udp_comm_request_handler(error) Display Service @ %s, Not Responding!", pctrl->ch_display_service_name);
            g_main_loop_quit(pctrl->loop);
            return ( G_SOURCE_REMOVE );
        } else {
            return (G_SOURCE_CONTINUE);
        }
    }

    g_print("%s\n", pctrl->ch_message); // Todo not needed

    return (G_SOURCE_CONTINUE);
}

static gboolean cb_udp_broadcast_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl) {
    GError *error = NULL;
    GSocketAddress *gsRmtAddr = NULL;
    GInetAddress *gsAddr = NULL;
    PRegData message = NULL;
    PRegData msg = NULL;
    PRegData *msgs;
    gchar * rmtHost = NULL;
    gssize gss_receive = 0;
    gchar *stamp = skn_get_timestamp();
    gint h_index = 0;
    gchar *converted = NULL;


    if ((condition & G_IO_HUP) || (condition & G_IO_ERR) || (condition & G_IO_NVAL)) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("DisplayService::cb_udp_broadcast_response_handler(error) Operational Error / Shutdown Signaled => %s\n", skn_gio_condition_to_string(condition));
        g_main_loop_quit(pctrl->loop);
        return ( G_SOURCE_REMOVE );
    }
    if (condition != G_IO_IN) {
        g_message("DisplayService::cb_udp_broadcast_response_handler(error) NOT G_IO_IN => %s\n", skn_gio_condition_to_string(condition));
        return (G_SOURCE_CONTINUE);
    }

    /*
     * Allocate a new queue message and read incoming request directly into it */
    message = g_new0(RegData,1);

    /*
     * If socket times out before reading data any operation will error with 'G_IO_ERROR_TIMED_OUT'.
     * Read Request Message and get Requestor IP Address or Name
    */
    gss_receive = g_socket_receive_from (gSock, &gsRmtAddr, message->pch_message, sizeof(message->pch_message), NULL, &error);
    if (error != NULL) { // gss_receive = Number of bytes read, or 0 if the connection was closed by the peer, or -1 on error
        g_error("g_socket_receive_from() => %s", error->message);
        g_clear_error(&error);
        g_free(message);
        g_free(stamp);
        return (G_SOURCE_CONTINUE);
    }
    if (gss_receive > 0 ) {
        if (G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) ) {
            gsAddr = g_inet_socket_address_get_address( G_INET_SOCKET_ADDRESS(gsRmtAddr) );
            if ( G_IS_INET_ADDRESS(gsAddr) ) {
                g_object_ref(gsAddr);
                rmtHost = g_resolver_lookup_by_address (pctrl->resolver, gsAddr, NULL, NULL);
                if (NULL == rmtHost) {
                    rmtHost = g_inet_address_to_string ( gsAddr);
                }
            }
        }

        /*
         * Convert to UTF8
         */
        converted = g_convert (message->pch_message, gss_receive, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        if (NULL != converted) {
            g_utf8_strncpy(message->pch_message, converted, sizeof(message->pch_message));
            g_free(converted);
        }

        g_utf8_strncpy(message->ch_timestamp, stamp, sizeof(message->ch_timestamp));
        g_utf8_strncpy(message->ch_from,      rmtHost, sizeof(message->ch_from));
        if ( (msgs = udp_registry_response_parser(message, message->pch_message)) != NULL ) {
            /*
             * Send it to be processed by a message handler */
            g_free(message);
            h_index = 0;
            msg = (PRegData)msgs[h_index];
            while ( msg != NULL ) {
                g_print("[%s]REGISTRY From=%s, Node=%s, IP=%s, Port=%s\n", msg->ch_timestamp, msg->ch_from, msg->ch_name, msg->ch_ip, msg->ch_port);
                if (g_strcmp0(pctrl->ch_display_service_name, msg->ch_name) == 0) {
                    pctrl->glRegistry = g_list_prepend(pctrl->glRegistry, msg);
                } else {
                    g_free(msg);
                }
                msg = (PRegData)msgs[++h_index];
            }
            g_free(msgs);
        } else {
            g_free(message);
        }
    }
    g_free(stamp);
    g_free(rmtHost);

    if ( G_IS_INET_ADDRESS(gsAddr) )
        g_object_unref(gsAddr);

    if ( G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) )
        g_object_unref(gsRmtAddr);

    return (G_SOURCE_CONTINUE);
}

gboolean udp_initialize_message_send(PControlData pctrl) {
    GError *error = NULL;
    GInetAddress *anyAddr = NULL;

    g_return_val_if_fail((NULL != pctrl), G_SOURCE_REMOVE);

    pctrl->gSock = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    if (error != NULL) {
        g_error("g_socket_new() => %s", error->message);
        g_clear_error(&error);
        return(FALSE);
    }

    anyAddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    pctrl->gsAddr = g_inet_socket_address_new(anyAddr, SKN_UDP_ANY_PORT);
        g_object_unref(anyAddr);

    g_socket_bind(pctrl->gSock, pctrl->gsAddr, TRUE, &error);
    if (error != NULL) {
        g_error("g_socket_bind() => %s", error->message);
        g_clear_error(&error);
        exit(FALSE);
    }

    pctrl->gCommSource = g_socket_create_source (pctrl->gSock, G_IO_IN, NULL);
        g_source_set_callback (pctrl->gCommSource, (GSourceFunc) cb_udp_comm_response_handler, pctrl, NULL); // its really a GSocketSourceFunc
        g_source_attach (pctrl->gCommSource, NULL);

    /*
     * Setup Timer to drive repeated Message to Display Service  */
    g_timeout_add ((pctrl->gMsgDelay * 1000), (GSourceFunc)cb_udp_comm_request_handler, pctrl);


    return (TRUE);
}

// (*GCompareFunc) 0 if found, -1 if not found
gint udp_registry_find_by_name(PRegData pr, gchar *pch_name) {
    return( g_strcmp0(pr->ch_name, pch_name) );
}

static gboolean cb_udp_registry_select_handler(PControlData pctrl) {
    GList *registry = NULL;
    PRegData preg = NULL;
    GInetAddress *anyAddr = NULL;

    g_return_val_if_fail((NULL != pctrl), G_SOURCE_REMOVE);

    /*
     * Find in Registry the selected service name */
    if (g_list_length(pctrl->glRegistry) < 1) {
        pctrl->gRegistryQueries++;

        if ((pctrl->gRegistryQueries % 15) == 0) {  // every 30 seconds redo query
            g_print("[REGISTRY] Looking for [%s] in Rpi Registry every 30 seconds.  StandBy...\n", pctrl->ch_display_service_name);
            skn_udp_network_broadcast_all_interfaces(pctrl->gBroadcastSocket, pctrl->paB);
        }

        return (G_SOURCE_CONTINUE);
    }

    if ( NULL == (registry = g_list_find_custom (pctrl->glRegistry, pctrl->ch_display_service_name, (GCompareFunc)udp_registry_find_by_name)) ) {
        return (G_SOURCE_CONTINUE);
    }

    preg = registry->data; // now extract ip and port
    /* Display Service Address */

    anyAddr = g_inet_address_new_from_string(preg->ch_ip); // g_object_unref(dIP) when done
    if (NULL == anyAddr) {
        g_error("g_inet_address_new_from_string() Failed => %s, %s", preg->ch_name, preg->ch_ip);
        return(G_SOURCE_CONTINUE);
    }
    pctrl->gsDSAddr = g_inet_socket_address_new(anyAddr,  g_ascii_strtoll(preg->ch_port, NULL, 10));
        g_object_unref(anyAddr);

    pctrl->gReady = udp_initialize_message_send(pctrl);

    if (pctrl->gReady) {
        g_print("[REGISTRY] FOUND %s on ip %s:%s, sending your message %d seconds.\n\n", preg->ch_name, preg->ch_ip, preg->ch_port, pctrl->gMsgDelay);
        return (G_SOURCE_REMOVE);
    } else {
        return (G_SOURCE_CONTINUE);
    }
}

static gboolean cb_unix_signal_handler(PUSignalData psig) {
    g_message("DisplayClient::cb_unix_signal_handler() %s Unix Signal Received => Shutdown Initiated!\n", psig->signalName);
    g_main_loop_quit(psig->loop);
    return ( G_SOURCE_REMOVE );
}

int main(int argc, char **argv) {

    ControlData cData;
    GSource * gBroadSource = NULL;
    GError *error = NULL;
    GSocket *gSock;
    GSocketAddress *gsAddr = NULL;
    GInetAddress *anyAddr = NULL;

    gchar * pch_display_service_name = NULL;
    gchar * pch_message = NULL;

    cData.gMsgDelay = MSG_DELAY_INTERVAL;
    cData.gErrorCount = 0;
    cData.gRegCount = 0;
    cData.gRegistryQueries = 0;
    cData.gReady = FALSE;

    cData.glRegistry = NULL;
    cData.sigTerm.signalName = "SIGTERM";
    cData.sigInt.signalName = "SIGINT";
    cData.sigHup.signalName = "SIGHUP";
    cData.gErrorCount = 0;
    cData.resolver = g_resolver_get_default();

    GOptionContext *gOptions = NULL;
    GOptionEntry pgmOptions[] = {
       {"display_service_name", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &pch_display_service_name, "DisplayService Name", "gtk_display_service"},
       {"display_message", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &pch_message, "Message to send to DisplayService", "single-quoted-string"},
       {"message_interval_delay", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &cData.gMsgDelay, "Send one message every Interval.", "[1 to 300] seconds, default no-repeat"},
       {NULL}
    };

    gOptions = g_option_context_new ("UDP message display client for IOT.");
    g_option_context_add_main_entries (gOptions, pgmOptions, NULL);
    g_option_context_parse (gOptions, &argc, &argv, &error);
    if (error != NULL) {
        g_error("g_option_context_parse() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }
    g_option_context_free(gOptions);

    if (NULL == pch_message) {
        g_utf8_strncpy(cData.ch_message, "GTK-+3.0 Rocks with GLIB-2.0 on any platform! (cldc)", sizeof(cData.ch_message));
    } else {
        g_utf8_strncpy(cData.ch_message, pch_message, sizeof(cData.ch_message));
        g_free(pch_message);
    }
    if (NULL == pch_display_service_name) {
        g_utf8_strncpy(cData.ch_display_service_name, "gtk_display_service", sizeof(cData.ch_display_service_name));
    } else {
        g_utf8_strncpy(cData.ch_display_service_name, pch_display_service_name, sizeof(cData.ch_display_service_name));
        g_free(pch_display_service_name);
    }
    if (0 == cData.gMsgDelay) {
        cData.gMsgDelay = MSG_DELAY_INTERVAL;
    }

    cData.paB = NULL;
    cData.paB = skn_util_get_default_interface_name_and_ipv4_address((char *)&cData.ch_intfName, (char *)&cData.ch_this_ip);
    if (NULL == cData.paB) {
        g_error("skn_skn_get_default_interface_name_and_ipv4_address() => Unable to discover network interface or non-available.");
        exit(EXIT_FAILURE);
    }

    cData.sigHup.loop = cData.sigTerm.loop = cData.sigInt.loop = cData.loop =  g_main_loop_new(NULL, FALSE);

    /*
     * Create broadcast UDP Socket for receiving messages */
    cData.gBroadcastSocket = gSock = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    if (error != NULL) {
        g_error("g_socket_new(broadcast) => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }
    g_socket_set_broadcast(gSock, TRUE);

    anyAddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    gsAddr  = g_inet_socket_address_new(anyAddr, SKN_UDP_ANY_PORT);
        g_object_unref(anyAddr);

    g_socket_bind(gSock, gsAddr, TRUE, &error);
    if (error != NULL) {
        g_error("g_socket_bind() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    gBroadSource = g_socket_create_source(gSock, G_IO_IN, NULL);
        g_source_ref(gBroadSource);
        g_source_set_callback (gBroadSource, (GSourceFunc) cb_udp_broadcast_response_handler, &cData, NULL); // its really a GSocketSourceFunc
        g_source_attach (gBroadSource, NULL);

    /*
     * Handle ctrl-break and kill signals cleanly */
    g_unix_signal_add (SIGINT, (GSourceFunc) cb_unix_signal_handler, &cData.sigInt); // SIGINT signal (Ctrl+C)
    g_unix_signal_add (SIGHUP, (GSourceFunc) cb_unix_signal_handler, &cData.sigHup);
    g_unix_signal_add (SIGTERM,(GSourceFunc) cb_unix_signal_handler, &cData.sigTerm);

    /*
     * Broadcast Registry Request: 10.100.1.255 */
    if ( skn_udp_network_broadcast_all_interfaces(gSock, cData.paB) ) {
        /*
         * Setup 2secTimer to find display_service before starting rest  */
        g_timeout_add (2000, (GSourceFunc)cb_udp_registry_select_handler, &cData);

        g_print("[REGISTRY] Looking for [%s] in Rpi Registry.  StandBy...\n", cData.ch_display_service_name);

        g_main_loop_run(cData.loop);
    }

    g_main_loop_unref(cData.loop);

    g_source_unref(gBroadSource);
    g_object_unref(gSock);
    g_object_unref(gsAddr);

    if (cData.gReady) {
        g_source_unref(cData.gCommSource);
        g_object_unref(cData.gSock);
        g_object_unref(cData.gsDSAddr);
        g_object_unref(cData.gsAddr);
    }

    g_free(cData.paB);
    g_list_free_full(cData.glRegistry, (GDestroyNotify)g_free);

    g_message("cmdDC: normal shutdown...");

    exit(EXIT_SUCCESS);
}
