#include "../common.h"
#include "gdrom.h"

#define GDROM_IPBIN_LBA  45150
#define IPBIN_TOC_OFFSET 0xFC

unsigned get_toc(CDROM_TOC *toc, size_t session, bool ipbintoc) {

    memset(toc, 0, sizeof(CDROM_TOC));
    char ipbin[2352];

    /* Use IP.BIN TOC instead of cached drive TOC */
    if(session == 1 && ipbintoc) {
        if(cdrom_reinit_ex(-1,-1,-1) != ERR_OK) {
            conio_printf("get_toc(): Failed to reinit GD-ROM!\n");
            return ERR_SYS;
        }

        /* Read in IP.BIN */
        if(cdrom_read_sectors_ex(ipbin, GDROM_IPBIN_LBA, 1, CDROM_READ_PIO) != ERR_OK) {
            conio_printf("get_toc(): Failed to read IP.BIN!\n");
            return ERR_SYS;
        }

        /* Verify IP.BIN data is from a Dreamcast disc */
        if(strncmp("SEGA SEGAKATANA", ipbin, strlen("SEGA SEGAKATANA")) != 0) {
            conio_printf("get_toc(): Invalid IP.BIN!\n");
            return ERR_SYS;
        }

        conio_printf("get_toc(): Using IP.BIN for TOC\n");
        memcpy(toc, (ipbin + IPBIN_TOC_OFFSET), sizeof(CDROM_TOC));
        return ERR_OK;
    }

    /* Read TOC from drive (standard) */
    if(cdrom_read_toc(toc, session) != ERR_OK) {
        conio_printf("get_toc(): Failed to read TOC for session: %d\n",
                    session);
        return ERR_SYS;
    }

    return ERR_OK;
}
