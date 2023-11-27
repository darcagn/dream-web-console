#include "common.h"

void send_main(http_state_t *hs) {
    /* Set up stream for webpage content */
    char *output;
    size_t output_size = 0;
    FILE *page = open_memstream(&output, &output_size);
    if(!page) {
        send_error(hs, 404, "Could not create new buffer for page");
        conio_printf("Error %d when creating buffer for socket %d\n", errno, hs->socket);
        return;
    }

    /* Write webpage content to stream */
    fprintf(page, "<html><head>");
    fprintf(page, stylesheet);
    fprintf(page, "<title>Welcome to Dream Web Console!</title></head></html>\n<body>\n");
    fprintf(page, "<h1>Welcome to the Dream Web Console!</h1>\n");
    fprintf(page, "<p><a href=\"/diagnostics\">Diagnostic Information</a></p>\n");
    fprintf(page, "<br />\n");
    fprintf(page, "<p><a href=\"/disc_index\">Dump disc (standard data format): [standard TOC]</a> - ");
    fprintf(page, "<a href=\"/disc_index?ipbintoc\">[IP.BIN TOC]</a></p>\n");
    fprintf(page, "<p><a href=\"/disc_index_raw\">Dump disc (TOSEC raw format): [standard TOC]</a> - ");
    fprintf(page, "<a href=\"/disc_index_raw?ipbintoc\">[IP.BIN TOC]</a>\n");
    fprintf(page, "<p><a href=\"/disc_index_sub\">Dump disc (raw + subchannels): [standard TOC]</a> - ");
    fprintf(page, "<a href=\"/disc_index_sub?ipbintoc\">[IP.BIN TOC]</a>\n");
    fprintf(page, "<br />\n");
    fprintf(page, "<p><a href=\"/gdrom_spin_down\">Spin down GD-ROM drive</a></p>\n");
    fprintf(page, "<br />\n");
    fprintf(page, "<p><a href=\"/index_vmu\">Browse VMUs</a></p>\n");
    fprintf(page, "<p><a href=\"/index_sd\">Browse SD card</a></p>\n");
    fprintf(page, "<p><a href=\"/index_hdd\">Browse hard drive</a></p>\n");
    fprintf(page, "<br />\n");
    fprintf(page, "<p><a href=\"/dc_bios.bin\">BIOS image</a> - (0x00000000 - 0x001FFFFF)</p>");
    fprintf(page, "<p><a href=\"/dc_flash.bin\">FlashROM image</a> - (0x00200000 - 0x0021FFFF)</p>");
    fprintf(page, "<p><a href=\"/syscalls.bin\">Syscalls</a> - (0x8C000000 - 0x8C007FFF)</p>");
    fprintf(page, "<br />\n");
    fprintf(page, "<p><a href=\"dwc-source-" VERSION ".zip\">Download Dreamcast Web Console source code</a></p>");
    fprintf(page, html_footer);

    /* Write output to socket */
    fclose(page);
    send_ok(hs, "text/html", -1);
    char *cursor = output;
    while(output_size > 0) {
        int rv = write(hs->socket, cursor, output_size);
        if (rv <= 0)
            goto send_out;
        output_size -= rv;
        cursor += rv;
    }
    send_out:
    free(output);
    output = NULL;
}
