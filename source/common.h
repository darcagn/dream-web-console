#ifndef DWC_COMMON_H
#define DWC_COMMON_H

#include <kos.h>
#include <conio/conio.h>
#include <errno.h>
#include <stdbool.h>

typedef struct http_state {
    int                 socket;
    struct sockaddr_in  client;
    socklen_t           client_size;
    kthread_t           *thd;
} http_state_t;

/* Stylesheets and HTML elements */

extern const char *stylesheet;
extern const char *html_footer;

/* Create a new thread for each client connection */
void *client_thread(void *p);

/* Send HTTP OK and Error headers */
void send_ok(http_state_t *hs, const char *content_type, size_t len);
void send_error(http_state_t *hs, unsigned error_code, const char *error_str);

/* Send main index page */
void send_main(http_state_t *hs);

/* Send diagnostics page */
void send_diag(http_state_t *hs);

/* Send VMU management page */
void send_vmu(http_state_t *hs);

/* Send SD management page */
void send_sd(http_state_t *hs);

/* Send HDD management page */
void send_hdd(http_state_t *hs);

/* Send a memory region */
void send_memory(http_state_t *hs, size_t start, size_t end);

/* Send file from romdisk.img
 * FIXME: In httpd-ack source code from 2008,
 *        this is written:
 * > for some reason this file is causing a crash
 * > when (hs->socket); is called later on
 * > maybe something to do with fs_open/close?
 * Need to investigate if this is still a problem,
 * or if a KOS bug was fixed, or if our refactoring
 * fixed the issue, etc... */

void send_fsfile(http_state_t *hs, char *file);

#define FREE(x) \
    free(x); \
    x = NULL;

#define DWC_LOG(...) conio_printf(__VA_ARGS__)

#define WEBPAGE_START(x) \
    char *dwc__page_output; \
    size_t dwc__output_size; \
    FILE *dwc__page = open_memstream(&dwc__page_output, &dwc__output_size); \
    if(!dwc__page) { \
        send_error(hs, 404, "Could not create new buffer for page"); \
        DWC_LOG("Error %d when creating buffer for socket %d\n", errno, hs->socket); \
        return; \
    } \
    WEBPAGE_WRITE("<html><head>%s<title>%s</title></head>\n<body>", stylesheet, x);

#define WEBPAGE_WRITE(...) fprintf(dwc__page, __VA_ARGS__)

#define WEBPAGE_FINISH() \
    WEBPAGE_WRITE(html_footer); \
    fclose(dwc__page); \
    send_ok(hs, "text/html", -1); \
    char *dwc__output_cursor = dwc__page_output; \
    while(dwc__output_size > 0) { \
        int rv = write(hs->socket, dwc__output_cursor, dwc__output_size); \
        if (rv <= 0) \
            goto dwc__send_out; \
        dwc__output_size -= rv; \
        dwc__output_cursor += rv; \
    } \
    dwc__send_out: \
    FREE(dwc__page_output);

#endif /* DWC_COMMON_H */
