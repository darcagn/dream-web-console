#include "common.h"

void send_main(http_state_t *hs) {
    WEBPAGE_START("Welcome to Dream Web Console!");

    WEBPAGE_WRITE("<h1>Welcome to the Dream Web Console!</h1>\n");
    WEBPAGE_WRITE("<p><a href=\"/diagnostics\">Diagnostic Information</a></p>\n");
    WEBPAGE_WRITE("<br />\n");
    WEBPAGE_WRITE("<p><a href=\"/disc_index\">Dump disc (standard data format): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc_index?ipbintoc\">[IP.BIN TOC]</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/disc_index_raw\">Dump disc (TOSEC raw format): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc_index_raw?ipbintoc\">[IP.BIN TOC]</a>\n");
    WEBPAGE_WRITE("<p><a href=\"/disc_index_sub\">Dump disc (raw + subchannels): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc_index_sub?ipbintoc\">[IP.BIN TOC]</a>\n");
    WEBPAGE_WRITE("<br />\n");
    WEBPAGE_WRITE("<p><a href=\"/gdrom_spin_down\">Spin down GD-ROM drive</a></p>\n");
    WEBPAGE_WRITE("<br />\n");
    WEBPAGE_WRITE("<p><a href=\"/index_vmu\">Browse VMUs</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/index_sd\">Browse SD card</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/index_hdd\">Browse hard drive</a></p>\n");
    WEBPAGE_WRITE("<br />\n");
    WEBPAGE_WRITE("<p><a href=\"/dc_bios.bin\">BIOS image</a> - (0x00000000 - 0x001FFFFF)</p>");
    WEBPAGE_WRITE("<p><a href=\"/dc_flash.bin\">FlashROM image</a> - (0x00200000 - 0x0021FFFF)</p>");
    WEBPAGE_WRITE("<p><a href=\"/syscalls.bin\">Syscalls</a> - (0x8C000000 - 0x8C007FFF)</p>");
    WEBPAGE_WRITE("<br />\n");
    WEBPAGE_WRITE("<p><a href=\"dwc-source-%s.zip\">Download Dreamcast Web Console source code</a></p>", VERSION);

    WEBPAGE_FINISH();
}
