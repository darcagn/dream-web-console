#include "common.h"

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS | INIT_NET);

/* Main httpd thread */
void *httpd(void *arg) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) {
        conio_printf("httpd: socket create failed\n");
        return NULL;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(80);

    if(bind(listenfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        conio_printf("httpd: bind failed\n");
        close(listenfd);
        return NULL;
    }

    if(listen(listenfd, 10) < 0) {
        conio_printf("httpd: listen failed\n");
        close(listenfd);
        return NULL;
    }

    conio_printf("httpd: listening for connections on socket %d\n", listenfd);

    for ( ; ; ) {
        int maxfdp1 = listenfd + 1;

        fd_set readset, writeset;
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_SET(listenfd, &readset);

        if(select(maxfdp1, &readset, &writeset, 0, 0) == 0)
            continue;

        http_state_t *hs;
        /* Check for new incoming connections */
        if(FD_ISSET(listenfd, &readset)) {
            hs = calloc(1, sizeof(http_state_t));
            hs->client_size = sizeof(hs->client);
            hs->socket = accept(listenfd, (struct sockaddr *)&hs->client, &hs->client_size);

            /* Adjust socket buffers to the largest possible buffer in KallistiOS */
            uint32_t new_buf_sz = 65535;
            setsockopt(hs->socket, SOL_SOCKET, SO_SNDBUF, &new_buf_sz, sizeof(new_buf_sz));
            setsockopt(hs->socket, SOL_SOCKET, SO_RCVBUF, &new_buf_sz, sizeof(new_buf_sz));

            char ipstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(hs->client.sin_addr.s_addr), ipstr, INET_ADDRSTRLEN);
            conio_printf("httpd: %s:%d connected on socket %d\n",
                         ipstr, hs->client.sin_port, hs->socket);

            if(hs->socket < 0) {
                free(hs);
                hs = NULL;
            } else {
                /* Create thread for new client */
                hs->thd = thd_create(1, client_thread, hs);
            }
        }
    }
}

int main(int argc, char **argv) {
    /* Automatically spin down GD-ROM once we are successfully loaded */
    cdrom_spin_down();

    pvr_init_defaults();

    /* Initialize conio text output */
    conio_init(CONIO_TTY_PVR, CONIO_INPUT_LINE);
    conio_set_theme(CONIO_THEME_MATRIX);

    conio_printf("Dream Web Console v%s\n", VERSION);
    conio_printf("Press Start to Quit\n");

    if(!net_default_dev)
        conio_printf("DHCP failed and no static IP set!\n");
    else {
        conio_printf("Ready! IP address: %d.%d.%d.%d\n",
            net_default_dev->ip_addr[0], net_default_dev->ip_addr[1],
            net_default_dev->ip_addr[2], net_default_dev->ip_addr[3]);
        thd_create(0, httpd, NULL);
    }

    for ( ; ; ) {
        MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st)
        if (st->buttons & CONT_START) {
            return 0;
        }
        if (st->buttons & CONT_Y) {
            malloc_stats();
        }
        MAPLE_FOREACH_END()
    }
    return 0;
}
