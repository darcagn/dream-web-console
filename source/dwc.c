#include <poll.h>
#include "common.h"

#ifdef MALLOC_STATS
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS | INIT_NET);
#else
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_NET);
#endif

/* Main httpd server thread */
void *httpd(void *arg) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) {
        DWC_LOG("httpd: socket create failed\n");
        return NULL;
    }

    struct sockaddr_in saddr = (struct sockaddr_in){0};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(80);

    if(bind(listenfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        DWC_LOG("httpd: bind failed\n");
        close(listenfd);
        return NULL;
    }

    if(listen(listenfd, 10) < 0) {
        DWC_LOG("httpd: listen failed\n");
        close(listenfd);
        return NULL;
    }

    DWC_LOG("httpd: listening for connections on socket %d\n", listenfd);

    /* Loop here listening for new connections */
    for ( ; ; ) {
        struct pollfd fds = { .fd = listenfd, .events = POLLIN, .revents = 0 };

        /* Check for new incoming connections */
        if(poll(&fds, 1, -1) > 0 && fds.revents & POLLIN) {
            http_state_t *hs = calloc(1, sizeof(http_state_t));
            hs->client_size = sizeof(hs->client);
            hs->socket = accept(listenfd, (struct sockaddr *)&hs->client, &hs->client_size);

            if(hs->socket < 0) {
                FREE(hs);
            } else {
                /* Adjust socket buffers to 65535, the
                 * largest possible buffer in KallistiOS */

                uint32_t new_buf_sz = 65535;
                setsockopt(hs->socket, SOL_SOCKET, SO_SNDBUF,
                           &new_buf_sz, sizeof(new_buf_sz));
                setsockopt(hs->socket, SOL_SOCKET, SO_RCVBUF,
                           &new_buf_sz, sizeof(new_buf_sz));

                /* Print client IP address */
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(hs->client.sin_addr.s_addr), ipstr, INET_ADDRSTRLEN);
                DWC_LOG("httpd: %s:%d connected on socket %d\n",
                             ipstr, hs->client.sin_port, hs->socket);

                /* Create thread for new client */
                hs->thd = thd_create(1, client_thread, hs);

#ifdef MALLOC_STATS
                printf("Dispatched new thread on socket %d...\n", hs->socket);
                malloc_stats();
#endif
            }
        } else {
            DWC_LOG("poll() error %d!\n", errno);
        }
    }
}

int main(int argc, char **argv) {
    /* Automatically spin down GD-ROM
     * once we are successfully loaded */
    cdrom_spin_down();

    /* Set up display and conio logging */
    pvr_init_defaults();
    conio_init(CONIO_TTY_PVR, CONIO_INPUT_LINE);
    conio_set_theme(CONIO_THEME_MATRIX);
    DWC_LOG("Dream Web Console v%s\n", VERSION);
    DWC_LOG("Press Start to Quit\n");

    if(!net_default_dev)
        DWC_LOG("DHCP failed and no static IP set!\n");
    else {
        DWC_LOG("Ready! IP address: %d.%d.%d.%d\n",
            net_default_dev->ip_addr[0], net_default_dev->ip_addr[1],
            net_default_dev->ip_addr[2], net_default_dev->ip_addr[3]);

        /* Spawn thread to listen for incoming connections */
        thd_create(0, httpd, NULL);
    }

    /* Loop here for controller input while http server runs */
    for ( ; ; ) {
        MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
        if (st->buttons & CONT_START) {
            return 0;
        }
#ifdef MALLOC_STATS
        if (st->buttons & CONT_Y) {
            malloc_stats();
        }
#endif
        MAPLE_FOREACH_END()
    }
    return 0;
}
