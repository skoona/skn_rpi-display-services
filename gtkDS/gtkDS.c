/**
 * gtkDS.c
 * IOT/RaspberryPi message display service
 *  gcc `pkg-config --cflags --libs gtk+-3.0 gssdp-1.0` -O3 -Wall -o gtkDS gtkDS.c
 *
 * Program Flow:
 *  1. Initialize    -> parse_options
 *  2. upd_socket    -> loop -> receive_message_and_push_queue
 *  2. upd_broad     -> loop -> receive_broadcast_message_and_push_queue
 *  2. gssdp_rgroup  -> loop -> receive_gssdp_notification_and_response_directly
 *  2. gssdp_rbrowse -> loop -> receive_gssdp_message_and_push_queue
 *  3. udp_timeout   -> loop -> trigger_refresh_on_gssdp_and_registry
 *  3. AsyncQueue    -> loop -> update_Message_GtkTreeView_head -> if max_count then delete_GtkTreeView_tail
 *  3. AsyncQueue    -> loop -> update_Registry_GtkTreeView_head -> if max_count then delete_GtkTreeView_tail
 *  3. AsyncQueue    -> loop -> update_GSSDP_GtkTreeView_head -> if max_count then delete_GtkTreeView_tail
 *  4. unix_signal   -> loop -> close_loop
 *  4. GtkStack      -> loop -> service_message_registry_and_gssdp_scrolling_pages
 *  4. GtkWindow     -> loop -> destroy_signal_callback -> close_loop
 *  5. Cleanup       -> exit
*/

/*
 * Program Standards passed from compiler
 */
#ifndef PACKAGE_VERSION
    #define PACKAGE_VERSION "0.9.0"
#endif
#ifndef PACKAGE_NAME
    #define PACKAGE_NAME "gtkDS"
#endif
#ifndef PACKAGE_DESCRIPTION
    #define PACKAGE_DESCRIPTION "Display Messages from other Raspberry Pi's on the network."
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgssdp/gssdp.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gio/gnetworking.h>
#include <ifaddrs.h>

#ifndef G_OPTION_FLAG_NONE
    #define G_OPTION_FLAG_NONE 0
#endif


#define SZ_TIMESTAMP_BUFF 32
#define SZ_PARAMS_BUFF    64
#define SZ_RMTADDR_BUFF  256
#define SZ_MESSAGE_BUFF  512
#define SZ_RESPONSE_BUFF 256

#define UDP_COMM_PORT      48029
#define UDP_BROADCAST_PORT 48028
#define UDP_REGULAR_PORT   48027
#define MS_TEN_MINUTES    600000

#define SKN_SMALL_DISPLAY_MODE_ON  1
#define SKN_SMALL_DISPLAY_MODE_OFF 0

#define MAX_MESSAGES_VIEWABLE 64

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

typedef struct _messageData {
    gchar ch_timestamp[SZ_TIMESTAMP_BUFF];
    gchar ch_remoteAddress[SZ_RMTADDR_BUFF];
    gchar ch_message[SZ_MESSAGE_BUFF];
} MsgData, *PMsgData;

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
  gchar ch_message[SZ_MESSAGE_BUFF];
} RegData, *PRegData, **PPRegData;

typedef struct _gssdpRegistryData {
  gchar ch_timestamp[SZ_TIMESTAMP_BUFF];
  gchar ch_urn[SZ_RMTADDR_BUFF];
  gchar ch_location[SZ_RMTADDR_BUFF];
  gchar ch_status[SZ_PARAMS_BUFF];
} GSSDPRegData, *PGSSDPRegData;

typedef struct _controlData {
    guint gMsgSourceId;
    guint gRegSourceId;
    guint gCommSourceId;
    guint gBroadSourceId;
    guint gGSSDPRegSourceId;
    guint gMsgCount;
    guint gGSSDPRegCount;
    guint gRegCount;
    gint  gStatusContextId;
    guint gUDPPort;
    guint gSmall;
    GResolver *resolver;
    GSSDPClient *gssdp_rgroup_client;
    GSSDPResourceGroup *resource_group;
    GSSDPResourceBrowser *resource_browser;
    GMainLoop *loop;
    GSocket *gbSock;
    GAsyncQueue *queueMessage;
    GAsyncQueue *queueRegistry;
    GAsyncQueue *queueGSSDPRegistry;
    GtkWidget *statusBar;
    GtkWidget *pg1Target;
    GtkWidget *pg2Target;
    GtkWidget *pg3Target;
    PIPBroadcastArray paB;
    gchar ch_intfName[SZ_RMTADDR_BUFF];
    gchar ch_this_ip[SZ_RMTADDR_BUFF];
    gchar ch_status_message[SZ_MESSAGE_BUFF];
    gchar *pch_search;
    gchar *pch_service_name;
} ControlData, *PControlData;

enum _messages {
  MSGS_COLUMN_TIMESTAMP,
  COLUMN_NODE,
  COLUMN_MESSAGES,
  MSG_NUM_COLUMNS
} EMessages;

enum _registry {
  UDP_COLUMN_TIMESTAMP,
  COLUMN_FROM,
  COLUMN_NAME,
  COLUMN_IP,
  COLUMN_PORT,
  REG_NUM_COLUMNS
} EUDPRegistry;

enum _gssdp_registry {
  GSSDP_COLUMN_TIMESTAMP,
  COLUMN_URN,
  COLUMN_LOCATION,
  COLUMN_STATUS,
  GSSDP_NUM_COLUMNS
} EGSSDPRegistry;

gchar * skn_get_timestamp();
gchar * skn_gio_condition_to_string(GIOCondition condition);
gchar * skn_strip(gchar * alpha);

PIPBroadcastArray skn_get_default_interface_name_and_ipv4_address(char * intf, char * ipv4);
gint skn_get_broadcast_ip_array(PIPBroadcastArray paB);
gint skn_get_default_interface_name(char *pchDefaultInterfaceName);
gboolean skn_udp_network_broadcast_all_interfaces(GSocket *gSock, PIPBroadcastArray pab);


static gboolean cb_unix_signal_handler(PUSignalData psig);
static gboolean cb_registry_refresh(PControlData pctrl);
static gboolean cb_registry_request_handler(PRegData msg, PControlData pctrl);
static gboolean cb_message_request_handler(PMsgData msg, PControlData pctrl);
static gboolean cb_udp_comm_request_handler(GSocket *socket, GIOCondition condition, PControlData pctrl);
static gboolean cb_udp_broadcast_response_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl);
PPRegData udp_registry_response_parser(PRegData msg, gchar *response);


GtkWidget * ui_page_layout(GtkWidget *parent, PControlData pctrl);
GtkWidget * ui_message_page_new(GtkWidget *parent, guint gSmall);
GtkWidget * ui_registry_page_new(GtkWidget *parent, guint gSmall);
GtkWidget * ui_gssdp_registry_page_new(GtkWidget *parent, guint gSmall);

gboolean ui_add_message_entry(GtkWidget *treeview, PMsgData msg, gboolean limiter);
gboolean ui_add_registry_entry(GtkWidget *treeview, PRegData msg, gboolean limiter);
gboolean ui_add_gssdp_registry_entry(GtkWidget *treeview, PGSSDPRegData msg, gboolean limiter);
guint ui_update_status_bar(PControlData pctrl);

gboolean gssdp_publish(PControlData pctrl, guint udp_port );
gboolean gssdp_browse(PControlData pctrl);
static void cb_gssdp_resource_available(GSSDPResourceBrowser *resource_browser, const char *usn, GList *locations, PControlData pctrl);
static void cb_gssdp_resource_unavailable(GSSDPResourceBrowser *resource_browser, const char *usn, PControlData pctrl);



/**
 * MessageQueueSource:
 *
 * Ref: https://developer.gnome.org/gnome-devel-demos/stable/custom-gsource.c.html.en
 *
 * This is a #GSource which wraps a #GAsyncQueue and is dispatched whenever a
 * message can be pulled off the queue. Messages can be enqueued from any
 * thread.
 *
 * The callbacks dispatched by a #MessageQueueSource have type
 * #MessageQueueSourceFunc.
 *
 * #MessageQueueSource supports adding a #GCancellable child source which will
 * additionally dispatch if a provided #GCancellable is cancelled.
 */
typedef struct {
  GSource         parent;
  GAsyncQueue    *queue;  /* owned */
  GDestroyNotify  destroy_message;
} MessageQueueSource;

/**
 * MessageQueueSourceFunc:
 * @message: (transfer full) (nullable): message pulled off the queue
 * @user_data: user data provided to g_source_set_callback()
 *
 * Callback function type for #MessageQueueSource.
 */
typedef gboolean (*MessageQueueSourceFunc) (gpointer message, gpointer user_data);

static gboolean message_queue_source_prepare (GSource *source, gint *timeout_) {
  MessageQueueSource *message_queue_source = (MessageQueueSource *) source;
  return (g_async_queue_length (message_queue_source->queue) > 0);
}

static gboolean  message_queue_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data) {
  MessageQueueSource *message_queue_source = (MessageQueueSource *) source;
  gpointer message;
  MessageQueueSourceFunc func = (MessageQueueSourceFunc) callback;

  /* Pop a message off the queue. */
  message = g_async_queue_try_pop (message_queue_source->queue);

  /* If there was no message, bail. */
  if (message == NULL)
    {
      /* Keep the source around to handle the next message. */
      return TRUE;
    }

  /* @func may be %NULL if no callback was specified.
   * If so, drop the message. */
  if (func == NULL)
    {
      if (message_queue_source->destroy_message != NULL)
        {
          message_queue_source->destroy_message (message);
        }

      /* Keep the source around to consume the next message. */
      return TRUE;
    }

  return (func(message, user_data));
}

static void message_queue_source_finalize (GSource *source) {
  MessageQueueSource *message_queue_source = (MessageQueueSource *) source;
  g_async_queue_unref (message_queue_source->queue);
}

static gboolean message_queue_source_closure_callback (gpointer message, gpointer user_data) {
  GClosure *closure = user_data;
  GValue param_value = G_VALUE_INIT;
  GValue result_value = G_VALUE_INIT;
  gboolean retval;

  g_warn_if_reached();

  /* The invoked function is responsible for freeing @message. */
  g_value_init (&result_value, G_TYPE_BOOLEAN);
  g_value_init (&param_value, G_TYPE_POINTER);
  g_value_set_pointer (&param_value, message);

  g_closure_invoke (closure, &result_value, 1, &param_value, NULL);
  retval = g_value_get_boolean (&result_value);

  g_value_unset (&param_value);
  g_value_unset (&result_value);

  return (retval);
}

static GSourceFuncs message_queue_source_funcs =
  {
    message_queue_source_prepare,
    NULL,  /* check */
    message_queue_source_dispatch,
    message_queue_source_finalize,
    (GSourceFunc) message_queue_source_closure_callback,
    NULL,
  };

/**
 * message_queue_source_new:
 * @queue: the queue to check
 * @destroy_message: (nullable): function to free a message, or %NULL
 * @cancellable: (nullable): a #GCancellable, or %NULL
 *
 * Create a new #MessageQueueSource, a type of #GSource which dispatches for
 * each message queued to it.
 *
 * If a callback function of type #MessageQueueSourceFunc is connected to the
 * returned #GSource using g_source_set_callback(), it will be invoked for each
 * message, with the message passed as its first argument. It is responsible for
 * freeing the message. If no callback is set, messages are automatically freed
 * as they are queued.
 *
 * Returns: (transfer full): a new #MessageQueueSource
 */
GSource * message_queue_source_new (GAsyncQueue *queue, GDestroyNotify destroy_message, GCancellable *cancellable) {
  GSource *source;  /* alias of @message_queue_source */
  MessageQueueSource *message_queue_source;  /* alias of @source */

  g_return_val_if_fail (queue != NULL, NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);

  source = g_source_new (&message_queue_source_funcs, sizeof (MessageQueueSource));
  message_queue_source = (MessageQueueSource *) source;

  /* The caller can overwrite this name with something more useful later. */
  g_source_set_name (source, "MessageQueueSource");

  message_queue_source->queue = g_async_queue_ref (queue);
  message_queue_source->destroy_message = destroy_message;

  /* Add a cancellable source. */
  if (cancellable != NULL)
    {
      GSource *cancellable_source;

      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_set_dummy_callback (cancellable_source);
      g_source_add_child_source (source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  return (source);
}


PIPBroadcastArray skn_get_default_interface_name_and_ipv4_address(gchar * intf, gchar * ipv4) {
    PIPBroadcastArray paB = g_new0(IPBroadcastArray, 1);

    if (skn_get_broadcast_ip_array(paB) != PLATFORM_ERROR) {
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
gint skn_get_broadcast_ip_array(PIPBroadcastArray paB) {
    struct ifaddrs * ifap;
    struct ifaddrs * p;
    gint rc = 0;

    memset(paB, 0, sizeof(IPBroadcastArray));
    paB->count = 0;
    paB->defaultIndex = 0;
    strcpy(paB->cbName, "IPBroadcastArray");

    rc = skn_get_default_interface_name(paB->chDefaultIntfName);
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
gint skn_get_default_interface_name(char *pchDefaultInterfaceName) {
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
   g_warning("[REGISTRY] Opening ProcFs for RouteInfo Failed: %d:%s, Alternate method will be attempted.", errno, strerror(errno));

    f_route = popen("route -n get 0.0.0.0", "r"); // for linux 'route -n -A inet', with interface at line_word[7]
    if (f_route != NULL) {
        while (fgets(line, SZ_INFO_BUFF - 1, f_route)) {
            dRoute = strtok(line, ":");
            iName = strtok(NULL, "\n");
            if (strcmp(skn_strip(dRoute), "interface") == 0) {
                strncpy(pchDefaultInterfaceName, skn_strip(iName), (SZ_INFO_BUFF - 1));
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

guint ui_update_status_bar(PControlData pctrl) {
    gchar ch_status_message[SZ_MESSAGE_BUFF];

    if (pctrl->gSmall != SKN_SMALL_DISPLAY_MODE_OFF) {
        return (0);
    }

    g_snprintf(ch_status_message, sizeof(ch_status_message), "Messages=%d, GSSDP entries=%d, RpiLocator entries=%d", pctrl->gMsgCount, pctrl->gGSSDPRegCount, pctrl->gRegCount);
    gtk_statusbar_pop(GTK_STATUSBAR(pctrl->statusBar), pctrl->gStatusContextId);

    return (gtk_statusbar_push(GTK_STATUSBAR(pctrl->statusBar), pctrl->gStatusContextId, ch_status_message));
}

static gboolean cb_unix_signal_handler(PUSignalData psig) {
    g_message("gtkDS::cb_unix_signal_handler() %s Unix Signal Received => Shutdown Initiated!\n", psig->signalName);
    g_main_loop_quit(psig->loop);
    return ( G_SOURCE_REMOVE );
}

static gboolean cb_registry_refresh(PControlData pctrl) {
    g_return_val_if_fail((NULL != pctrl), G_SOURCE_CONTINUE);

    skn_udp_network_broadcast_all_interfaces(pctrl->gbSock, pctrl->paB);
//    gssdp_resource_browser_rescan(pctrl->resource_browser);  require 2.4+
    gssdp_resource_browser_set_active(pctrl->resource_browser, TRUE);
    return ( G_SOURCE_CONTINUE );
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
    gchar response[SZ_RESPONSE_BUFF];
    gint h_index = 0;
    gchar *converted = NULL;

    if ((condition & G_IO_HUP) || (condition & G_IO_ERR) || (condition & G_IO_NVAL)) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("gtkDS::cb_udp_broadcast_response_handler(error) Operational Error / Shutdown Signaled => %s\n", skn_gio_condition_to_string(condition));
        g_main_loop_quit(pctrl->loop);
        return ( G_SOURCE_REMOVE );
    }
    if (condition != G_IO_IN) {
        g_message("gtkDS::cb_udp_broadcast_response_handler(error) NOT G_IO_IN => %s\n", skn_gio_condition_to_string(condition));
        return (G_SOURCE_CONTINUE);
    }

    /*
     * Allocate a new queue message and read incoming request directly into it */
    message = g_new0(RegData,1);

    /*
     * If socket times out before reading data any operation will error with 'G_IO_ERROR_TIMED_OUT'.
     * Read Request Message and get Requestor IP Address or Name
    */
    gss_receive = g_socket_receive_from (gSock, &gsRmtAddr, message->ch_message, sizeof(message->ch_message), NULL, &error);
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
        converted = g_convert (message->ch_message, gss_receive, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        if (NULL != converted) {
            g_utf8_strncpy(message->ch_message, converted, sizeof(message->ch_message));
            g_free(converted);
        }

        g_utf8_strncpy(message->ch_timestamp, stamp, sizeof(message->ch_timestamp));
        g_utf8_strncpy(message->ch_from,      rmtHost, sizeof(message->ch_from));
        if ( (msgs = udp_registry_response_parser(message, message->ch_message)) != NULL ) {
            /*
             * Send it to be processed by a message handler */
            g_free(message);
            h_index = 0;
            msg = (PRegData)msgs[h_index];
            while ( msg != NULL ) {
                g_async_queue_push (pctrl->queueRegistry, msg);
                msg = (PRegData)msgs[++h_index];
            }
            g_free(msgs);
        } else {
            g_free(message);

            /* Format: name=rpi_locator_service,ip=10.100.1.19,port=48028|
             *         name=gtk_display_service,ip=10.100.1.19,port=48029|
             */

            g_snprintf(response, sizeof(response),
                       "name=rpi_locator_service,ip=%s,port=48028|name=%s,ip=%s,port=%d|",
                       pctrl->ch_this_ip, pctrl->pch_service_name, pctrl->ch_this_ip, pctrl->gUDPPort);
            /*
             * Send Registry Response to caller */

            g_print("[REGISTRY] Responding to Query: %s\n", response);

            g_socket_send_to (gSock, gsRmtAddr, response, strlen(response), NULL, &error);
            if (error != NULL) {  // gss_send = Number of bytes written (which may be less than size ), or -1 on error
                g_error("g_socket_send_to() => %s", error->message);
                g_free(message);
                g_clear_error(&error);
            }
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

static gboolean cb_udp_comm_request_handler(GSocket *gSock, GIOCondition condition, PControlData pctrl) {
    GError *error = NULL;
    GSocketAddress *gsRmtAddr = NULL;
    GInetAddress *gsAddr = NULL;
    PMsgData message = NULL;
    gchar * rmtHost = NULL;
    gssize gss_receive = 0;
    gchar *stamp = skn_get_timestamp();
    gchar response[SZ_RESPONSE_BUFF];

    if ((condition & G_IO_HUP) || (condition & G_IO_ERR) || (condition & G_IO_NVAL)) {  /* SHUTDOWN THE MAIN LOOP */
        g_message("gtkDS::cb_udp_comm_request_handler(error) Operational Error / Shutdown Signaled => %s\n", skn_gio_condition_to_string(condition));
        g_main_loop_quit(pctrl->loop);
        return ( G_SOURCE_REMOVE );
    }
    if (condition != G_IO_IN) {
        g_message("gtkDS::cb_udp_comm_request_handler(error) NOT G_IO_IN => %s\n", skn_gio_condition_to_string(condition));
        return (G_SOURCE_CONTINUE);
    }

    /*
     * Allocate a new queue message and read incoming request directly into it */
    message = g_new0(MsgData,1);
    g_utf8_strncpy(message->ch_timestamp, stamp, sizeof(message->ch_timestamp));
    g_free(stamp);

    /*
     * If socket times out before reading data any operation will error with 'G_IO_ERROR_TIMED_OUT'.
     * Read Request Message and get Requestor IP Address or Name
    */
    gss_receive = g_socket_receive_from (gSock, &gsRmtAddr, message->ch_message, sizeof(message->ch_message), NULL, &error);
    if (error != NULL) { // gss_receive = Number of bytes read, or 0 if the connection was closed by the peer, or -1 on error
        g_error("g_socket_receive_from() => %s", error->message);
        g_clear_error(&error);
        g_free(message);
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
        g_snprintf(message->ch_remoteAddress, sizeof(message->ch_remoteAddress), "%s", rmtHost);
        g_free(rmtHost);
        g_snprintf(response, sizeof(response), "%d %s", 202, "Accepted");
    } else {
        g_snprintf(message->ch_message, sizeof(message->ch_message), "%s", "Error: Input not Usable");
        g_snprintf(response, sizeof(response), "%d %s", 406, "Not Acceptable");
    }

    /*
     * Send Response to caller */
    g_socket_send_to (gSock, gsRmtAddr, response, strlen(response), NULL, &error);
    if (error != NULL) {  // gss_send = Number of bytes written (which may be less than size ), or -1 on error
        g_error("g_socket_send_to() => %s", error->message);
        g_free(message);
        g_clear_error(&error);

        if ( G_IS_INET_ADDRESS(gsAddr) )
            g_object_unref(gsAddr);

        if ( G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) )
            g_object_unref(gsRmtAddr);

        return (G_SOURCE_CONTINUE);
    }

    /*
     * Send it to be processed by a message handler */
    stamp = g_convert (message->ch_message, gss_receive, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    if (NULL != stamp) {
        g_utf8_strncpy(message->ch_message, stamp, sizeof(message->ch_message));
        g_free(stamp);
    } else {
        g_error("g_convert(udp_comm_request) => %s", error->message);
        g_clear_error(&error);
    }
    g_async_queue_push (pctrl->queueMessage, message);

    if ( G_IS_INET_ADDRESS(gsAddr) )
        g_object_unref(gsAddr);

    if ( G_IS_INET_SOCKET_ADDRESS(gsRmtAddr) )
        g_object_unref(gsRmtAddr);

    return (G_SOURCE_CONTINUE);
}

//MessageQueueSourceFunc
static gboolean cb_message_request_handler(PMsgData msg, PControlData pctrl) {

    g_return_val_if_fail((NULL != msg) && (NULL != pctrl), G_SOURCE_CONTINUE);

    pctrl->gMsgCount += 1;

    ui_add_message_entry( pctrl->pg1Target, msg, (pctrl->gMsgCount >= MAX_MESSAGES_VIEWABLE));

    ui_update_status_bar(pctrl);

    g_free(msg);

    return(G_SOURCE_CONTINUE);
}

//MessageQueueSourceFunc
static gboolean cb_registry_request_handler(PRegData msg, PControlData pctrl) {

    g_return_val_if_fail((NULL != msg) && (NULL != pctrl), G_SOURCE_CONTINUE);

    pctrl->gRegCount += 1;

    ui_add_registry_entry( pctrl->pg2Target, msg, (pctrl->gRegCount >= (MAX_MESSAGES_VIEWABLE * 2)));

    ui_update_status_bar(pctrl);

    g_free(msg);

    return(G_SOURCE_CONTINUE);
}

//MessageQueueSourceFunc
static gboolean cb_gssdp_registry_request_handler(PGSSDPRegData msg, PControlData pctrl) {

    g_return_val_if_fail((NULL != msg) && (NULL != pctrl), G_SOURCE_CONTINUE);

    pctrl->gGSSDPRegCount += 1;

    ui_add_gssdp_registry_entry( pctrl->pg3Target, msg, (pctrl->gGSSDPRegCount >= (MAX_MESSAGES_VIEWABLE * 2)));

    ui_update_status_bar(pctrl);

    g_free(msg);

    return(G_SOURCE_CONTINUE);
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
 * Remove Trailing and Leading Blanks
 * - caution: pointers from argv are readonly and segfault on 'alpha[end] = 0'
 */
gchar * skn_strip(gchar * alpha) {
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

/**
 *
 * Parse this response message
 *
 * Format: name=rpi_locator_service,ip=10.100.1.19,port=48028|
 *         name=lcd_display_service,ip=10.100.1.19,port=48029|
 *
 *  the vertical bar char '|' is the line separator, % and ; are also supported
 *
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

GtkWidget * ui_message_page_new(GtkWidget *parent, guint gSmall) {
    GtkWidget *scrolled = NULL;
    GtkTreeView *treeview = NULL;
    GtkListStore *store = NULL;
    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *column = NULL;

    if (gSmall == SKN_SMALL_DISPLAY_MODE_OFF) {
        gtk_box_pack_start (GTK_BOX (parent), gtk_label_new ("Device Messages from the local Internet of Things (IOT).") , FALSE, FALSE, 6);
    }
    /*
     * main container is a GtkTreeView wrapped by a scrollable window */
    scrolled = gtk_scrolled_window_new (NULL, NULL);
         gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
         gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled),GTK_SHADOW_ETCHED_OUT);
         gtk_box_pack_start (GTK_BOX (parent), scrolled, TRUE, TRUE, 0);


    /* create list store for tree view */
    store = gtk_list_store_new (MSG_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (GTK_TREE_MODEL(store)));
        gtk_tree_view_set_grid_lines(treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

    /* column for timestamp */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Timestamp", renderer, "text", MSGS_COLUMN_TIMESTAMP, NULL);
        gtk_tree_view_column_set_sort_column_id (column, MSGS_COLUMN_TIMESTAMP);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(treeview, column);

    /* column for service name */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Sent By", renderer, "text", COLUMN_NODE, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_NODE);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column (treeview, column);

    /* column for IP Address */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Message", renderer, "text", COLUMN_MESSAGES, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_MESSAGES);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_append_column (treeview, column);

    g_object_unref (store);

    gtk_tree_view_set_search_column(treeview, COLUMN_NODE);
    gtk_tree_view_set_enable_search(treeview, TRUE);

    gtk_container_add (GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

    gtk_widget_show_all(parent);
    gtk_tree_view_columns_autosize(treeview);

    return (GTK_WIDGET(treeview));
}

GtkWidget * ui_registry_page_new(GtkWidget *parent, guint gSmall) {
    GtkWidget *scrolled = NULL;
    GtkTreeView *treeview = NULL;
    GtkListStore *store = NULL;
    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *column = NULL;


    if (gSmall == SKN_SMALL_DISPLAY_MODE_OFF) {
        gtk_box_pack_start (GTK_BOX (parent), gtk_label_new ("Raspberry Pi Network Services Registry.") , FALSE, FALSE, 6);
    }

    /*
     * main container is a GtkTreeView wrapped by a scrollable window */
    scrolled = gtk_scrolled_window_new (NULL, NULL);
         gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
         gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled),GTK_SHADOW_ETCHED_OUT);
         gtk_box_pack_start (GTK_BOX (parent), scrolled, TRUE, TRUE, 0);


    /* create list store for tree view */
    store = gtk_list_store_new (REG_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (GTK_TREE_MODEL(store)));
        gtk_tree_view_set_grid_lines(treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

    /* column for timestamp */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Timestamp", renderer, "text", UDP_COLUMN_TIMESTAMP, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, UDP_COLUMN_TIMESTAMP);
        gtk_tree_view_append_column(treeview, column);

    /* column for from node */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Reported By", renderer, "text", COLUMN_FROM, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_FROM);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(treeview, column);

    /* column for service name */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Service Name", renderer, "text", COLUMN_NAME, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(treeview, column);

    /* column for IP Address */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("IP Address", renderer, "text", COLUMN_IP, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_IP);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column (treeview, column);

    /* column for port number */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("UDP Port", renderer, "text", COLUMN_PORT, NULL);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_PORT);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column (treeview, column);

    g_object_unref (store);

    gtk_tree_view_set_enable_search(treeview, TRUE);
    gtk_tree_view_set_search_column (treeview, COLUMN_NAME);

    gtk_container_add (GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

    gtk_widget_show_all(parent);

    gtk_tree_view_columns_autosize(treeview);

    return (GTK_WIDGET(treeview));
}

GtkWidget * ui_gssdp_registry_page_new(GtkWidget *parent, guint gSmall) {
    GtkWidget *scrolled = NULL;
    GtkTreeView *treeview = NULL;
    GtkListStore *store = NULL;
    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *column = NULL;

    if (gSmall == SKN_SMALL_DISPLAY_MODE_OFF) {
        gtk_box_pack_start (GTK_BOX (parent), gtk_label_new ("GSSDP Network Services Registry.") , FALSE, FALSE, 6);
    }

    /*
     * main container is a GtkTreeView wrapped by a scrollable window */
    scrolled = gtk_scrolled_window_new (NULL, NULL);
         gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
         gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled),GTK_SHADOW_ETCHED_OUT);
         gtk_box_pack_start (GTK_BOX (parent), scrolled, TRUE, TRUE, 0);


    /* create list store for tree view */
    store = gtk_list_store_new (GSSDP_NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model (GTK_TREE_MODEL(store)));
        gtk_tree_view_set_grid_lines(treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

    /* column for timestamp */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Timestamp", renderer, "text", GSSDP_COLUMN_TIMESTAMP, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, GSSDP_COLUMN_TIMESTAMP);
        gtk_tree_view_append_column(treeview, column);

    /* column for service name */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("GSSDP URN", renderer, "text", COLUMN_URN, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_URN);
        gtk_tree_view_append_column(treeview, column);

    /* column for IP Address */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("GSSDP Location", renderer, "text", COLUMN_LOCATION, NULL);
        gtk_tree_view_column_set_expand(column, TRUE);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_LOCATION);
        gtk_tree_view_append_column (treeview, column);

    /* column for port number */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("GSSDP Status", renderer, "text", COLUMN_STATUS, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id (column, COLUMN_STATUS);
        gtk_tree_view_append_column (treeview, column);

    g_object_unref (store);

    gtk_tree_view_set_enable_search(treeview, TRUE);
    gtk_tree_view_set_search_column (treeview, COLUMN_URN);

    gtk_container_add (GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));

    gtk_widget_show_all(parent);

    gtk_tree_view_columns_autosize(treeview);

    return (GTK_WIDGET(treeview));
}

gboolean ui_add_message_entry(GtkWidget *treeview, PMsgData msg, gboolean limiter) {
    gboolean result = TRUE;
    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)) );
    GtkTreeIter iter;
    gint num_rows = 0;

    // insert at head of list
    gtk_list_store_insert_with_values (store, NULL, 0, MSGS_COLUMN_TIMESTAMP, msg->ch_timestamp,
                                                       COLUMN_NODE, msg->ch_remoteAddress,
                                                       COLUMN_MESSAGES, msg->ch_message, -1);

    if (limiter) {  // get last iter and remove it
        num_rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);

        if ( (num_rows > 8) && (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, (num_rows -1))) ) { // last entry
            gtk_tree_model_unref_node(GTK_TREE_MODEL(store), &iter);
            gtk_list_store_remove(store, &iter);
        }
    }

    return (result);
}

gboolean ui_add_registry_entry(GtkWidget *treeview, PRegData msg, gboolean limiter) {
    gboolean result = TRUE;
    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)) );
    GtkTreeIter iter;
    gint num_rows = 0;

    // insert at head of list
    gtk_list_store_insert_with_values (store, NULL, 0, UDP_COLUMN_TIMESTAMP, msg->ch_timestamp ,
                                                       COLUMN_FROM, msg->ch_from,
                                                       COLUMN_NAME, msg->ch_name,
                                                       COLUMN_IP, msg->ch_ip,
                                                       COLUMN_PORT, msg->ch_port, -1);

    if (limiter) {  // get last iter and remove it
        num_rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);

        if ( (num_rows > 8) && (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, (num_rows -1))) ) { // last entry
            gtk_tree_model_unref_node(GTK_TREE_MODEL(store), &iter);
            gtk_list_store_remove(store, &iter);
        }
    }

    return (result);
}

gboolean ui_add_gssdp_registry_entry(GtkWidget *treeview, PGSSDPRegData msg, gboolean limiter) {
    gboolean result = TRUE;
    GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model(GTK_TREE_VIEW(treeview)) );
    GtkTreeIter iter;
    gint num_rows = 0;

    // insert at head of list
    gtk_list_store_insert_with_values (store, NULL, 0, GSSDP_COLUMN_TIMESTAMP, msg->ch_timestamp,
                                                       COLUMN_URN, msg->ch_urn,
                                                       COLUMN_LOCATION, msg->ch_location,
                                                       COLUMN_STATUS, msg->ch_status, -1);

    if (limiter) {  // get last iter and remove it
        num_rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);

        if ( (num_rows > 8) && (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, (num_rows -1))) ) { // last entry
            gtk_tree_model_unref_node(GTK_TREE_MODEL(store), &iter);
            gtk_list_store_remove(store, &iter);
        }
    }

    return (result);
}

GtkWidget * ui_page_layout(GtkWidget *parent, PControlData pctrl) {
    GtkWidget *grid = NULL;
    GtkWidget *switcher = NULL;
    GtkWidget *stack = NULL;
    GtkWidget *pg1Box = NULL;
    GtkWidget *pg2Box = NULL;
    GtkWidget *pg3Box = NULL;


    grid = gtk_grid_new(); // left, top, width, hieght

    switcher = gtk_stack_switcher_new();
        gtk_widget_set_hexpand (switcher, TRUE);
        gtk_widget_set_halign (switcher, GTK_ALIGN_CENTER);
        gtk_grid_attach (GTK_GRID (grid), switcher, 0, 0, 1, 1);

    stack = gtk_stack_new();
        gtk_widget_set_hexpand (stack, TRUE);
        gtk_widget_set_vexpand(stack, TRUE);
        gtk_stack_set_homogeneous( GTK_STACK(stack), TRUE);
        gtk_stack_set_transition_type( GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
        gtk_stack_set_transition_duration( GTK_STACK(stack), 400);
        gtk_grid_attach (GTK_GRID (grid), stack, 0, 1, 1, 1);

    pg1Box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        pctrl->pg1Target = ui_message_page_new(pg1Box, pctrl->gSmall);
        gtk_stack_add_titled ( GTK_STACK(stack), GTK_WIDGET(pg1Box), "messages", "Messages");
    pg2Box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        pctrl->pg2Target = ui_registry_page_new(pg2Box, pctrl->gSmall);
        gtk_stack_add_titled ( GTK_STACK(stack), GTK_WIDGET(pg2Box), "udp_registry", "Rpi Registry");
    pg3Box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        pctrl->pg3Target = ui_gssdp_registry_page_new(pg3Box, pctrl->gSmall);
        gtk_stack_add_titled ( GTK_STACK(stack), GTK_WIDGET(pg3Box), "gssdp_registry", "GSSDP Registry");

    gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER(switcher), GTK_STACK(stack));
    gtk_stack_set_visible_child (GTK_STACK(stack), GTK_WIDGET(pg1Box));

    if (pctrl->gSmall == SKN_SMALL_DISPLAY_MODE_OFF) {
        pctrl->statusBar = gtk_statusbar_new();
            gtk_grid_attach (GTK_GRID (grid), pctrl->statusBar, 0, 3, 1, 1);
            pctrl->gStatusContextId = gtk_statusbar_get_context_id(GTK_STATUSBAR(pctrl->statusBar), "GtkDisplayService");
            gtk_statusbar_push(GTK_STATUSBAR(pctrl->statusBar), pctrl->gStatusContextId, "Online and Ready to do good work.");
    }

    gtk_container_add (GTK_CONTAINER(parent), grid);

    gtk_widget_show_all(grid);

    return (stack);
}


static void cb_gtk_shutdown (GtkWidget *object, gpointer data) {
    PUSignalData psig = (PUSignalData)data;
    g_message("gtkDS::cb_gtk_shutdown() %s Destroy Signal Received => Shutdown Initiated!\n", psig->signalName);
    g_main_loop_quit(psig->loop);
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

    g_async_queue_push(pctrl->queueGSSDPRegistry, msg);

}
static void cb_gssdp_resource_unavailable(GSSDPResourceBrowser *resource_browser, const char *usn, PControlData pctrl) {
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

    g_utf8_strncpy(msg->ch_location, "N/A", sizeof(msg->ch_location));
    g_utf8_strncpy(msg->ch_status, "Unavailable", sizeof(msg->ch_status));

    g_async_queue_push(pctrl->queueGSSDPRegistry, msg);
}

gboolean gssdp_publish(PControlData pctrl, guint udp_port ) {
        GError *error = NULL;
        gchar ch_buffer[SZ_MESSAGE_BUFF];


        pctrl->gssdp_rgroup_client = gssdp_client_new(NULL, NULL, &error);
        if (error) {
                g_printerr ("Error creating the GSSDP gssdp_client for resource_group: %s\n", error->message);
                g_error_free (error);
                return(FALSE);
        }

        pctrl->resource_group = gssdp_resource_group_new(pctrl->gssdp_rgroup_client);

        g_snprintf(ch_buffer, sizeof(ch_buffer), "http://%s/", pctrl->ch_this_ip );
        gssdp_resource_group_add_resource_simple
                (pctrl->resource_group,
                 "urn:rpilocator",
                 "uuid:80e6e2b7-fa4a-4d06-820a-406d5ecbbe51::upnp:rootdevice",
                 ch_buffer);

        g_snprintf(ch_buffer, sizeof(ch_buffer), "udp://%s:%d", pctrl->ch_this_ip, udp_port );
        gssdp_resource_group_add_resource_simple
                (pctrl->resource_group,
                 "urn:rpilocator",
                 "uuid:21798cdc-d875-42b9-bb52-8043e6396301::urn:schemas-upnp-org:service:GtkDisplayService:1",
                 ch_buffer);

        gssdp_resource_group_set_available(pctrl->resource_group, TRUE);


        return(TRUE);
}

/**
 * Setup a monitor for the registry page
*/
gboolean gssdp_browse(PControlData pctrl) {

    pctrl->resource_browser = gssdp_resource_browser_new (pctrl->gssdp_rgroup_client, pctrl->pch_search); // GSSDP_ALL_RESOURCES);
    g_signal_connect (pctrl->resource_browser, "resource-available", G_CALLBACK (cb_gssdp_resource_available), pctrl);
    g_signal_connect (pctrl->resource_browser, "resource-unavailable", G_CALLBACK (cb_gssdp_resource_unavailable), pctrl);

    gssdp_resource_browser_set_active(pctrl->resource_browser, TRUE);

    return (TRUE);
}

int main(int argc, char **argv) {

    GError *error = NULL;
    GMainLoop *loop = NULL;
    GSocket *gSock = NULL;
    GSocketAddress *gsAddr = NULL;
    GSource * gCommSource = NULL;
    GSocketAddress *gbAddr = NULL;
    GSource * gBroadSource = NULL;
    GSource * gMsgSource = NULL;
    GSource * gRegSource = NULL;
    GSource * gGSSDPRegSource = NULL;
    ControlData cData;
    PIPBroadcastArray paB = NULL;
    GInetAddress *anyAddr = NULL;
    USignalData sigTerm;
    USignalData sigInt;
    USignalData sigHup;
    USignalData winDestroy;
    GtkWidget *window = NULL;
    guint gUDPPort = 0;
	GOptionContext *gOptions = NULL;
	GOptionEntry pgmOptions[] = {
       {"service-name", 'a', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &cData.pch_service_name, "Alternate Service Name", "gtk_display_service"},
       {"udp_port_number", 'p', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &gUDPPort, "UDP Port Number to listen on. default=48029", "48029"},
       {"small", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &cData.gSmall, "Small Screen mode, reduced status and titles.", NULL},
       {"search-key", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &cData.pch_search, "GSSDP Search Key.", "[urn:rpilocator | ssdp:all]"},
  	   {NULL}
	};
    cData.gSmall = SKN_SMALL_DISPLAY_MODE_OFF;
    cData.pch_search = NULL;
    cData.pch_service_name = NULL;

	gOptions = g_option_context_new ("UDP message display service for IOT.");
	g_option_context_add_group (gOptions, (GOptionGroup *) gtk_get_option_group(TRUE) );
	g_option_context_add_main_entries (gOptions, pgmOptions, NULL);

	g_option_context_parse (gOptions, &argc, &argv, &error);
    if (error != NULL) {
        g_error("g_option_context_parse() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }
	g_option_context_free(gOptions);

    if (gUDPPort == 0) {
        gUDPPort = cData.gUDPPort = UDP_COMM_PORT;
    } else {
        cData.gUDPPort = gUDPPort;
    }
    if (NULL == cData.pch_search) {
        cData.pch_search = "urn:rpilocator";
    }
    if (NULL == cData.pch_service_name) {
        cData.pch_service_name = "gtk_display_service";
    }

    cData.paB = paB = skn_get_default_interface_name_and_ipv4_address((char *)&cData.ch_intfName, (char *)&cData.ch_this_ip);
    if (NULL == paB) {
        g_error("skn_skn_get_default_interface_name_and_ipv4_address() => Unable to discover network interface or non-available.");
        exit(EXIT_FAILURE);
    }

    winDestroy.loop = sigTerm.loop = sigInt.loop = sigHup.loop = cData.loop = loop = g_main_loop_new(NULL, FALSE);
    sigTerm.signalName = "SIGTERM";
    sigInt.signalName = "SIGINT";
    sigHup.signalName = "SIGHUP";
    winDestroy.signalName = "gtkDestroyWindow";
    cData.resolver = g_resolver_get_default();
    cData.queueMessage = g_async_queue_new();
    cData.queueRegistry = g_async_queue_new();
    cData.queueGSSDPRegistry = g_async_queue_new();
    cData.gMsgCount = 0;
    cData.gRegCount = 0;
    cData.gGSSDPRegCount = 0;


    /*
     * Create and Add MessageQueue to main loop for service (i.e. incoming messages)   */
    gMsgSource = message_queue_source_new (cData.queueMessage, (GDestroyNotify) g_free, NULL);
        g_source_ref(gMsgSource);
        g_source_set_callback (gMsgSource, (GSourceFunc)cb_message_request_handler, &cData, NULL);
        cData.gMsgSourceId = g_source_attach (gMsgSource, NULL);

    gRegSource = message_queue_source_new(cData.queueRegistry, (GDestroyNotify) g_free, NULL);
        g_source_ref(gRegSource);
        g_source_set_callback (gRegSource, (GSourceFunc)cb_registry_request_handler, &cData, NULL);
        cData.gRegSourceId = g_source_attach (gRegSource, NULL);

    gGSSDPRegSource = message_queue_source_new(cData.queueGSSDPRegistry, (GDestroyNotify) g_free, NULL);
        g_source_ref(gGSSDPRegSource);
        g_source_set_callback (gGSSDPRegSource, (GSourceFunc)cb_gssdp_registry_request_handler, &cData, NULL);
        cData.gGSSDPRegSourceId = g_source_attach (gGSSDPRegSource, NULL);

    /*
     * Create regular UDP Socket for receiving messages */
    gSock = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    if (error != NULL) {
        g_error("g_socket_new() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    anyAddr = g_inet_address_new_any(G_SOCKET_FAMILY_IPV4);
    gsAddr = g_inet_socket_address_new(anyAddr, gUDPPort);

    g_socket_bind(gSock, gsAddr, TRUE, &error);
    if (error != NULL) {
        g_error("g_socket_bind() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    gCommSource = g_socket_create_source (gSock, G_IO_IN, NULL);
        g_source_ref(gCommSource);
        g_source_set_callback (gCommSource, (GSourceFunc) cb_udp_comm_request_handler, &cData, NULL); // its really a GSocketSourceFunc
        cData.gCommSourceId =  g_source_attach (gCommSource, NULL);

    /*
     * Create broadcast UDP Socket for receiving messages */
    cData.gbSock = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    if (error != NULL) {
        g_error("g_socket_new(broadcast) => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }
    g_socket_set_broadcast(cData.gbSock, TRUE);

    gbAddr = g_inet_socket_address_new(anyAddr, UDP_BROADCAST_PORT);
        g_object_unref(anyAddr);

    g_socket_bind(cData.gbSock, gbAddr, TRUE, &error);
    if (error != NULL) {
        g_error("g_socket_bind() => %s", error->message);
        g_clear_error(&error);
        exit(EXIT_FAILURE);
    }

    gBroadSource = g_socket_create_source(cData.gbSock, G_IO_IN, NULL);
        g_source_ref(gBroadSource);
        g_source_set_callback (gBroadSource, (GSourceFunc) cb_udp_broadcast_response_handler, &cData, NULL); // its really a GSocketSourceFunc
        cData.gBroadSourceId =  g_source_attach (gBroadSource, NULL);

        g_timeout_add(MS_TEN_MINUTES, (GSourceFunc)cb_registry_refresh, &cData);

    /*
     * Handle ctrl-break and kill signals cleanly */
    g_unix_signal_add (SIGINT, (GSourceFunc) cb_unix_signal_handler, &sigInt); // SIGINT signal (Ctrl+C)
    g_unix_signal_add (SIGTERM,(GSourceFunc) cb_unix_signal_handler, &sigTerm);
    g_unix_signal_add (SIGHUP, (GSourceFunc) cb_unix_signal_handler, &sigHup);

    /*
     * Create a GTK+3 Page to show messages */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (window), "Gtk Display Service");
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);
        gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
            g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(cb_gtk_shutdown), &winDestroy);

    /*
     * Create body of main page  */
    ui_page_layout(GTK_WIDGET(window), &cData);

    gtk_widget_show_all(GTK_WIDGET(window));


    /*
     * Broadcast Registry Request: 10.100.1.255 */
    if ( gssdp_publish(&cData, gUDPPort) && gssdp_browse(&cData) && skn_udp_network_broadcast_all_interfaces(cData.gbSock, paB) ) {
        g_message("gtkDS: Ready to do Good, on %s:%s:%d...\n", cData.ch_intfName, cData.ch_this_ip, gUDPPort);

        g_main_loop_run(loop);
    }

    gssdp_resource_browser_set_active(cData.resource_browser, FALSE);
    gssdp_resource_group_set_available (cData.resource_group, FALSE);

    g_source_unref(gCommSource);
    g_source_unref(gBroadSource);
    g_source_unref(gRegSource);
    g_source_unref(gMsgSource);
    g_source_unref(gGSSDPRegSource);
    g_object_unref(gSock);
    g_object_unref(cData.gbSock);
    g_object_unref(gsAddr);
    g_object_unref(gbAddr);
    g_object_unref(cData.resolver);
    g_async_queue_unref(cData.queueRegistry);
    g_async_queue_unref(cData.queueMessage);
    g_async_queue_unref(cData.queueGSSDPRegistry);
    g_object_unref (cData.resource_browser);
    g_object_unref (cData.resource_group);
    g_object_unref (cData.gssdp_rgroup_client);

    g_main_loop_unref(loop);

    g_message("gtkDS: shutdown...");

    exit(EXIT_SUCCESS);
}
