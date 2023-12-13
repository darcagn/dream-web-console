#include "common.h"

void send_main(http_state_t *hs) {
    WEBPAGE_START("Welcome to Dream Web Console!");

    WEBPAGE_WRITE("<h1>Welcome to the Dream Web Console!</h1>\n");
    WEBPAGE_WRITE("<h2>System</h2>\n");
    WEBPAGE_WRITE("<p><a href=\"/system/\">System Information</a> [unimplemented]</p>\n");
    WEBPAGE_WRITE("<p><a href=\"/?/\">Browse root filesystem</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/dc_bios.bin\">Dump BIOS image</a> - ");
    WEBPAGE_WRITE("<a href=\"/dc_flash.bin\">Dump FlashROM image</a> - ");
    WEBPAGE_WRITE("<a href=\"/syscalls.bin\">Dump Syscalls</a></p>\n");
    WEBPAGE_WRITE("<br />\n");

    WEBPAGE_WRITE("<h2>Maple</h2>\n");
    WEBPAGE_WRITE("<p><a href=\"/vmu/\">VMU &amp; Memory Card Management</a></p>\n");
    WEBPAGE_WRITE("<br />\n");

    WEBPAGE_WRITE("<h2>Optical Disc</h2>\n");
    WEBPAGE_WRITE("<p><a href=\"/?/cd/\">Browse disc filesystem</a></p>");
    WEBPAGE_WRITE("<p><a href=\"/disc/\">Dump disc (standard data format): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc/?ipbintoc\">[IP.BIN TOC]</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/disc/?raw\">Dump disc (TOSEC raw format): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc/?raw_ipbintoc\">[IP.BIN TOC]</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/disc/?sub\">Dump disc (raw + subchannels): [standard TOC]</a> - ");
    WEBPAGE_WRITE("<a href=\"/disc/?sub_ipbintoc\">[IP.BIN TOC]</a></p>\n");
    WEBPAGE_WRITE("<p><a href=\"/disc/spin_down\">Spin down GD-ROM drive</a></p>\n");
    WEBPAGE_WRITE("<br />\n");

    WEBPAGE_WRITE("<h2>Storage</h2>\n");
    WEBPAGE_WRITE("<p><a href=\"/sd/\">SD Card Management</a> [unimplemented]</p>\n");
    WEBPAGE_WRITE("<p><a href=\"/hdd/\">G1 Hard Disk Management</a> [unimplemented]</p>\n");
    WEBPAGE_WRITE("<br />\n");

    WEBPAGE_WRITE("<h2>Miscellaneous</h2>\n");
    WEBPAGE_WRITE("<p><a href=\"dwc-source-%s.zip\">Download Dreamcast Web Console source code</a></p>", VERSION);
    WEBPAGE_WRITE("<br />\n");

    WEBPAGE_FINISH();
}
