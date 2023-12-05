#include "../common.h"
#include "gdrom.h"

typedef struct {
    char name[24];
    size_t start;
    size_t end;
} cdrom_info_t;

cdrom_info_t cdrom_info[] = {
    { "Title",             0x80, 0xff },
    { "Media ID",          0x20, 0x24 },
    { "Media Config",      0x25, 0x2f },
    { "Regions",           0x30, 0x37 },
    { "Peripheral String", 0x38, 0x3f },
    { "Product Number",    0x40, 0x49 },
    { "Version",           0x4A, 0x4f },
    { "Release Date",      0x50, 0x5f },
    { "Manufacturer ID",   0x70, 0x7f },
};

void trim_cdrom_info(char *input, char *output, size_t size) {
    char *p = input;
    char *o = output;

    /* Skip any spaces at the beginning */
    while(*p == ' ' && input + size > p)
        p++;

    /* Copy rest to output buffer */
    while(input + size > p) {
        *o = *p;
        p++;
        o++;
    }

    /* Make sure output buffer is null terminated */
    *o = '\0';
    o--;

    /* Remove trailing spaces */
    while(*o == ' ' && o > output) {
        *o='\0';
        o--;
    }
}

void send_disc_index(http_state_t *hs, bool ipbintoc, unsigned defaulttype) {
    WEBPAGE_START("Dream Web Console - Dump Disc");
    WEBPAGE_WRITE("<h1>Dream Web Console - Listing of GD-ROM</h1>\n"
                  "<hr>\n<table width=\"640\">\n");

    /* Reinitialize drive */
    if(cdrom_reinit_ex(-1,-1,-1) != ERR_OK) {
        WEBPAGE_WRITE("ERROR: cdrom_reinit_ex() failed");
        goto send_toc_out;
    }

    /* Write GD-ROM info */
    WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">GD-ROM</td>"
                  "<td colspan=\"3\" class=\"top\">INFO</td></tr>\n");

    char sector[2352], info[129];
    bool sendgdi = false;
    if(cdrom_read_sectors_ex(sector, 45150, 1, CDROM_READ_PIO) == ERR_OK) {
        /* Verify that we have a Dreamcast GD-ROM */
        if(strncasecmp("SEGA SEGAKATANA", sector, strlen("SEGA SEGAKATANA")) == 0) {

            /* The GDI format cannot hold sub channels,
             * so omit it if that's our setting */
            if(defaulttype != DEFAULT_DUMP_TYPE_SUB)
                sendgdi = true;

            for(size_t i = 0; i < sizeof(cdrom_info) / sizeof(cdrom_info_t);i++) {
                trim_cdrom_info(sector + cdrom_info[i].start, info,
                                cdrom_info[i].end - cdrom_info[i].start + 1);
                WEBPAGE_WRITE("<tr><td>%s</td><td colspan=\"3\">%s</td></tr>\n",
                              cdrom_info[i].name, info);
            }
            WEBPAGE_WRITE("<tr><td>Dump Preset:</td><td colspan=\"3\">");
            switch(defaulttype) {
                case DEFAULT_DUMP_TYPE_DATA:
                    WEBPAGE_WRITE("Standard data (2048 bytes per sector)"
                                  "<br />Using %s</td></tr>\n",
                                  (ipbintoc ? "using IP.BIN TOC" : "standard TOC"));
                    break;
                case DEFAULT_DUMP_TYPE_RAW:
                    WEBPAGE_WRITE("Raw data (2352 bytes per sector)"
                                  "<br />Using %s</td></tr>\n",
                                  (ipbintoc ? "using IP.BIN TOC" : "standard TOC"));
                    break;
                case DEFAULT_DUMP_TYPE_SUB:
                    WEBPAGE_WRITE("Raw data + sub channels (2448 bytes per sector)"
                                  "<br />Using %s</td></tr>\n",
                                  (ipbintoc ? "using IP.BIN TOC" : "standard TOC"));
                    break;
            }
        } else {
            WEBPAGE_WRITE("<tr><td colspan=\"4\">"
                          "WARN: Not a Dreamcast disc, bad header signature!"
                          "</td></td>\n");
        }
    } else {
        WEBPAGE_WRITE("<tr><td colspan=\"4\">"
                      "WARN: Not a Dreamcast disc, read failed!"
                      "</td></td>\n");
    }

    WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">Track</td>"
                  "<td align=\"right\" class=\"top\">Start Sector</td>"
                  "<td align=\"right\" class=\"top\">End Sector</td>"
                  "<td align=\"right\" class=\"top\">Track Size</td></tr>");

    CDROM_TOC toc;
    size_t session, track, track_size, track_start, track_end, gap;
    unsigned track_type;

    for(session = 0; session < 2; session++) {
        if(get_toc(&toc, session, (session == 1 ? ipbintoc : 0)) != ERR_OK) {
            DWC_LOG("WARN: Failed to read TOC for session %d\n", session + 1);
            continue;
        }

        for(track = TOC_TRACK(toc.first);track <= TOC_TRACK(toc.last); track++) {
            track_type = TOC_CTRL(toc.entry[track-1]);
            track_start = TOC_LBA(toc.entry[track-1]);

            gap = 0;
            if(track == TOC_TRACK(toc.last)) {
                track_end = TOC_LBA(toc.leadout_sector) - 1;
            } else {

                /* Set 150-sector gap between tracks of different types */
                if(track_type != TOC_CTRL(toc.entry[track])) {
                    track_end = TOC_LBA(toc.entry[track]) - 1;
                    gap = 150;
                    track_end -= gap;
                } else {
                    track_end = TOC_LBA(toc.entry[track]) - 1;
                }
            }

            /* Generate link information */
            switch (defaulttype) {
                case DEFAULT_DUMP_TYPE_DATA:
                    track_size = (track_type == TRACK_DATA ? 2048 : 2352) * (track_end - track_start + 1);
                    WEBPAGE_WRITE("<tr><td><a href=\"track%02d.%s?ipbintoc%d_session%02d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d\">",
                            track, (track_type == TRACK_DATA ? "iso" : "raw"),
                            ipbintoc, session + 1,
                            (track_type == TRACK_DATA ? DATASEL_DATA : DATASEL_OTHER),
                            (track_type == TRACK_DATA ? TRACK_TYPE_MODE1 : TRACK_TYPE_CDDA),
                            (track_type == TRACK_DATA ? 2048 : 2352),
                            gap, 1, 16, 0, 1, 5);
                    WEBPAGE_WRITE("track%02d.%s</a></td><td align=\"right\">%d</td><td align=\"right\">%d</td><td align=\"right\">%d</td></tr>\n",
                            track, (track_type == TRACK_DATA ? "iso" : "raw"),
                            track_start, track_end, track_size);
                    break;
                case DEFAULT_DUMP_TYPE_RAW:
                    track_size = 2352 * (track_end - track_start + 1);
                    WEBPAGE_WRITE("<tr><td><a href=\"track%02d.%s?ipbintoc%d_session%02d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d\">",
                            track, (track_type == TRACK_DATA ? "bin" : "raw"),
                            ipbintoc, session + 1,
                            DATASEL_OTHER,
                            TRACK_TYPE_ANY,
                            2352,
                            gap, 1, 16, 0, 1, 5);
                    WEBPAGE_WRITE("track%02d.%s</a></td>"
                                  "<td align=\"right\">%d</td>"
                                  "<td align=\"right\">%d</td>"
                                  "<td align=\"right\">%d</td></tr>\n",
                                  track, (track_type == TRACK_DATA ? "bin" : "raw"),
                                  track_start, track_end, track_size);
                    break;
                case DEFAULT_DUMP_TYPE_SUB:
                    track_size = 2448 * (track_end - track_start + 1);
                    WEBPAGE_WRITE("<tr><td><a href=\"track%02d.%s?ipbintoc%d_session%02d_datasel%d_trktype%d_secsz%d_gap%d_dma%d_secrd%d_sub%d_abort%d_retry%d\">",
                            track, (track_type == TRACK_DATA ? "bin" : "raw"),
                            ipbintoc, session + 1,
                            DATASEL_OTHER,
                            TRACK_TYPE_ANY,
                            2352,
                            gap, 0, 1, 1, 1, 5);
                    WEBPAGE_WRITE("track%02d.%s</a></td>"
                                  "<td align=\"right\">%d</td>"
                                  "<td align=\"right\">%d</td>"
                                  "<td align=\"right\">%d</td></tr>\n",
                                  track, (track_type == TRACK_DATA ? "bin" : "raw"),
                                  track_start, track_end, track_size);
                    break;
            }
        }
    }

    /* Create a misc section for GDI file */
    if(sendgdi) {
        WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">Misc</td>"
                      "<td colspan=\"3\" class=\"top\">Info</td></tr>");
        switch (defaulttype) {
            case DEFAULT_DUMP_TYPE_DATA:
                WEBPAGE_WRITE("<tr><td><a href=\"disc.gdi%s\">disc.gdi</a></td>"
                              "<td colspan=\"3\">GDI file (2048-sector ISO format)</td></tr>",
                              (ipbintoc ? "?data_ipbintoc" : "?data"));
                break;
            case DEFAULT_DUMP_TYPE_RAW:
                WEBPAGE_WRITE("<tr><td><a href=\"disc.gdi%s\">disc.gdi</a></td>"
                              "<td colspan=\"3\">GDI file (2352-sector BIN format)</td></tr>",
                              (ipbintoc ? "?raw_ipbintoc" : "?raw"));
                break;
            case DEFAULT_DUMP_TYPE_SUB:
                break;
        }
    }

    send_toc_out:

    WEBPAGE_WRITE("</table>");

    WEBPAGE_FINISH();
}
