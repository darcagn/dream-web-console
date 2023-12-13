#include "common.h"
#include "gdrom/gdrom.h"

void log_request(http_state_t *hs) {
    switch(hs->method) {
        case METHOD_GET:
            DWC_LOG("httpd: GET %s\n", hs->path);
            break;
        case METHOD_POST:
            DWC_LOG("httpd: POST %s\n", hs->path);
        default:
    }
}

/* Can't think of why we'll need any more than this for our
 * HTTP header line, but we can adjust it if needs change. */
#define HEADER_MAXSIZE 256

/* Client threads start here */
void *client_thread(void *p) {
    http_state_t *hs = (http_state_t *)p;

    /* Buffer used to parse and hold header for request */
    char *buf = calloc(1, HEADER_MAXSIZE);
    if(buf == NULL) {
        DWC_LOG("httpd: malloc failure in client_thread(), socket %d\n", hs->socket);
        send_error(hs, 503, "malloc failure in client_thread()");
        goto client_thread_out;
    }

    /* Read from socket and determine HTTP method. */
    for(size_t i = 0;; ++i) {
        if(read(hs->socket, &buf[i], 1) < 1) {
            DWC_LOG("httpd: Unable to read method, socket %d\n", hs->socket);
            send_error(hs, 400, "Unable to read method from socket.");
            goto client_thread_out;
        }

        /* If we find space or 8th iteration (i=7),
         * null terminate the buffer and break */
        if(buf[i] == 0x20 || i >= 7) {
            buf[i] = 0;
            break;
        }
    }

    /* Determine if method type is supported and set in our http state struct */
    /* TODO: Support HEAD method, but we have to address that within handlers */
    if(strcmp(buf, "GET") == 0) {
        hs->method = METHOD_GET;
    } else if(strcmp(buf, "POST") == 0) {
        hs->method = METHOD_POST;
    } else {
        DWC_LOG("httpd: Method %s unsupported on socket %d\n", buf, hs->socket);
        send_error(hs, 501, "Requested method is not implemented.");
        goto client_thread_out;
    }

    /* Read in the remainder of the request path until we reach the newline. */
    size_t path_len = 0; /* Save the length of our path here */
    for(size_t i = 0;; ++i) {
        if(i >= HEADER_MAXSIZE) {
            DWC_LOG("httpd: URI too long, quitting on socket %d\n", hs->socket);
            send_error(hs, 414, "URI too long.");
            goto client_thread_out;
        }

        if(read(hs->socket, &buf[i], 1) < 1) {
            DWC_LOG("httpd: Error reading request path, socket %d\n", hs->socket);
            send_error(hs, 400, "Error while reading request path.");
            goto client_thread_out;
        }

        /* We only need up to the first space for our request string, so null terminate
         * any if found. We also need to handle the carriage return character. */
        /* TODO: Should we reject more invalid character types here? */
        if(buf[i] == ' ' || buf[i] == '\r') {
            buf[i] = 0;
            /* Save our path length if it wasn't already */
            if(!path_len)
                path_len = i + 1;
        }

        /* If we reach the end of the header indicated by a newline,
         * we will then terminate the buffer string and break. */
        if(buf[i] == '\n') {
            buf[i] = 0;
            /* Save our path length if it wasn't already */
            if(!path_len)
                path_len = i + 1;
            /* If we wanted to check HTTP version, we could grab that now... */
            break;
        }
    }

    /* Our http state struct will now own the processed requested path */
    hs->path = realloc(buf, path_len);
    buf = NULL;
    /* That should have been reallocation in place and never fail, but... */
    if(!hs->path) {
        DWC_LOG("httpd: realloc failure in client_thread(), socket %d\n", hs->socket);
        send_error(hs, 503, "realloc failure in client_thread()");
        goto client_thread_out;
    }

    /* Log our request */
    log_request(hs);

    /*TODO: Read in and set relevant header options here */
    /* Let's get another line to see if there's other interesting information for us in the header */
    /*buf = calloc(1, HEADER_MAXSIZE);
    if(buf == NULL) {
        DWC_LOG("httpd: malloc failure in client_thread(), socket %d\n", hs->socket);
        send_error(hs, 503, "malloc failure in client_thread()");
        goto client_thread_out;
    }*/

    /* If GET, should be able to ignore the rest of header and respond */
    /* Set Referrer in hs? */
    /* Set boundary in hs */
    /* Set content-length in hs? */

    /* We now have enough information to dispatch the request to an appropriate
     * handler. Let's make some helper macros to make finding a handler easier */

#define GET                     (hs->method == METHOD_GET)
#define POST                    (hs->method == METHOD_POST)
#define PATH_IS(x)              (!strcmp(hs->path, x))
#define PATH_STARTS_WITH(x)     (!strncmp(hs->path, x, strlen(x)))


    /********************************
     ******* Dispatch Time!  ********
     ********************************/

    /* Send main page */
    if(GET && PATH_IS("/")) {
        send_main(hs);
        goto client_thread_out;
    }

    /* Handle filer request */
    /* TODO: This is inelegant browsing...
     * We'll probably remove this when we have source-specific
     * browsing fully implemented for all sources. */
    if(GET && PATH_STARTS_WITH("/?/")) {
        send_fs(hs, hs->path + 2);
        goto client_thread_out;
    }

    /********************************
     ******* System Handling ********
     ********************************/

    if(PATH_STARTS_WITH("/system/")) {

        /* Handle system index page */
        if(GET && PATH_IS("/system/")) {
            send_error(hs, 404, "System page is currently unimplemented. Come back later for more.");
            goto client_thread_out;
        }

        /* Handle maple diagnostics page */
        if(GET && PATH_IS("/system/maple")) {
            send_error(hs, 404, "Maple diagnostics page is currently unimplemented. Come back later for more.");
            goto client_thread_out;
        }

        /* FIXME: Parse parameters in the called function instead of here... */
        /* Handle downloading memory ranges */
        if(GET && PATH_STARTS_WITH("/system/mem")) {
            size_t memory_start, memory_end;
            if(sscanf(hs->path, "/system/mem?start%x_end%x", &memory_start, &memory_end) == 2) {
                send_memory(hs, memory_start, memory_end, NULL);
                goto client_thread_out;
            }
        }

    }

    /********************************
     **** GD-ROM / Disc Handling ****
     ********************************/

    if(PATH_STARTS_WITH("/disc/")) {

        /* Handle disc spin down command */
        if(GET && PATH_IS("/disc/spin_down")) {
            gdrom_spin_down(hs);
            goto client_thread_out;
        }

        /* FIXME: Clean up disc index handling by parsing parameters
         * in the called function instead of here... */

        /* Handle downloading disc index (standard data with standard TOC) */
        if(GET && PATH_IS("/disc/")) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_DATA);
            goto client_thread_out;
        }

        /* Handle downloading disc index (standard data with IP.BIN TOC */
        if(GET && PATH_IS("/disc/?ipbintoc")) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_DATA);
            goto client_thread_out;
        }

        /* Handle downloading disc index (raw data with standard TOC) */
        if(GET && PATH_IS("/disc/?raw")) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_RAW);
            goto client_thread_out;
        }

        /* Handle downloading disc index (raw data with IP.BIN TOC) */
        if(GET && PATH_IS("/disc/?raw_ipbintoc")) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_RAW);
            goto client_thread_out;
        }

        /* Handle downloading disc index (raw data + subchannels with standard TOC) */
        if(GET && PATH_IS("/disc/?sub")) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_SUB);
            goto client_thread_out;
        }

        /* Handle downloading disc index (raw data + subchannels with IP.BIN TOC) */
        if(GET && PATH_IS("/disc/?sub_ipbintoc")) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_SUB);
            goto client_thread_out;
        }

        /* FIXME: Clean up GDI file handling by parsing parameters
         * in the called function instead of here... */

        /* Handle downloading GDI file for disc */
        if(GET && PATH_IS("/disc/gdi")) {
            send_gdi(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_DATA, false);
            goto client_thread_out;
        }

        /* Handle downloading GDI file, 2048 ISO style */
        if(GET && PATH_IS("/disc/gdi?data")) {
            send_gdi(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_DATA, false);
            goto client_thread_out;
        }

        /* Handle downloading GDI file, 2048 ISO style with IP.BIN TOC */
        if(GET && PATH_IS("/disc/gdi?data_ipbintoc")) {
            send_gdi(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_DATA, true);
            goto client_thread_out;
        }

        /* Handle downloading GDI file, 2352 raw style */
        if(GET && PATH_IS("/disc/gdi?raw")) {
            send_gdi(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_RAW, true);
            goto client_thread_out;
        }

        /* Handle downloading GDI file, 2352 raw style with IP.BIN TOC */
        if(GET && PATH_IS("/disc/gdi?raw_ipbintoc")) {
            send_gdi(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_RAW, true);
            goto client_thread_out;
        }

        /* FIXME: Clean up track dump handling by parsing parameters
         * in the called function instead of here... */

        if(GET && PATH_STARTS_WITH("/disc/track")) {
            unsigned ipbintoc, datasel, trktype, dma, sub, abort;
            size_t session, track, secsz, secrd, gap, retry;
            char track_ext[4] = {0};
            if(sscanf(hs->path, "/disc/track%d.%c%c%c?ipbintoc%d_session%d_datasel%d_trktype%d"
                                "_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d",
                      &track, track_ext, track_ext+1, track_ext+2, &ipbintoc, &session, &datasel, &trktype,
                      &secsz, &gap, &dma, &secrd, &sub, &abort, &retry) == 15) {

                unsigned file_ext = 0;
                if(strncasecmp(track_ext, "bin", 3) == 0) {
                    file_ext = TRACK_FILE_EXT_BIN;
                } else if(strncasecmp(track_ext, "iso", 3) == 0) {
                    file_ext = TRACK_FILE_EXT_ISO;
                } else if(strncasecmp(track_ext, "raw", 3) == 0) {
                    file_ext = TRACK_FILE_EXT_RAW;
                } else {
                    DWC_LOG("file extension unknown, falling back to 'bin'\n");
                    file_ext = TRACK_FILE_EXT_BIN;
                }

                send_track(hs, ipbintoc, session, track, datasel, trktype, secsz,
                           gap, dma, secrd, sub, abort, retry, file_ext);

                goto client_thread_out;
            }
        }

    }

    /********************************
     **** VMU / Memcard Handling ****
     ********************************/

    if(PATH_STARTS_WITH("/vmu/")) {

        /* Handle sending VMU management page */
        if(GET && PATH_IS("/vmu/")) {
            send_vmu(hs);
            goto client_thread_out;
        }

        /* Handle VMU image dumping as raw */
        /* FIXME: WIP function */
        size_t port, unit;
        if(sscanf(hs->path, "/vmu/image?raw_p%du%d", &port, &unit) == 2) {
            send_vmu_image(hs, maple_enum_dev(port, unit), false);
            goto client_thread_out;
        }

        /* Handle VMU image dumping as DCM */
        /* FIXME: WIP function */
        if(sscanf(hs->path, "/vmu/image?dcm_p%du%d", &port, &unit) == 2) {
            send_vmu_image(hs, maple_enum_dev(port, unit), true);
            goto client_thread_out;
        }

        /* Handle sending VMU management page */
        if(POST && PATH_IS("/vmu/upload")) {
            send_error(hs, 404, "VMU upload functionality is currently unimplemented. Come back later for more.");
            goto client_thread_out;
        }

    }

    /********************************
     ******* SD Card Handling *******
     ********************************/

    if(PATH_STARTS_WITH("/sd/")) {

        if(GET && PATH_IS("/sd/")) {
            send_error(hs, 404, "SD page is currently unimplemented. Come back later for more.");
            goto client_thread_out;
        }

    }

    /********************************
     ***** Hard Drive Handling ******
     ********************************/

    if(PATH_STARTS_WITH("/hdd/")) {

        if(GET && PATH_IS("/hdd/")) {
            send_error(hs, 404, "HDD page is currently unimplemented. Come back later for more.");
            goto client_thread_out;
        }

    }

    /********************************
     ***** Miscellaneous Files ******
     ********************************/

    /* Handle client downloading source code */
    if(GET && (PATH_IS("/dwc-source-" VERSION ".zip") ||
              (PATH_IS("/source.zip")))) {
        send_fs(hs, "/rd/dwc-source-" VERSION ".zip");
        goto client_thread_out;
    }

    /* We can have fun, too, can't we? */
    if(GET && PATH_IS("/brew_coffee")) {
        send_error(hs, 418, "I'm a teapot!");
        goto client_thread_out;
    }

    /********************************
     ******** Legacy Paths **********
     ********************************/

    if(GET && (PATH_IS("/gdrom_spin_down") ||
               PATH_IS("/cdrom_spin_down"))) {
        gdrom_spin_down(hs);
        goto client_thread_out;
    }

    /* Handle downloading BIOS image */
    if(GET && PATH_IS("/dc_bios.bin")) {
        send_memory(hs, 0xA0000000, 0xA01FFFFF, "dc_bios.bin");
        goto client_thread_out;
    }

    /* Handle downloading flashrom image */
    if(GET && PATH_IS("/dc_flash.bin")) {
        send_memory(hs, 0xA0200000, 0xA021FFFF, "dc_flash.bin");
        goto client_thread_out;
    }

    /* Handle downloading syscalls image */
    if(GET && PATH_IS("/syscalls.bin")) {
        send_memory(hs, 0xAC000000, 0xAC007FFF, "syscalls.bin");
        goto client_thread_out;
    }

    /* Handle all other requests -- we shouldn't get
     * here unless all conditionals above failed to
     * find an adequate handler for the request */
    send_error(hs, 404, "Invalid request or file not found.");

    client_thread_out:

    DWC_LOG("httpd: closed connection on socket %d\n", hs->socket);

#ifdef MALLOC_STATS
    printf("Finished with thread on socket %d...\n", hs->socket);
    malloc_stats();
#endif

    /* buf should already be freed and NULL, but just in case */
    FREE(buf);

    /* Clean up our http state and all associated allocated memory */
    close(hs->socket);
    FREE(hs->path);
    FREE(hs);

    /* We're now done with this thread. */
    return NULL;
}
