#include "common.h"

void send_memory(http_state_t *hs, size_t start, size_t end) {
    conio_printf("sending memory 0x%lx - 0x%lx, socket %d\n", start, end, hs->socket);

    ptrdiff_t data_size = end - start + 1;
    if(data_size <= 0) {
        send_error(hs, 404, "End memory location before start");
        return;
    }

    send_ok(hs, "application/octet-stream", data_size);

    /* write() doesnt like memory location to be zero,
     * so send that 1 byte, then the reset */
    if(start == 0) {
        char tmp;
        memcpy(&tmp, 0, 1);
        write(hs->socket, &tmp, 1);
        data_size--;
        start++;
    }

    /* Write output to socket */
    size_t offset = 0;
    while(data_size > 0) {
        int rv = write(hs->socket, (char *)start+offset, data_size);
        if(rv <= 0) {
            return;
        }

        data_size -= rv;
        offset += rv;
    }
}
