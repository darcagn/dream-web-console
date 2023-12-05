#include "common.h"
#include "gdrom/gdrom.h"

mutex_t gdrom_mutex = MUTEX_INITIALIZER;

int readline(int sock, char *buf, size_t bufsize) {
    int r;
    size_t rt;
    char c;

    rt = 0;

    do {
        r = read(sock, &c, 1);
        if(!r)
            return -1;
        if(rt < bufsize)
            buf[rt++] = c;
    } while (c != '\n');

    buf[rt-1] = 0;

    if(buf[rt-2] == '\r')
        buf[rt-2] = 0;

    return 0;
}

int read_headers(http_state_t *hs, char *buffer, size_t bufsize) {
    char fn[256];

    for(size_t i = 0; ; i++) {
        if(readline(hs->socket, buffer, bufsize) < 0) {
            if(i > 0)
                return 0;
            else
                return -1;
        }

        if(!strlen(buffer))
            break;

        if(!i) {
            if(strncmp(buffer, "GET", 3) == 0) {
                size_t j;
                for(j = 4; buffer[j] && buffer[j] != 32 && j < 256; j++) {
                    fn[j-4] = buffer[j];
                }

                fn[j-4] = 0;
            }
        }
    }

    strcpy(buffer, fn);
    return 0;
}

/* Client threads start here */
#define HEADER_BUFSIZE 1024
void *client_thread(void *p) {
    http_state_t *hs = (http_state_t *)p;
    DWC_LOG("httpd: client thread started, socket %d\n", hs->socket);

    char *buf = malloc(HEADER_BUFSIZE);
    if(buf == NULL) {
        DWC_LOG("httpd: malloc failure in client_thread(), socket %d\n",
                     hs->socket);
        goto client_thread_out;
    }

    if(read_headers(hs, buf, HEADER_BUFSIZE) < 0) {
        goto client_thread_out;
    }

    DWC_LOG("httpd: client request '%s', socket %d\n", buf, hs->socket);

    unsigned ipbintoc, datasel, trktype, dma, sub, abort;
    size_t session, track, secsz, secrd, gap, retry;
    size_t memory_start, memory_end;

    if( /* Deal with requests... */
        /* Command to dump a data track as *.bin */
        (sscanf(buf, "/track%d.bin?ipbintoc%d_session%d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d",
        &track, &ipbintoc, &session, &datasel, &trktype, &secsz, &gap,
        &dma, &secrd, &sub, &abort, &retry) == 12) ||

        /* Command to dump a data track as *.iso */
        (sscanf(buf, "/track%d.iso?ipbintoc%d_session%d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d",
        &track, &ipbintoc, &session, &datasel, &trktype, &secsz, &gap,
        &dma, &secrd, &sub, &abort, &retry) == 12) ||

        /* Command to dump an audio track as *.raw */
        (sscanf(buf, "/track%d.raw?ipbintoc%d_session%d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d",
                &track, &ipbintoc, &session, &datasel, &trktype, &secsz, &gap,
                &dma, &secrd, &sub, &abort, &retry) == 12))
    { /* Handle dumping a track */
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_track(hs, ipbintoc, session, track, datasel, trktype, secsz,
                       gap, dma, secrd, sub, abort, retry);
            mutex_unlock(&gdrom_mutex);
        } else {
                send_error(hs, 404, "Track dump refused, cdrom is locked by another thread");
        }

    /* Handle source code download requests */
    } else if(strcmp(buf, "/dwc-source-" VERSION ".zip") == 0) {
        send_fsfile(hs, "/rd/dwc-source-" VERSION ".zip");
    } else if(strcmp(buf, "/source.zip") == 0) {
        send_fsfile(hs, "/rd/dwc-source-" VERSION ".zip");

    /* Handle downloading memory ranges */
    } else if(sscanf(buf, "/memory_start%u_end%u.bin", &memory_start, &memory_end) == 2) {
        send_memory(hs, memory_start, memory_end);

    /* Handle downloading GDI file, 2048 ISO style */
    } else if((strcmp(buf, "/disc.gdi") == 0) ||
                (strcmp(buf, "/disc.gdi?data") == 0)) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_gdi(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_DATA);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "disc.gdi refused, GD-ROM is locked by another thread!");
        }

    /* Handle downloading GDI file, 2048 ISO style with IP.BIN TOC */
    } else if(strcmp(buf, "/disc.gdi?data_ipbintoc") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_gdi(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_DATA);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "disc.gdi refused, GD-ROM is locked by another thread!");
        }

    /* Handle downloading GDI file, 2352 raw style */
    } else if(strcmp(buf, "/disc.gdi?raw") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_gdi(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_RAW);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "disc.gdi refused, GD-ROM is locked by another thread!");
        }

    /* Handle downloading GDI file, 2352 raw style with IP.BIN TOC */
    } else if(strcmp(buf, "/disc.gdi?raw_ipbintoc") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_gdi(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_RAW);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "disc.gdi refused, GD-ROM is locked by another thread!");
        }

    /* Handle downloading BIOS image */
    } else if(strcmp(buf, "/dc_bios.bin") == 0) {
        send_memory(hs, 0xA0000000, 0xA01FFFFF);

    /* Handle downloading flashrom image */
    } else if(strcmp(buf, "/dc_flash.bin") == 0) {
        send_memory(hs, 0xA0200000, 0xA021FFFF);

    /* Handle downloading syscalls image */
    } else if(strcmp(buf, "/syscalls.bin") == 0) {
        send_memory(hs, 0xAC000000, 0xAC007FFF);

    /* Handle gdrom/cdrom spin down command */
    } else if(strcmp(buf, "/gdrom_spin_down") == 0 ||
            strcmp(buf, "/cdrom_spin_down") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            cdrom_spin_down();
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "GD-ROM spin down refused, GD-ROM is locked by another thread!");
        }

    /* Handle sending a main page */
    } else if(strcmp(buf, "/") == 0 || strcmp(buf, "/main_page") == 0) {
        send_main(hs);

    /* Handle sending VMU management page */
    } else if(strcmp(buf, "/index_vmu") == 0) {
        send_vmu(hs);

    /* Handle sending SD management page */
    } else if(strcmp(buf, "/index_sd") == 0) {
        send_sd(hs);

    /* Handle sending HDD management page */
    } else if(strcmp(buf, "/index_hdd") == 0) {
        send_hdd(hs);

    /* Handle downloading disc index (standard data with standard TOC) */
    } else if(strcmp(buf, "/disc_index") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_DATA);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle downloading disc index (standard data with IP.BIN TOC */
    } else if(strcmp(buf, "/disc_index?ipbintoc") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_DATA);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle downloading disc index (raw data with standard TOC) */
    } else if(strcmp(buf, "/disc_index_raw") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_RAW);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle downloading disc index (raw data with IP.BIN TOC) */
    } else if(strcmp(buf, "/disc_index_raw?ipbintoc") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_RAW);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle downloading disc index (raw data + subchannels with standard TOC) */
    } else if(strcmp(buf, "/disc_index_sub") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_DEFAULT, DEFAULT_DUMP_TYPE_SUB);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle downloading disc index (raw data + subchannels with IP.BIN TOC) */
    } else if(strcmp(buf, "/disc_index_sub?ipbintoc") == 0) {
        if(mutex_trylock(&gdrom_mutex) == 0) {
            send_disc_index(hs, TOC_TYPE_IPBIN, DEFAULT_DUMP_TYPE_SUB);
            mutex_unlock(&gdrom_mutex);
        } else {
            send_error(hs, 404, "TOC list refused, GD-ROM locked by another thread!");
        }

    /* Handle diagnostics page */
    } else if(strcmp(buf, "/diagnostics") == 0) {
        send_diag(hs);

    /* Handle invalid requests */
    } else {
        send_error(hs, 404, "Invalid request or file not found.");
    }

    client_thread_out:

    DWC_LOG("httpd: closed connection, socket %d\n", hs->socket);

#ifdef MALLOC_STATS
    printf("Finished with thread on socket %d...\n", hs->socket);
    malloc_stats();
#endif

    FREE(buf);
    close(hs->socket);
    FREE(hs);
    return NULL;
}
