#include "common.h"

/* Send HTTP 200 OK header. */
void send_ok(http_state_t *hs, const char *content_type,
             size_t len, const char *filename, bool attachment) {
    char *output;
    size_t output_size = 0;
    FILE *header = open_memstream(&output, &output_size);

    fprintf(header, "HTTP/1.1 200 OK\r\nContent-type: %s\r\n", content_type);

    /* Add Content-Length header, if specified */
    if(len > 0) {
        fprintf(header, "Content-Length: %d\r\n", len);
    }

    /* Add Content-Disposition */
    fprintf(header, "Content-Disposition: %s",
            (attachment ? "attachment" : "inline"));

    if(filename) {
        fprintf(header, "; filename=\"%s\"", filename);
    }

    fprintf(header, "\r\n");

    /* Add Connection close */
    fprintf(header, "Connection: close\r\n");

    /* Finish header */
    fprintf(header, "\r\n");

    fclose(header);
    write(hs->socket, output, output_size);
    FREE(output);
}

/* Send HTTP error page. */
void send_error(http_state_t *hs, unsigned error_code, const char *error_str) {
    /* Write HTTP response header */
    /* FIXME: Can this be sized better? */
    char buf[256];
    sprintf(buf, "HTTP/1.1 %d %s\r\nContent-type: text/html\r\n\r\n",
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
        DWC_LOG("malloc failure in send_error(), socket %d\n", hs->socket);
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
    char *cursor = output;
    while(output_size > 0) {
        int rv = write(hs->socket, cursor, output_size);
        if (rv <= 0)
            goto send_out;
        output_size -= rv;
        cursor += rv;
    }
    send_out:
    FREE(output);
}
