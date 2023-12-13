#include "common.h"

#define VMU_BLOCK_SIZE  512
#define VMU_BLOCK_COUNT 256

void send_vmu_image(http_state_t *hs, maple_device_t *vmu_dev, bool dcm) {
    DWC_LOG("Sending VMU image on socket %d\n", hs->socket);

    uint8_t *buf = calloc(VMU_BLOCK_COUNT, VMU_BLOCK_SIZE);
    if(!buf) {
        send_error(hs, 404, "ERROR: malloc failure in send_vmu_image()");
        DWC_LOG("malloc failure in send_vmu_image(), socket %d\n", hs->socket);
        return;
    }

    for(size_t i = 0; i < VMU_BLOCK_COUNT; ++i) {
        if(!vmu_dev) {
            DWC_LOG("ERROR: VMU not connected!\n");
            send_error(hs, 404, "VMU not connected!");
            goto send_out;
        } else {
            if(vmu_block_read(vmu_dev, i, buf+(i * VMU_BLOCK_SIZE)) != MAPLE_EOK) {
                DWC_LOG("Error reading block %d from VMU!\n", i);
                send_error(hs, 404, "Error reading block from VMU!");
                goto send_out;
            }
        }
    }

    /* Nexus DCM dumps are raw VMU dumps where each 4-byte chunk is
     * reversed: i.e. 01234567 becomes 32107654 */
    if(dcm) {
        DWC_LOG("Sending VMU image in Nexus format\n");
        for(uint8_t *offset = buf;
            offset < buf + (VMU_BLOCK_SIZE * VMU_BLOCK_COUNT);
            offset += 4) {

            /* Swap 4th byte with 1st */
            uint8_t tmp = *offset;
            *offset = *(offset + 3);
            *(offset + 3) = tmp;

            /* Swap 2nd byte with 3rd */
            tmp = *(offset + 1);
            *(offset + 1) = *(offset + 2);
            *(offset + 2) = tmp;
        }
    }

    send_ok(hs, "application/octet-stream", (VMU_BLOCK_SIZE * VMU_BLOCK_COUNT),
            (dcm ? "vmu_image.dcm" : "vmu_image.bin"), true);
    uint8_t *cursor = buf;
    size_t output_size = (VMU_BLOCK_COUNT * VMU_BLOCK_SIZE);
    while(output_size > 0) {
        int rv = write(hs->socket, cursor, output_size);
        if (rv <= 0)
            goto send_out;
        output_size -= rv;
        cursor += rv;
    }
    send_out:
    FREE(buf);
}

void send_vmu(http_state_t *hs) {
    maple_device_t *memcard = NULL;
    WEBPAGE_START("Dream Web Console - VMU &amp; Memory Card Management");
    WEBPAGE_WRITE("<h1>Dream Web Console - VMU &amp; Memory Card Management</h1>\n");
    WEBPAGE_WRITE("<p>Browse or transfer data with VMUs and memory cards. Raw dumps, VMS files, and Nexus DCI/DCM files supported.</p>\n<ul>\n");

    WEBPAGE_WRITE("<li>Port A:\n\t<ul>\n");
    memcard = maple_enum_dev(0, 1);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD)) {
            WEBPAGE_WRITE("\t\t<li>Slot 1:");

            WEBPAGE_WRITE("<table>");
            WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">Filename</td>"
                          "<td colspan=\"1\" class=\"top\">Bytes</td>"
                          "<td colspan=\"1\" class=\"top\">Download</td></tr>");

            char *path = "/vmu/a1/";
            file_t fd = fs_open(path, O_RDONLY | O_DIR);
            if(fd < 0) {
                WEBPAGE_WRITE("<tr><td colspan=\"3\">Error showing file list!</li>");
            } else {
                dirent_t *d;
                while((d = fs_readdir(fd))) {
                    WEBPAGE_WRITE("<tr><td>%s</td><td>%d</td><td><a href=\"/?%s%s\">VMS</a> <a href=\"VMI\">VMI</a> <a href=\"DCI\">DCI</a></td></tr>", d->name, d->size, path, d->name);
                }
            }
            fs_close(fd);

            WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">vmu_image.bin</td><td colspan=\"1\" class=\"top\">128K</td><td colspan=\"1\" class=\"top\"><a href=\"/vmu/image?raw_p0u1\">Raw BIN</a></td></tr>");
            WEBPAGE_WRITE("<tr cellpadding=\"2\"><td class=\"top\">vmu_image.dcm</td><td colspan=\"1\" class=\"top\">128K</td><td colspan=\"1\" class=\"top\"><a href=\"/vmu/image?dcm_p0u1\">Nexus DCM</a></td></tr>");
            WEBPAGE_WRITE("<tr><td colspan=\"3\" style=\"text-align: center; vertical-align: middle;\">");
            WEBPAGE_WRITE("<form style=\"display: flex;\" action=\"/vmu/upload\" method=\"post\" enctype=\"multipart/form-data\">");
            WEBPAGE_WRITE("    <input style=\"padding: 5px;\" type=\"file\" id=\"file\" name=\"file\" accept=\".bin, .vms, .dci, .dcm\">");
            WEBPAGE_WRITE("    <input style=\"padding: 5px;\" type=\"submit\" value=\"Upload\">");
            WEBPAGE_WRITE("</form>\n");
            WEBPAGE_WRITE("</td></tr>");
            WEBPAGE_WRITE("</table>");

            WEBPAGE_WRITE("</li>");
        } else {
            WEBPAGE_WRITE("\t\t<li>Slot 1: no memory card device inserted</li>\n");
        }


    /* Ugly copy/paste code for A2-D2 maple devices, once we're done above we'll just do a loop here */

    memcard = maple_enum_dev(0, 2);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 2: <a href=\"/?/vmu/a2/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p0u2\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p0u2\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 2: no memory card device inserted</li>\n");
    WEBPAGE_WRITE("\t</ul>\n</li>\n");


    WEBPAGE_WRITE("<li>Port B:\n\t<ul>\n");
    memcard = maple_enum_dev(1, 1);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 1: <a href=\"/?/vmu/b1/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p1u1\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p1u1\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 1: no memory card device inserted</li>\n");

    memcard = maple_enum_dev(1, 2);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 2: <a href=\"/?/vmu/b2/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p1u2\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p1u2\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 2: no memory card device inserted</li>\n");
    WEBPAGE_WRITE("\t</ul>\n</li>\n");


    WEBPAGE_WRITE("<li>Port C:\n\t<ul>\n");
    memcard = maple_enum_dev(2, 1);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 1: <a href=\"/?/vmu/c1/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p2u1\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p2u1\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 1: no memory card device inserted</li>\n");

    memcard = maple_enum_dev(2, 2);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 2: <a href=\"/?/vmu/c2/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p2u2\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p2u2\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 2: no memory card device inserted</li>\n");
    WEBPAGE_WRITE("\t</ul>\n</li>\n");


    WEBPAGE_WRITE("<li>Port D:\n\t<ul>\n");

    memcard = maple_enum_dev(3, 1);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 1: <a href=\"/?/vmu/d1/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p3u1\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p3u1\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 1: no memory card device inserted</li>\n");

    memcard = maple_enum_dev(3, 2);
        if(memcard && (memcard->info.functions & MAPLE_FUNC_MEMCARD))
            WEBPAGE_WRITE("\t\t<li>Slot 2: <a href=\"/?/vmu/d2/\">browse files</a>, "
                          "<a href=\"/vmu/image?raw_p3u2\">dump raw</a>, "
                          "<a href=\"/vmu/image?dcm_p3u2\">dump Nexus DCM</a></li>\n");
        else
            WEBPAGE_WRITE("\t\t<li>Slot 2: no memory card device inserted</li>\n");
    WEBPAGE_WRITE("\t</ul>\n</li>\n");
    WEBPAGE_WRITE("</ul>\n");

    WEBPAGE_FINISH();
}
