#ifndef DWC_COMMON_H
#define DWC_COMMON_H

#define _GNU_SOURCE
#include <kos.h>
#include <conio/conio.h>
#include <errno.h>
#include <stdbool.h>

typedef enum http_method {
    METHOD_GET = 1,
    METHOD_POST = 2,
} http_method_t;

typedef struct http_state {
    int                 socket;
    struct sockaddr_in  client;
    socklen_t           client_size;
    kthread_t           *thd;
    http_method_t       method;
    char                *path;
    /* POST Content-Type */
    /* POST boundary */
} http_state_t;

/* Stylesheets and HTML elements */
extern const char *stylesheet;
extern const char *html_footer;

/* Create a new thread for each client connection */
void *client_thread(void *p);

/* Send HTTP OK and Error headers */
void send_ok(http_state_t *hs, const char *content_type,
             size_t len, const char *filename, bool attachment);
void send_error(http_state_t *hs, unsigned error_code, const char *error_str);

/* Send main index page */
void send_main(http_state_t *hs);

/* Send VMU management page */
void send_vmu(http_state_t *hs);

/* Send a VMU dump for the specified VMU device */
void send_vmu_image(http_state_t *hs, maple_device_t *vmu_dev, bool dcm);

/* Send a memory region */
void send_memory(http_state_t *hs, size_t start, size_t end, const char* filename);

/* Access file or directory from filesystem */
void send_fs(http_state_t *hs, char *path);

/* Always set pointer to NULL after free */
#define FREE(x) \
    free(x); \
    x = NULL;

/* Define our DWC_LOGger */
#define DWC_LOG(...) conio_printf(__VA_ARGS__)

/* Shorthand to set up a standard webpage.
 * Pass webpage title as an argument */
#define WEBPAGE_START(x) \
    char *dwc__page_output; \
    size_t dwc__output_size; \
    FILE *dwc__page = open_memstream(&dwc__page_output, &dwc__output_size); \
    if(!dwc__page) { \
        send_error(hs, 404, "Could not create new buffer for page"); \
        DWC_LOG("Error %d when creating buffer for socket %d\n", errno, hs->socket); \
        return; \
    } \
    WEBPAGE_WRITE("<html>\n<head>%s<title>%s</title>\n</head>\n\n<body>", stylesheet, x);

/* Shorthand to write to a standard webpage buffer */
#define WEBPAGE_WRITE(...) fprintf(dwc__page, __VA_ARGS__)

/* Shorthand to complete, write out,
 * and clean up standard webpage */
#define WEBPAGE_FINISH() \
    WEBPAGE_WRITE(html_footer); \
    fclose(dwc__page); \
    send_ok(hs, "text/html", dwc__output_size, NULL, false); \
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
