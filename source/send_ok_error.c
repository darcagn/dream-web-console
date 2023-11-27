#include "common.h"

/* Send HTTP response. If len > 0, include Content-Length */
void send_ok(http_state_t *hs, const char *content_type, size_t len) {
    char buf[512];

    if(len > 0) {
        sprintf(buf, "HTTP/1.0 200 OK\r\nContent-type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", content_type, len);
    } else {
        sprintf(buf, "HTTP/1.0 200 OK\r\nContent-type: %s\r\nConnection: close\r\n\r\n", content_type);
    }

    write(hs->socket, buf, strlen(buf));
}

void send_error(http_state_t *hs, unsigned error_code, const char *error_str) {
    /* Write HTTP response header */
    char buf[512];
    sprintf(buf, "HTTP/1.0 %d %s\r\nContent-type: text/html\r\n\r\n",
            error_code, error_str);
    write(hs->socket, buf, strlen(buf));

    /* Set up stream for webpage content */
    char *output;
    size_t output_size = 0;
    FILE *page = open_memstream(&output, &output_size);
    if(!page) {
        /* If we can't even display a fancy error page due to malloc failure,
         * send a quick error straight to browser */
        sprintf(buf, "%d %s   %s", error_code, error_str,
                "...and malloc() failure when trying to present error page!");
        write(hs->socket, buf, strlen(buf));
        conio_printf("malloc failure in send_error(), socket %d\n", hs->socket);
        return;
    }

    fprintf(page, "<html><head>");
    fprintf(page, stylesheet);
    fprintf(page, "<title>Dream Web Console - Error!</title></head></html>\n<body>\n");
    fprintf(page, "<h1>Dream Web Console - Error!</h1>\n");
    fprintf(page, "<p>Error %d: %s</p>\n", error_code, error_str);
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

