/**
 * gssdpDC.c
 * IOT/RaspberryPi message display client
 *  gcc -v `pkg-config --cflags --libs glib-2.0 gio-2.0` -O3 -Wall -o gssdpDC gssdpDC.c
 *
 * Program Flow:
 *  1. Initialize    -> parse_options
 *  2. gssdp_browser -> loop ->
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
    #define PACKAGE_NAME "gssdpDC"
#endif
#ifndef PACKAGE_DESCRIPTION
    #define PACKAGE_DESCRIPTION "Send Message to Raspberry Pi's on the network."
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <libgssdp/gssdp.h>
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

typedef struct _gssdpRegistryData {
  gchar ch_timestamp[SZ_TIMESTAMP_BUFF];
  gchar ch_urn[SZ_RMTADDR_BUFF];
  gchar ch_location[SZ_RMTADDR_BUFF];
  gchar ch_status[SZ_PARAMS_BUFF];
} GSSDPRegData, *PGSSDPRegData;

typedef struct _controlData {
    GMainLoop *loop;
    USignalData sigTerm;
    USignalData sigInt;
    USignalData sigHup;
    GSource *gCommSource;
    GSocket *gSock;
    GSocketAddress *gsDSAddr;
    GSocketAddress *gsAddr;
    GSSDPClient *gssdp_rgroup_client;
    GSSDPResourceBrowser *resource_browser;
    GResolver *resolver;
    GList *glRegistry;
    PIPBroadcastArray paB;
    guint gRegistryQueries;
    guint gRegCount;
    guint gErrorCount;
    guint gMsgDelay;
    gboolean gReady;
    gboolean oneShot;
    gchar *pch_search;
    gchar ch_intfName[SZ_RMTADDR_BUFF];
    gchar ch_this_ip[SZ_RMTADDR_BUFF];
    gchar ch_display_service_name[SZ_RMTADDR_BUFF];
    gchar ch_message[SZ_MESSAGE_BUFF];
    gchar ch_request[SZ_MESSAGE_BUFF];
    gchar ch_response[SZ_RESPONSE_BUFF];
} ControlData, *PControlData;


gboolean skn_gssdp_browse(PControlData pctrl);
static void cb_gssdp_resource_available(GSSDPResourceBrowser *resource_browser, const char *usn, GList *locations, PControlData pctrl);

gchar  *  skn_get_timestamp();
gint      udp_registry_find_by_name(PGSSDPRegData pr, gchar *pch_name);
gchar  *  skn_gio_condition_to_string(GIOCondition condition);
gboolean  udp_initialize_message_send(PControlData pctrl);

static gboolean cb_unix_signal_handler(PUSignalData psig);
static gboolean cb_udp_comm_request_handler(PControlData pctrl);
static gboolean cb_udp_comm_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl);
static gboolean cb_udp_registry_select_handler(PControlData pctrl);

PIPBroadcastArray skn_util_get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);
gint skn_util_get_broadcast_ip_array(PIPBroadcastArray paB);
gint skn_util_get_default_interface_name(char *pchDefaultInterfaceName);
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
        g_warning("[GSSDP] InterfaceName and Address: unable to access information.");
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
        g_warning("[GSSDP] No Default Network Interfaces Found!.");
        paB->chDefaultIntfName[0] = 0;
    }
    rc = getifaddrs(&ifap);
    if (rc != 0) {
        g_warning("[GSSDP] No Network Interfaces Found at All ! %d:%d:%s", rc, errno, strerror(errno) );
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
   g_print("[GSSDP] Opening ProcFs for RouteInfo Failed: %d:%s, Alternate method will be attempted.\n", errno, strerror(errno));

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
        g_warning("[GSSDP] Alternate method to get RouteInfo Failed: %d:%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

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

    if (pctrl->oneShot) {
        g_main_loop_quit(pctrl->loop);
        return ( G_SOURCE_REMOVE );
    } else {
        return (G_SOURCE_CONTINUE);
    }

}

static gboolean cb_udp_comm_request_handler(PControlData pctrl) {
    GError *error = NULL;

    if ( NULL == pctrl) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("DisplayClient::cb_udp_comm_request_handler(error) Invalid Pointer");
        return ( G_SOURCE_REMOVE );
    }

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
gint udp_registry_find_by_name(PGSSDPRegData pr, gchar *pch_name) {
    gint result = -1;
    if (NULL != g_strrstr(pr->ch_urn, pch_name)) {
        result = 0;
    }
    return( result );
}

static gboolean cb_udp_registry_select_handler(PControlData pctrl) {
    GList *registry = NULL;
    PGSSDPRegData preg = NULL;
    GInetAddress *anyAddr = NULL;
    gchar **parts = NULL;
    RegData msg;

    g_return_val_if_fail((NULL != pctrl), G_SOURCE_REMOVE);

    /*
     * Find in Registry the selected service name */
    if (g_list_length(pctrl->glRegistry) < 1) {
        pctrl->gRegistryQueries++;

        if ((pctrl->gRegistryQueries % 15) == 0) {  // every 30 seconds redo query
            g_print("[GSSDP] Looking for [%s] in Rpi Registry every 30 seconds.  StandBy...\n", pctrl->ch_display_service_name);
            gssdp_resource_browser_set_active(pctrl->resource_browser, TRUE);
        }

        return (G_SOURCE_CONTINUE);
    }

    if ( NULL == (registry = g_list_find_custom (pctrl->glRegistry, pctrl->ch_display_service_name, (GCompareFunc)udp_registry_find_by_name)) ) {
        g_list_free_full(pctrl->glRegistry, (GDestroyNotify)g_free);
		pctrl->glRegistry = NULL;
        return (G_SOURCE_CONTINUE);
    }

    preg = registry->data; // now extract ip and port
    /* Display Service Address */

    parts = g_strsplit (preg->ch_location, ":",-1);
    g_utf8_strncpy(msg.ch_name, pctrl->ch_display_service_name, sizeof(msg.ch_name));
    g_utf8_strncpy(msg.ch_ip, &parts[1][2], sizeof(msg.ch_ip));
    g_utf8_strncpy(msg.ch_port, parts[2], sizeof(msg.ch_port));
    g_strfreev(parts);

    anyAddr = g_inet_address_new_from_string(msg.ch_ip); // g_object_unref(dIP) when done
    if (NULL == anyAddr) {
        g_error("g_inet_address_new_from_string() Failed => %s, %s", preg->ch_location, preg->ch_location);  // "udp://%s:%d"
        return(G_SOURCE_CONTINUE);
    }
    pctrl->gsDSAddr = g_inet_socket_address_new(anyAddr,  g_ascii_strtoll(msg.ch_port, NULL, 10));
        g_object_unref(anyAddr);

    pctrl->gReady = udp_initialize_message_send(pctrl);

    if (pctrl->gReady) {
        g_print("[GSSDP] FOUND %s on %s, sending your message in %d seconds.\n\n", preg->ch_urn, preg->ch_location, pctrl->gMsgDelay);
        gssdp_resource_browser_set_active(pctrl->resource_browser, FALSE);
        return (G_SOURCE_REMOVE);
    } else {
        return (G_SOURCE_CONTINUE);
    }
}

static void cb_gssdp_resource_available(GSSDPResourceBrowser *resource_browser, const char *usn, GList *locations, PControlData pctrl) {
    PGSSDPRegData msg = NULL;
    gchar *stamp = skn_get_timestamp();
    gchar *converted = NULL;

    g_warn_if_fail(NULL != pctrl);

    msg = g_new0(GSSDPRegData, 1);

    g_utf8_strncpy(msg->ch_timestamp, stamp, sizeof(msg->ch_timestamp));
    g_free(stamp);

    /* Convert to UTF8 */
    converted = g_convert (usn, g_utf8_strlen(usn, -1), "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    if (NULL != converted) {
        g_utf8_strncpy(msg->ch_urn, converted, sizeof(msg->ch_urn));
        g_free(converted);
    } else {
        g_utf8_strncpy(msg->ch_urn, usn, sizeof(msg->ch_urn));
    }

    converted = g_convert (locations->data, g_utf8_strlen(locations->data, -1), "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    if (NULL != converted) {
        g_utf8_strncpy(msg->ch_location, converted, sizeof(msg->ch_location));
        g_free(converted);
    } else {
        g_utf8_strncpy(msg->ch_location, locations->data, sizeof(msg->ch_location));
    }

    g_utf8_strncpy(msg->ch_status, "Available", sizeof(msg->ch_status));

    g_print("[%s]GSSDP %s, %s, %s\n", msg->ch_timestamp, msg->ch_urn, msg->ch_location, msg->ch_status);

    pctrl->glRegistry = g_list_prepend(pctrl->glRegistry, msg);
}

/**
 * Setup a monitor for the registry page
*/
gboolean skn_gssdp_browse(PControlData pctrl) {
    GError *error = NULL;

    pctrl->gssdp_rgroup_client = gssdp_client_new(NULL, NULL, &error);
    if (error) {
            g_printerr ("Error creating the GSSDP gssdp_client for resource_browser: %s\n", error->message);
            g_error_free (error);
            return(FALSE);
    }

    pctrl->resource_browser = gssdp_resource_browser_new (pctrl->gssdp_rgroup_client, pctrl->pch_search); // GSSDP_ALL_RESOURCES);
    g_signal_connect (pctrl->resource_browser, "resource-available", G_CALLBACK (cb_gssdp_resource_available), pctrl);

    gssdp_resource_browser_set_active(pctrl->resource_browser, TRUE);

    return (TRUE);
}

static gboolean cb_unix_signal_handler(PUSignalData psig) {
    g_message("DisplayClient::cb_unix_signal_handler() %s Unix Signal Received => Shutdown Initiated!\n", psig->signalName);
    g_main_loop_quit(psig->loop);
    return ( G_SOURCE_REMOVE );
}

int main(int argc, char **argv) {

    ControlData cData;
    GError *error = NULL;
    gchar * pch_display_service_name = NULL;
    gchar * pch_message = NULL;

    GOptionContext *gOptions = NULL;
    GOptionEntry pgmOptions[] = {
       {"display_service_name", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &pch_display_service_name, "DisplayService Name", "GtkDisplayService"},
       {"display_message", 'm', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &pch_message, "Message to send to DisplayService", "single-quoted-string"},
       {"message_interval_delay", 'n', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &cData.gMsgDelay, "Send one message every Interval.", "[1 to 300] seconds, default no-repeat"},
       {"search-key", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &cData.pch_search, "GSSDP Search Key.", "[urn:rpilocator | ssdp:all]"},
       {NULL}
    };

    cData.gMsgDelay = 0;
    cData.gErrorCount = 0;
    cData.gRegCount = 0;
    cData.gRegistryQueries = 0;
    cData.gReady = FALSE;
    cData.oneShot = FALSE;

    cData.pch_search = NULL;
    cData.glRegistry = NULL;
    cData.sigTerm.signalName = "SIGTERM";
    cData.sigInt.signalName = "SIGINT";
    cData.sigHup.signalName = "SIGHUP";
    cData.gErrorCount = 0;
    cData.resolver = g_resolver_get_default();

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
        g_utf8_strncpy(cData.ch_message, "GTK-+3.0 Rocks with GLIB-2.0 on any platform! (gssdpDC)", sizeof(cData.ch_message));
    } else {
        g_utf8_strncpy(cData.ch_message, pch_message, sizeof(cData.ch_message));
        g_free(pch_message);
    }
    if (NULL == pch_display_service_name) {
        g_utf8_strncpy(cData.ch_display_service_name, "GtkDisplayService", sizeof(cData.ch_display_service_name));
    } else {
        g_utf8_strncpy(cData.ch_display_service_name, pch_display_service_name, sizeof(cData.ch_display_service_name));
        g_free(pch_display_service_name);
    }
    if (0 == cData.gMsgDelay) {
        cData.gMsgDelay = MSG_DELAY_INTERVAL;
        cData.oneShot = TRUE;   // if no delay is given, send message once and quit.
    }
    if (NULL == cData.pch_search) {
        cData.pch_search = "urn:rpilocator";
    }


    cData.paB = NULL;
    cData.paB = skn_util_get_default_interface_name_and_ipv4_address((char *)&cData.ch_intfName, (char *)&cData.ch_this_ip);
    if (NULL == cData.paB) {
        g_error("skn_skn_get_default_interface_name_and_ipv4_address() => Unable to discover network interface or non-available.");
        exit(EXIT_FAILURE);
    }

    cData.sigHup.loop = cData.sigTerm.loop = cData.sigInt.loop = cData.loop =  g_main_loop_new(NULL, FALSE);

    /*
     * Handle ctrl-break and kill signals cleanly */
    g_unix_signal_add (SIGINT, (GSourceFunc) cb_unix_signal_handler, &cData.sigInt); // SIGINT signal (Ctrl+C)
    g_unix_signal_add (SIGHUP, (GSourceFunc) cb_unix_signal_handler, &cData.sigHup);
    g_unix_signal_add (SIGTERM,(GSourceFunc) cb_unix_signal_handler, &cData.sigTerm);

    if ( skn_gssdp_browse(&cData) ) {
        /*
         * Setup 2secTimer to find display_service before starting rest  */
        g_timeout_add (2000, (GSourceFunc)cb_udp_registry_select_handler, &cData);

        g_print("[GSSDP] Looking for [%s] in Rpi Registry.  StandBy...\n", cData.ch_display_service_name);

        g_main_loop_run(cData.loop);

        gssdp_resource_browser_set_active(cData.resource_browser, FALSE);
    }

    g_main_loop_unref(cData.loop);

    if (cData.gReady) {
        g_source_unref(cData.gCommSource);
        g_object_unref(cData.gSock);
        g_object_unref(cData.gsDSAddr);
        g_object_unref(cData.gsAddr);
    }
    g_object_unref (cData.resource_browser);
    g_object_unref (cData.gssdp_rgroup_client);

    g_free(cData.paB);
    g_list_free_full(cData.glRegistry, (GDestroyNotify)g_free);

    g_message("gssdpDC: normal shutdown...");

    exit(EXIT_SUCCESS);
}
