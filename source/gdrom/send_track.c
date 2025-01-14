#include "../common.h"
#include "gdrom.h"

#define MAX_SECTOR_READ         128

void send_track(http_state_t *hs, bool ipbintoc, size_t session, size_t track, unsigned datasel,
                unsigned trktype, size_t secsz, size_t gap, unsigned dma, size_t secrd,
                unsigned sub, bool abort, size_t retry, unsigned track_file_ext) {

    if(mutex_trylock(&gdrom_mutex)) {
        send_error(hs, 409, "GD-ROM access refused. GD-ROM is locked by another thread!");
        DWC_LOG("httpd: Disc drive unlock refused on socket %d\n", hs->socket);
        return;
    }

    DWC_LOG("s:%d toc:%d t:%d ds:%d tt:%d ss:%d dma:%d sr:%d sub:%d retry:%d ab:%d\n",
                ipbintoc, session, track, datasel, trktype, secsz, dma, secrd,
                sub, retry, abort);

    /* We first need to set up and check parameters before operation */

    switch(datasel) {
        case DATASEL_HEADER:    datasel = 0x8000; break;
        case DATASEL_SUBHEADER: datasel = 0x4000; break;
        case DATASEL_DATA:      datasel = 0x2000; break;
        case DATASEL_OTHER:
        default:                datasel = 0x1000; break;
    }

    switch(trktype) {
        case TRACK_TYPE_UNKNOWN:     trktype = 0x0E00; break;
        case TRACK_TYPE_MODE2_NONXA: trktype = 0x0C00; break;
        case TRACK_TYPE_MODE2_FORM2: trktype = 0x0A00; break;
        case TRACK_TYPE_MODE2_FORM1: trktype = 0x0800; break;
        case TRACK_TYPE_MODE2:       trktype = 0x0600; break;
        case TRACK_TYPE_MODE1:       trktype = 0x0400; break;
        case TRACK_TYPE_CDDA:        trktype = 0x0200; break;
        case TRACK_TYPE_ANY:
        default:                     trktype = 0x0000; break;
    }

    /* Sector size must be in the range 1 to 2352 */
    if(secsz <= 0 || secsz > 2352) {
        DWC_LOG("WARN: Invalid sector size %d, forcing 2352\n", secsz);
        secsz = 2352;
    }

    if(dma != CDROM_READ_PIO)
        dma = CDROM_READ_DMA;

    /* Number of sectors to read must be in range 1 to MAX_SECTOR_READ */
    if(secrd > MAX_SECTOR_READ) {
        DWC_LOG("WARN: secrd %d > MAX, forcing %d\n", secrd, MAX_SECTOR_READ);
        secrd = MAX_SECTOR_READ;
    } else if(secrd <= 0) {
        DWC_LOG("WARN: secrd %d <= 0, forcing 1\n", secrd);
        secrd = 1;
    }

    /* Make sure sub dumping is off, or set to syscalls, or set to manual read */
    if(sub > 2) {
        DWC_LOG("WARN: invalid sub %d, forcing %d\n", sub, SUB_NONE);
        sub = SUB_NONE;
    }

    /* Correct parameters that are not compatible with sub dumping */
    if(sub) {
        if(secrd != 1) {
            DWC_LOG("WARN: sub enabled, secrd forced to 1\n");
            secrd = 1;
        }
        if(dma) {
            DWC_LOG("WARN: sub enabled, dma read disabled\n");
            dma = 0;
        }
    }

    bool sec_range = false; /* Are we dumping a track or sector range? */
    size_t sector_start;  /* The sector at which to begin dumping */
    size_t sector_end;    /* The last sector to dump */

    /* Create a pointer which will later point to our disc read buffer */
    uint8_t *buf = NULL;

    if(session >= 100 && track >= 100 && track >= session) {
        /* Allow session/track number to act as sector range start/end */
        sec_range = true;
        sector_start = session;
        sector_end = track + 1;
    } else {
        /* Determine the start/end LBA from TOC */
        CDROM_TOC toc;

        if(get_toc(&toc, session - 1, ipbintoc) != ERR_OK) {
            send_error(hs, 404, "ERROR: TOC read failed");
            goto send_track_out;
        }

        /* Check if requested track exists in the TOC */
        if(track < TOC_TRACK(toc.first) || track > TOC_TRACK(toc.last)) {
            send_error(hs, 404, "ERROR: invalid track for session");
            goto send_track_out;
        }

        sector_start = TOC_LBA(toc.entry[track-1]);
        /* Sector end is found by using the next track's start sector, or the
         * leadout sector, if selected track is the last of the session. */
        if(track == TOC_TRACK(toc.last)) {
            sector_end = TOC_LBA(toc.leadout_sector) - gap;
        } else {
            sector_end = TOC_LBA(toc.entry[track]) - gap;
        }
    }

    /* Reinitialize the GD-ROM drive with our parameters */
    if(cdrom_reinit_ex(datasel, trktype, secsz) != ERR_OK) {
        send_error(hs, 404, "ERROR: reinit failed");
        goto send_track_out;
    }

    /* Calculate total size in bytes of all sectors in the range
     * to be dumped, with current secsz setting */
    size_t track_sz = (secsz + (sub ? 96 : 0)) * (sector_end - sector_start);

    /* Dump operation parameters are now known -- allocate and set up buffer */

    /* Buffer size must be large enough for one entire iteration of our dumping loop,
     * but also have a little extra data for subcode processing, etc. */
    size_t buffer_sz = ((secsz + (sub ? 96 : 0)) * secrd) + 32;

    /* Assign the buffer pointer to an aligned buffer which will hold our entire operation */
    buf = memalign(32, buffer_sz);
    if(!buf) {
        send_error(hs, 404, "ERROR: malloc failure while sending track");
        DWC_LOG("malloc failure while sending track on socket %d\n", hs->socket);
        goto send_track_out;
    }

    /* memalign will return memory in the P1 memory map area (0x80000000-0x9FFFFFFF)
     * MEM_AREA_P2_BASE will bump it into P2 memory map area (0xA0000000-0xBFFFFFFF)
     * which disables caching to avoid any issues with DMA transfers */
    /* FIXME: Should be able to make this conditional on if DMA is being used or not? */
    uint8_t *nocache = (uint8_t *)((uintptr_t) buf | MEM_AREA_P2_BASE);

    /* Buffer is set up for dumping -- now loop over the sector range and retrieve
     * sectors in secrd-sized chunks */

    size_t sector = sector_start; /* Sector variable to hold the next LBA to be dumped */
    int rv;                       /* Scratch variable to hold various return values */

    /* Generate proper filename for track download */
    char filename[16] = {0};
    if(sec_range) {
        sprintf(filename, "sector_dump.bin");
    } else {
        switch (track_file_ext) {
            default:
            case TRACK_FILE_EXT_BIN: sprintf(filename, "track%02d.bin", track); break;
            case TRACK_FILE_EXT_ISO: sprintf(filename, "track%02d.iso", track); break;
            case TRACK_FILE_EXT_RAW: sprintf(filename, "track%02d.raw", track); break;
        }
    }

    send_ok(hs, "application/octet-stream", track_sz, filename, true);
    do { /* from sector_start to sector_end, loop to dump entire requested range in chunks */

        if(sector_end - sector < secrd) {
            secrd = sector_end - sector;
        }

        /* Size of data to be written to socket for this iteration of the loop */
        size_t loop_data_size = (secsz + (sub ? 96 : 0)) * secrd;

        /* Poison the entire buffer */
        memset(nocache, 'Q', buffer_sz);

        /* Read secrd sectors until successful or number of retries exceeded */
        size_t attempt = 0;
        do {
            if(attempt != 0)
                DWC_LOG("READ ERROR: sector: %d, retrying\n", sector);

            rv = cdrom_read_sectors_ex(nocache, sector, secrd, dma);
            attempt++;

        } while(rv != ERR_OK && attempt < retry);

        /* If abort parameter set and reading sectors never succeeded, quit */
        if(rv != ERR_OK) {
            DWC_LOG("READ ERROR: sector: %d, FAILED\n", sector);
            if(abort)
                goto send_track_out;
        }

        /* Handle sub channels */
        /* NOTE: Memory offsets in this block are under
         * the assumption that secrd will be 1!  */
        if(sub) {
            /* Subcode read call will return a 4-byte header that we don't
             * want. So let's save the last 4 bytes of previous sector read
             * on the stack and overwrite those bytes with the header; the
             * other 96 bytes will be in the correct position, and then
             * afterwards we can restore those 4 bytes we saved here */
            uint8_t overlap[4];
            memcpy(&overlap, nocache+secsz-4, 4);

            if(sub == SUB_SYSCALL) {
                /* Syscall method of getting sub channel data */

                /* Read subcode, overwriting the last 4 bytes of sector read
                 * call in order to have the subcode data in the right spot */
                rv = cdrom_get_subcode(nocache+secsz-4, 100, CD_SUB_Q_ALL);
                if(rv != ERR_OK) {
                    DWC_LOG("SUB ERROR: sector: %d, FAILED\n", sector);
                    if(abort)
                        goto send_track_out;
                }
            } else if(sub == SUB_READ_SECTOR) {
                /* cdrom_read_sectors_ex method of getting sub channel data */
                if(cdrom_change_datatype(16, trktype, secsz) != ERR_OK) {
                    DWC_LOG("SUB ERROR: failed to set datatype (p=16)\n");
                    if(abort)
                        goto send_track_out;
                }

                /* Read subcode, overwriting the last 4 bytes of sector read
                 * call in order to have the subcode data in the right spot */
                rv = cdrom_read_sectors_ex(nocache+secsz-4, sector, secrd, dma);
                if(rv != ERR_OK) {
                    DWC_LOG("SUB ERROR: sector: %d, FAILED\n", sector);
                    if(abort)
                        goto send_track_out;
                }

                /* Reset the read datatype */
                if(cdrom_change_datatype(datasel, trktype, secsz) != ERR_OK) {
                    DWC_LOG("SUB ERROR: failed to reset datatype (datasel=%d)\n", datasel);
                    if(abort)
                        goto send_track_out;
                }
            }

            /* Overwrite subcode call header with the saved 4 bytes */
            memcpy(nocache+secsz-4, &overlap, 4);
        } /* End sub channels handling */

        /* Write data for this loop out to socket */
        size_t offset = 0;
        while(loop_data_size > 0) {
            rv = write(hs->socket, nocache+offset, loop_data_size);
            if(rv <= 0)
                goto send_track_out;
            loop_data_size -= rv;
            offset += rv;
        }
        sector += secrd;
    } while(sector < sector_end);
    send_track_out:
    FREE(buf);
    mutex_unlock(&gdrom_mutex);
}
