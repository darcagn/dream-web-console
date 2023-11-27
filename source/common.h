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

#endif /* DWC_COMMON_H */
