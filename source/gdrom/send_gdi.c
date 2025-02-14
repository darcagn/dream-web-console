#include "../common.h"
#include "gdrom.h"

/* Send GDI file */
void send_gdi(http_state_t *hs, bool ipbintoc, unsigned defaulttype, bool view) {
    if(mutex_trylock(&gdrom_mutex)) {
        send_error(hs, 409, "GDI file download refused. GD-ROM is locked by another thread!");
        DWC_LOG("httpd: Disc drive unlock refused on socket %d\n", hs->socket);
        return;
    }

    DWC_LOG("sending gdi file, socket %d\n", hs->socket);
    char *output = NULL;
    size_t output_size = 0;

    /* Retrieve TOCs to create track listing */
    CDROM_TOC toc1, toc2;

    if(get_toc(&toc1, 0, 0) != ERR_OK) {
        DWC_LOG("FATAL: Failed to read toc, session %d\n", 1);
        send_error(hs, 404, "No disc in drive? (error reading session 1 TOC)");
        goto cleanup;
    }

    if(get_toc(&toc2, 1, ipbintoc) != ERR_OK) {
        DWC_LOG("FATAL: Failed to read toc, session %d\n", 2);
        send_error(hs, 404, "Not a dreamcast disc (2nd session)");
        goto cleanup;
    }

    /* Set up stream for GDI data */
    FILE *page = open_memstream(&output, &output_size);
    if(!page) {
        send_error(hs, 404, "Could not create new buffer for page");
        DWC_LOG("Error %d when creating buffer for socket %d\n", errno, hs->socket);
        goto cleanup;
    }

    /* Total track count on first line */
    fprintf(page, "%ld\r\n", TOC_TRACK(toc2.last));

    /* Remaining lines are...
     * track_num track_start track_type sector_size filename offset */
    if(defaulttype == DEFAULT_DUMP_TYPE_DATA) {
        /* Session 1 */
        for(size_t track = TOC_TRACK(toc1.first); track <= TOC_TRACK(toc1.last); track++) {
            fprintf(page, "%d %ld %ld %d track%02d.%s %d\r\n", track,
                    TOC_LBA(toc1.entry[track-1]) - 150,
                    TOC_CTRL(toc1.entry[track-1]),
                    (TOC_CTRL(toc1.entry[track-1]) == TRACK_DATA ? 2048 : 2352),
                    track,
                    (TOC_CTRL(toc1.entry[track-1]) == TRACK_DATA ? "iso" : "raw"),
                    0);
        }

        /* Session 2 */
        for(size_t track = TOC_TRACK(toc2.first); track <= TOC_TRACK(toc2.last); track++) {
            fprintf(page, "%d %ld %ld %d track%02d.%s %d\r\n", track,
                    TOC_LBA(toc2.entry[track-1]) - 150,
                    TOC_CTRL(toc2.entry[track-1]),
                    (TOC_CTRL(toc2.entry[track-1]) == TRACK_DATA ? 2048 : 2352),
                    track,
                    (TOC_CTRL(toc2.entry[track-1]) == TRACK_DATA ? "iso" : "raw"),
                    0);
        }
    } else if(defaulttype == DEFAULT_DUMP_TYPE_RAW) {
        /* Session 1 */
        for(size_t track = TOC_TRACK(toc1.first); track <= TOC_TRACK(toc1.last); track++) {
            fprintf(page, "%d %ld %ld %d track%02d.%s %d\r\n", track,
                    TOC_LBA(toc1.entry[track-1]) - 150,
                    TOC_CTRL(toc1.entry[track-1]),
                    2352, track,
                    (TOC_CTRL(toc1.entry[track-1]) == TRACK_DATA ? "bin" : "raw"),
                    0);
        }

        /* Session 2 */
        for(size_t track = TOC_TRACK(toc2.first); track <= TOC_TRACK(toc2.last); track++) {
            fprintf(page, "%d %ld %ld %d track%02d.%s %d\r\n", track,
                    TOC_LBA(toc2.entry[track-1]) - 150,
                    TOC_CTRL(toc2.entry[track-1]),
                    2352, track,
                    (TOC_CTRL(toc2.entry[track-1]) == TRACK_DATA ? "bin" : "raw"),
                    0);
        }
    }

    /* Write output to socket */
    fclose(page);
    send_ok(hs, "text/plain", -1, "disc.gdi", view);
    char *cursor = output;
    while(output_size > 0) {
        int rv = write(hs->socket, cursor, output_size);
        if (rv <= 0) {
            goto cleanup;
        }
        output_size -= rv;
        cursor += rv;
    }
    cleanup:
    FREE(output);
    mutex_unlock(&gdrom_mutex);
}
