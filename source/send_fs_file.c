#include "common.h"

#define FSFILE_BUFFER        32768

void send_fsfile(http_state_t *hs, char *file) {
    DWC_LOG("sending fsfile %s, socket %d\n", file, hs->socket);

    file_t fd = fs_open(file, O_RDONLY);
    if(fd < 0) {
        send_error(hs, 404, "File not found or unreadable");
        return;
    }

    char *output = malloc(FSFILE_BUFFER);
    if(!output) {
        send_error(hs, 404, "ERROR: malloc failure in send_fsfile()");
        DWC_LOG("malloc failure in send_fsfile(), socket %d\n", hs->socket);
        return;
    }

    /* Write output to socket */
    send_ok(hs, "application/octet-stream", -1);
    int count;
    while((count = fs_read(fd, output, FSFILE_BUFFER))) {
        size_t offset = 0;
        while(count > 0) {
            int rv = write(hs->socket, output+offset, count);
            if(rv <= 0)
                goto send_out;
            count -= rv;
            offset += rv;
        }
    }
    send_out:
    fs_close(fd);
    FREE(output);
}
