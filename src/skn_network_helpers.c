/**
 * sknNetworkHelpers.c
 */

#include "skn_network_helpers.h"


PServiceRegistry skn_service_registry_valiadated_registry(const char *response);
          void * skn_service_registry_get_entry_field_ref(PRegistryEntry prent, char *field);
          void * skn_service_registry_entry_create_helper(char *key, char **name, char **ip, char **port);
             int skn_service_registry_entry_create(PServiceRegistry psreg, char *name, char *ip, char *port, int *errors);
             int skn_service_registry_response_parse(PServiceRegistry psreg, const char *response, int *errors);
PServiceRegistry skn_service_registry_create();




/**
 * skn_udp_host_create_regular_socket()
 * - creates a dgram socket without broadcast enabled
 *
 * - returns i_socket | EXIT_FAILURE
 */
int skn_udp_host_create_regular_socket(int port, double rcvTimeout) {
    struct sockaddr_in addr;
    int i_socket, reuseEnable = 1;

    if ((i_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        skn_util_logger(SD_EMERG, "Create Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    if ((setsockopt(i_socket, SOL_SOCKET, SO_REUSEADDR, &reuseEnable, sizeof(reuseEnable))) < 0) {
        skn_util_logger(SD_EMERG, "Set Socket Reuse Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = rcvTimeout;
    tv.tv_usec = (long)(rcvTimeout - tv.tv_sec) * 1000000L;
    if ((setsockopt(i_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        skn_util_logger(SD_EMERG, "Set Socket RcvTimeout Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    /* set up local address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(i_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        skn_util_logger(SD_EMERG, "Bind to local Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    return i_socket;
}

/**
 * skn_udp_host_create_broadcast_socket()
 * - creates a dgram socket with broadcast enabled
 *
 * - returns i_socket | EXIT_FAILURE
 */
int skn_udp_host_create_broadcast_socket(int port, double rcvTimeout) {
    struct sockaddr_in addr;
    int i_socket, broadcastEnable = 1;

    if ((i_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        skn_util_logger(SD_EMERG, "Create Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }
    if ((setsockopt(i_socket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) < 0) {
        skn_util_logger(SD_EMERG, "Set Socket Broadcast Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = rcvTimeout;
    tv.tv_usec = (long)(rcvTimeout - tv.tv_sec) * 1000000L;
    skn_util_logger(SD_INFO, "Set Socket RcvTimeout Option set to: tv_sec=%ld, tv_usec=%ld", tv.tv_sec, tv.tv_usec);
    if ((setsockopt(i_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) < 0) {
        skn_util_logger(SD_EMERG, "Set Socket RcvTimeout Option error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    /* set up local address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(i_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        skn_util_logger(SD_EMERG, "Bind to local Socket error=%d, etext=%s", errno, strerror(errno));
        return (EXIT_FAILURE);
    }

    return i_socket;
}

/*
 * Remote Service Registry routines
*/
PServiceRequest skn_udp_service_provider_service_request_new(PRegistryEntry pre, int host_socket, char *request) {
    PServiceRequest psr = NULL;

    if (pre == NULL) {
        return NULL;
    }
    psr = (PServiceRequest) malloc(sizeof(ServiceRequest));
    memset(psr, 0, sizeof(ServiceRequest));
    strcpy(psr->cbName, "PServiceRequest");
    psr->iSocket = host_socket;
    psr->pre = pre;
    strncpy(psr->request, request, SZ_INFO_BUFF-1);
    return psr;
}

/**
 * skn_udp_service_provider_registry_responder
 * - Listen for Registry requests and responder with our entries,
 *   until gi_exit_flag == SKN_RUN_MODE_RUN
 *
 * @param i_socket
 * @param response
 * @return
 */
int skn_udp_service_provider_registry_responder(int i_socket, char *response) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    IPBroadcastArray aB;
    char request[SZ_INFO_BUFF];
    char recvHostName[SZ_INFO_BUFF];
    signed int rLen = 0, rc = 0;
    int exit_code = EXIT_SUCCESS, i_response_len = 0;


    memset(request, 0, sizeof(request));
    memset(recvHostName, 0, sizeof(recvHostName));

    rc = skn_util_get_broadcast_ip_array(&aB);
    if (rc == PLATFORM_ERROR) {
        return EXIT_FAILURE;
    }

    if (gd_pch_service_name == NULL) {
        gd_pch_service_name = "lcd_display_service";
    }

    if (strlen(response) < 16) {
        if (gd_i_display) {
            i_response_len = snprintf(response, (SZ_INFO_BUFF - 1),
                     "name=rpi_locator_service,ip=%s,port=%d|"
                     "name=%s,ip=%s,port=%d|",
                     aB.ipAddrStr[aB.defaultIndex], SKN_FIND_RPI_PORT,
                     gd_pch_service_name,
                     aB.ipAddrStr[aB.defaultIndex], SKN_RPI_DISPLAY_SERVICE_PORT);
        } else {
            i_response_len = snprintf(response, (SZ_INFO_BUFF - 1),
                            "name=rpi_locator_service,ip=%s,port=%d|",
                            aB.ipAddrStr[aB.defaultIndex], SKN_FIND_RPI_PORT);
        }
    }
    skn_udp_service_provider_registry_entry_response_logger(response);

    skn_util_logger(SD_DEBUG, "Socket Bound to %s:%s", aB.chDefaultIntfName, aB.ipAddrStr[aB.defaultIndex]);

    while (gi_exit_flag == SKN_RUN_MODE_RUN) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_port = htons(SKN_FIND_RPI_PORT);
        remaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        addrlen = sizeof(remaddr);

        if ((rLen = recvfrom(i_socket, request, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen)) < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            skn_util_logger(SD_ERR, "RcvFrom() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }
        request[rLen] = 0;

        rc = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, (SZ_INFO_BUFF-1), NULL, 0, NI_DGRAM);
        if (rc != 0) {
            skn_util_logger(SD_ERR, "GetNameInfo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }
        skn_util_logger(SD_NOTICE, "Received request from %s @ %s:%d", recvHostName, inet_ntoa(remaddr.sin_addr), ntohs(remaddr.sin_port));
        skn_util_logger(SD_NOTICE, "Request data: [%s]\n", request);

        /*
         * Add new registry entry by command */
        if ((strncmp("ADD ", request, sizeof("ADD")) == 0) &&
            (skn_service_registry_valiadate_response_format(&request[4]) == EXIT_SUCCESS)) {
            if ((response[i_response_len-1] == '|') ||
                (response[i_response_len-1] == '%') ||
                (response[i_response_len-1] == ';')) {
                strncpy(&response[i_response_len], &request[4], ((SZ_COMM_BUFF - 1) - (strlen(response) + strlen(&request[4]))) );
                i_response_len += (rLen - 4);
                skn_util_logger(SD_NOTICE, "COMMAND: Add New RegistryEntry Request Accepted!");
            }
        }

        if (sendto(i_socket, response, strlen(response), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_util_logger(SD_EMERG, "SendTo() Failure code=%d, etext=%s", errno, strerror(errno));
            exit_code = EXIT_FAILURE;
            break;
        }

        /*
         * Shutdown by command */
        if (strcmp("QUIT!", request) == 0) {
            skn_util_logger(SD_NOTICE, "COMMAND: Shutdown Requested! exit code=%d", gi_exit_flag);
            break;
        }
    }

    return exit_code;
}

/**
 * skn_udp_service_provider_registry_entry_response_logger()
 *
 * - Prints each pair of values from a text registry entry
*/
void skn_udp_service_provider_registry_entry_response_logger(const char * response) {
    char * worker = NULL, *parser = NULL, *base = strdup(response);
    parser = base;
    skn_util_logger(SD_NOTICE, "Response Message:");
    while (( ((worker = strsep(&parser, "|")) != NULL) ||
          ((worker = strsep(&parser, "%")) != NULL) ||
          ((worker = strsep(&parser, ";")) != NULL)) &&
          (strlen(worker) > 8)) {
        skn_util_logger(SD_NOTICE, "[%s]", worker);
    }
    free(base);
}

/**
 * skn_udp_service_provider_send()
 * - side effects: none
 *
 * - returns EXIT_SUCCESS | EXIT_FAILURE
 */
int skn_udp_service_provider_send(PServiceRequest psr) {
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    signed int vIndex = 0;
    struct timeval start, end;

    memset(&remaddr, 0, sizeof(remaddr));
    remaddr.sin_family = AF_INET;
    remaddr.sin_addr.s_addr = inet_addr(psr->pre->ip);
    remaddr.sin_port = htons(psr->pre->port);

    /*
     * SEND */
    gettimeofday(&start, NULL);
    if (sendto(psr->iSocket, psr->request, strlen(psr->request), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
        gettimeofday(&end, NULL);
        skn_util_logger(SD_WARNING, "ServiceRequest: SendTo(%1.6f) Timed out; Failure code=%d, etext=%s", skn_util_duration_in_ms(&start,&end), errno, strerror(errno));
        return EXIT_FAILURE;
    }
    skn_util_logger(SD_NOTICE, "ServiceRequest sent to %s:%s:%d", psr->pre->name, psr->pre->ip, psr->pre->port);

    /*
     * RECEIVE */
    vIndex = recvfrom(psr->iSocket, psr->response, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen);
    if (vIndex == PLATFORM_ERROR) {
        gettimeofday(&end, NULL);
        skn_util_logger(SD_WARNING, "ServiceRequest: recvfrom(%1.6f) Failure code=%d, etext=%s", skn_util_duration_in_ms(&start,&end), errno, strerror(errno));
        return EXIT_FAILURE;
    }
    psr->response[vIndex] = 0;
    gettimeofday(&end, NULL);

    skn_util_logger(SD_INFO, "Response(%1.3fs) received from [%s] %s:%d",
                    skn_util_duration_in_ms(&start,&end),
                    psr->response,
                    inet_ntoa(remaddr.sin_addr),
                    ntohs(remaddr.sin_port)
              );

    if (strcmp(psr->response, "QUIT!") == 0) {
        skn_util_logger(SD_NOTICE, "Shutdown Requested!");
        return EXIT_FAILURE;
    }

    return (EXIT_SUCCESS);
}


/**
 * service_registry_create()
 *
 * - Collection of Services and their locations
 * - Returns the Registry
 */
PServiceRegistry skn_service_registry_create() {
    PServiceRegistry psreg = NULL;

    psreg = (PServiceRegistry) malloc(sizeof(ServiceRegistry));
    if (psreg != NULL) {
        memset(psreg, 0, sizeof(ServiceRegistry));
        strcpy(psreg->cbName, "PServiceRegistry");
        psreg->computedMax = (sizeof(psreg->entry) / sizeof(void *));
        psreg->count = 0;
    }

    return psreg;
}

/**
 * service_registry_entry_create()
 *
 * - Create a Service Entry and adds it to the Registry collection
 * - Returns EXIT_FAILURE/SUCCESS
*/
int skn_service_registry_entry_create(PServiceRegistry psreg, char *name, char *ip, char *port, int *errors) {
    PRegistryEntry prent = NULL;

    if ((psreg == NULL) || (name == NULL) || (ip == NULL) || (port == NULL)) {
        skn_util_logger(SD_DEBUG, "Parse failure missing value: (%s,%s,%s)", name, ip, port);
        if (errors != NULL)
            (*errors)++;
        return EXIT_FAILURE;
    }

    if (psreg->count >= psreg->computedMax) {
        skn_util_logger(SD_WARNING, "Capacity Error: Too many! New entry %d:%s exceeds maximum of %d allowed! Consider using the --unique-registry option.", psreg->count, name, psreg->computedMax);
        if (errors != NULL)
            (*errors)++;
        return EXIT_FAILURE;
    }

    /* update or create entry */
    if (gd_i_unique_registry) {
        prent = skn_service_registry_find_entry(psreg, name);
    }
    if (prent == NULL) {
        prent = (PRegistryEntry) malloc(sizeof(RegistryEntry));
        if (prent != NULL) {
            psreg->entry[psreg->count] = prent;
            psreg->count++;
        }
    }

    if (prent != NULL) {
        memset(prent, 0, sizeof(RegistryEntry));
        strcpy(prent->cbName, "PRegistryEntry");
        strcpy(prent->name, name);
        strcpy(prent->ip, ip);
        prent->port = atoi(port);
    } else {
        skn_util_logger(SD_WARNING, "Internal Memory Error: Could not allocate memory for entry %d:%s !", name, psreg->count);
        if (errors != NULL)
            (*errors)++;

        return EXIT_FAILURE;
    }

    return psreg->count;
}

/**
 * service_registry_valiadate_response_format()
 *
 * - Validates a text registry entry format
 * - Returns EXIT_FAILURE/SUCCESS
*/
int skn_service_registry_valiadate_response_format(const char *response) {
    int errors = 0; // false

    PServiceRegistry psr = skn_service_registry_create();
    skn_service_registry_response_parse(psr, response, &errors);
    skn_service_registry_destroy(psr);

    if (errors > 0) {
        return EXIT_FAILURE; // false
    } else {
        return EXIT_SUCCESS; // true
    }
}

/**
 * service_registry_valiadated_registry()
 *
 * - Validate the text formatted entry and return registry collection
 * - Returns a Registry with one entry
*/
PServiceRegistry skn_service_registry_valiadated_registry(const char *response) {
    int errors = 0;

    PServiceRegistry psr = skn_service_registry_create();
    skn_service_registry_response_parse(psr, response, &errors);
    if (errors > 0) {
        free(psr);
        return NULL; // false
    }

    return psr;
}

/**
 * service_registry_find_entry()
 *
 * - Finds an existing entry
 * - Returns EXIT_FAILURE/SUCCESS
*/
PRegistryEntry skn_service_registry_find_entry(PServiceRegistry psreg, char *serviceName) {
    PRegistryEntry prent = NULL;
    int index = 0;

    if (psreg == NULL)
        return NULL;

    while (index < psreg->count) {
        if (strcmp(serviceName, psreg->entry[index]->name) == 0) {
            prent = psreg->entry[index];
            break;
        }
        index++;
    }

    return prent;
}

/**
 * Returns the number of services in registry
 */
int skn_service_registry_entry_count(PServiceRegistry psr) {
    return psr->count;
}

/**
 * Logs each entry in service registry
 */
int skn_service_registry_list_entries(PServiceRegistry psr) {
    int index = 0;

    skn_util_logger(" ", "\nServiceRegistry:");
    for (index = 0; index < psr->count; index++) {
        skn_util_logger(SD_INFO, "RegistryEntry(%02d) name=%s, ip=%s, port=%d", index, psr->entry[index]->name, psr->entry[index]->ip, psr->entry[index]->port);
    }
    return index;
}

/**
 * service_registry_get_entry_field_ref()
 * - compute the address of each field on demand, from struct
 * - Returns the address offset of the registry field requested
*/
void * skn_service_registry_get_entry_field_ref(PRegistryEntry prent, char *field) {
    int index = 0;
    void * result = NULL;
    char * names[4] = { "name", "ip", "port", NULL };
    int offsets[4] = { offsetof(RegistryEntry, name),
                       offsetof(RegistryEntry, ip),
                       offsetof(RegistryEntry, port),
                       0
                     };
    for (index = 0; names[index] != NULL; index++) {
        if (strcmp(names[index], field) == 0) {
            result = (void *) prent + offsets[index];
            break;
        }
    }
    return result;
}

/**
 * service_registry_entry_create_helper()
 * - Determines which field should be updated
 * - compute the address of each field on demand, from local vars
 * - Returns the address offset of the registry field requested
*/
void * skn_service_registry_entry_create_helper(char *key, char **name, char **ip, char **port) {
    int index = 0;
    char * guess = NULL;
    void * result = NULL;
    char * names[4] = { "name", "ip", "port", NULL };
    void * offsets[] = { name, ip, port, NULL };

    skn_util_strip(key); // cleanup first
    for (index = 0; names[index] != NULL; index++) { // find direct match
        if (strcmp(names[index], key) == 0) {
            result = offsets[index];
            break;
        }
    }
    if (result == NULL) { // try to guess what the key is
        if ((guess = strstr(key, "e")) != NULL) {
            result = offsets[0];
        } else if ((guess = strstr(key, "i")) != NULL) {
            result = offsets[1];
        } else if ((guess = strstr(key, "t")) != NULL) {
            result = offsets[2];
        }
    }

    return result;
}

/**
 * service_registry_response_parse()
 *
 * Parse this response message, guess spellings, skip invalid, export error count
 *
 * Format: name=rpi_locator_service,ip=10.100.1.19,port=48028|
 *         name=lcd_display_service,ip=10.100.1.19,port=48029|
 *
 *  the vertical bar char '|' is the line separator, % and ; are also supported
 *
*/
int skn_service_registry_response_parse(PServiceRegistry psreg, const char *response, int *errors) {
    int control = 1;
    char *base = NULL, *psep = NULL, *resp = NULL, *line = NULL,
         *keypair = NULL, *element = NULL,
         *name = NULL, *ip = NULL, *pport = NULL,
         **meta = NULL;

    base = resp = strdup(response);

    if (strstr(response, "|")) {
        psep = "|";
    } else if (strstr(response, "%")) {
        psep = "%";
    } else if (strstr(response, ";")) {
        psep = ";";
    }

    while ((line = strsep(&resp, psep)) != NULL) {
        if (strlen(line) < 16) {
            continue;
        }

        pport = ip = name = NULL;
        while ((keypair = strsep(&line, ",")) != NULL) {
            if (strlen(keypair) < 1) {
                continue;
            }

            control = 1;
            element = strstr(keypair, "=");
            if (element != NULL) {
                element[0] = 0;
                meta = skn_service_registry_entry_create_helper(keypair, &name, &ip, &pport);
                if (meta != NULL && (element[1] != 0)) {
                    *meta = skn_util_strip(++element);
                } else {
                    control = 0;
                    if (errors != NULL)
                        (*errors)++;
                    skn_util_logger(SD_WARNING, "Response format failure: for name=%s, ip=%s, port=%s, first failing entry: [%s]", name, ip, pport, keypair);
                    break;
                }
            }
        } // end while line

        if (control == 1) { // catch a breakout caused by no value
            skn_service_registry_entry_create(psreg, name, ip, pport, errors);
        }

    } // end while buffer

    free(base);
    return psreg->count;
}

/**
 * skn_service_registry_new()
 *
 * - Retrieves entries from every service that responds to the broadcast
 *   parses and builds a new Registry of all entries, or unique entries.
 *
 * - Returns Populated Registry
*/
PServiceRegistry skn_service_registry_new(char *request) {
    int i_socket = EXIT_FAILURE;
    struct sockaddr_in remaddr; /* remote address */
    socklen_t addrlen = sizeof(remaddr); /* length of addresses */
    IPBroadcastArray aB;
    int vIndex = 0;
    char response[SZ_INFO_BUFF];
    char recvHostName[SZ_INFO_BUFF];
    signed int rLen = 0;
    struct timeval start;

    memset(response, 0, sizeof(response));
    memset(recvHostName, 0, sizeof(recvHostName));

    /* Create local socket for sending requests */
    i_socket = skn_udp_host_create_broadcast_socket(0, 4.0);
    if (i_socket == EXIT_FAILURE) {
        return (NULL);
    }

    skn_util_get_broadcast_ip_array(&aB);
    strncpy(gd_ch_intfName, aB.chDefaultIntfName, SZ_CHAR_BUFF);
    strncpy(gd_ch_ipAddress, aB.ipAddrStr[aB.defaultIndex], SZ_CHAR_BUFF);

    skn_util_logger(SD_NOTICE, "Socket Bound to %s", aB.ipAddrStr[aB.defaultIndex]);

    gettimeofday(&start, NULL);
    for (vIndex = 0; vIndex < aB.count; vIndex++) {
        memset(&remaddr, 0, sizeof(remaddr));
        remaddr.sin_family = AF_INET;
        remaddr.sin_addr.s_addr = inet_addr(aB.broadAddrStr[vIndex]);
        remaddr.sin_port = htons(SKN_FIND_RPI_PORT);

        if (sendto(i_socket, request, strlen(request), 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
            skn_util_logger(SD_WARNING, "SendTo() Timed out; Failure code=%d, etext=%s", errno, strerror(errno));
            break;
        }
        skn_util_logger(SD_NOTICE, "Message Broadcasted on %s:%s:%d", aB.ifNameStr[vIndex], aB.broadAddrStr[vIndex], SKN_FIND_RPI_PORT);
    }

    PServiceRegistry psr = skn_service_registry_create();
    skn_util_logger(SD_DEBUG, "Waiting for all responses\n");
    while (gi_exit_flag == SKN_RUN_MODE_RUN) { // depends on a socket timeout of 5 seconds

        rLen = recvfrom(i_socket, response, (SZ_INFO_BUFF - 1), 0, (struct sockaddr *) &remaddr, &addrlen);
        if (rLen == PLATFORM_ERROR) {  // EAGAIN
            break;
        }
        response[rLen] = 0;

        rLen = getnameinfo(((struct sockaddr *) &remaddr), sizeof(struct sockaddr_in), recvHostName, (SZ_INFO_BUFF -1), NULL, 0, NI_DGRAM);
        if (rLen != 0) {
            skn_util_logger(SD_EMERG, "getnameinfo() failed: %s\n", gai_strerror(rLen));
            break;
        }

        skn_util_logger(SD_DEBUG, "Response(%1.3fs) received from %s @ %s:%d",
                        skn_util_duration_in_ms(&start, NULL),
                        recvHostName,
                        inet_ntoa(remaddr.sin_addr),
                        ntohs(remaddr.sin_port)
                  );
        skn_service_registry_response_parse(psr, response, NULL);
    }
    if (i_socket) {
        close(i_socket);
    }

    return (psr);
}

/**
 * service_registry_destroy()
 * - Free each entry, then the Registry itself.
*/
void skn_service_registry_destroy(PServiceRegistry psreg) {
    int index = 0;

    if (psreg == NULL)
        return;

    for (index = 0; index < psreg->count; index++) {
        free(psreg->entry[index]);
    }
    free(psreg);
}
