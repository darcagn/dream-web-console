#include "../common.h"
#include "gdrom.h"

mutex_t gdrom_mutex = MUTEX_INITIALIZER;

#define GDROM_IPBIN_LBA  45150
#define IPBIN_TOC_OFFSET 0xFC

void gdrom_spin_down(http_state_t *hs) {
    if(mutex_trylock(&gdrom_mutex)) {
        send_error(hs, 409, "GD-ROM spin down refused. GD-ROM is locked by another thread!");
        DWC_LOG("httpd: Disc drive unlock refused on socket %d\n", hs->socket);
        return;
    }

    DWC_LOG("httpd: Spinning down disc drive on socket %d\n", hs->socket);
    cdrom_spin_down();

    WEBPAGE_START("Dream Web Console - Disc spin down");
    WEBPAGE_WRITE("<h1>Dream Web Console - Disc spin down</h1><hr />");
    WEBPAGE_WRITE("<p>The disc drive has been issued a spin down command.</p>\n");
    /* TODO: Implement the referrer header here,
     * so we know the page the user came from */
    WEBPAGE_WRITE("<p><a href=\"/\">Go back</a></p>\n");
    WEBPAGE_FINISH();
    mutex_unlock(&gdrom_mutex);
}

unsigned get_toc(CDROM_TOC *toc, size_t session, bool ipbintoc) {
    *toc = (CDROM_TOC){0};
    char ipbin[2352];

    /* Use IP.BIN TOC instead of cached drive TOC */
    if(session == 1 && ipbintoc) {
        if(cdrom_reinit_ex(-1,-1,-1) != ERR_OK) {
            DWC_LOG("get_toc(): Failed to reinit GD-ROM!\n");
            return ERR_SYS;
        }

        /* Read in IP.BIN */
        if(cdrom_read_sectors_ex(ipbin, GDROM_IPBIN_LBA, 1, CDROM_READ_PIO) != ERR_OK) {
            DWC_LOG("get_toc(): Failed to read IP.BIN!\n");
            return ERR_SYS;
        }

        /* Verify IP.BIN data is from a Dreamcast disc */
        if(strncmp("SEGA SEGAKATANA", ipbin, strlen("SEGA SEGAKATANA")) != 0) {
            DWC_LOG("get_toc(): Invalid IP.BIN!\n");
            return ERR_SYS;
        }

        DWC_LOG("get_toc(): Using IP.BIN for TOC\n");
        memcpy(toc, (ipbin + IPBIN_TOC_OFFSET), sizeof(CDROM_TOC));
        return ERR_OK;
    }

    /* Read TOC from drive (standard) */
    if(cdrom_read_toc(toc, session) != ERR_OK) {
        DWC_LOG("get_toc(): Failed to read TOC for session: %d\n",
                    session);
        return ERR_SYS;
    }

    return ERR_OK;
}
