#include "net.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

bool net_client_connect(NetClient *c, const char *server_ip, int port)
{
    memset(c, 0, sizeof(*c));

    c->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c->sock < 0) { perror("socket"); return false; }

    c->server_addr.sin_family = AF_INET;
    c->server_addr.sin_port   = htons((uint16_t)port);
    inet_pton(AF_INET, server_ip, &c->server_addr.sin_addr);

    /* connect() sets the default peer — filters recv to this address */
    if (connect(c->sock, (struct sockaddr *)&c->server_addr,
                sizeof(c->server_addr)) < 0) {
        perror("connect"); close(c->sock); return false;
    }

    /* 200 ms blocking recv for the handshake */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
    setsockopt(c->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    PktConnect conn = { PKT_CONNECT };
    uint8_t    buf[64];

    for (int attempt = 0; attempt < 25; attempt++) {
        send(c->sock, &conn, sizeof(conn), 0);
        ssize_t n = recv(c->sock, buf, sizeof(buf), 0);
        if (n >= (ssize_t)sizeof(PktAccept) && buf[0] == PKT_ACCEPT) {
            c->my_id     = ((PktAccept *)buf)->player_id;
            c->connected = true;
            break;
        }
    }

    if (!c->connected) {
        fprintf(stderr, "[client] could not connect to %s:%d\n", server_ip, port);
        close(c->sock);
        return false;
    }

    /* Switch to non-blocking for the game loop */
    int flags = fcntl(c->sock, F_GETFL, 0);
    fcntl(c->sock, F_SETFL, flags | O_NONBLOCK);

    printf("[client] connected as player %d\n", c->my_id);
    return true;
}

void net_client_send_input(NetClient *c, uint8_t buttons, float yaw, float pitch,
                           float x, float y, float z)
{
    if (!c->connected) return;
    PktInput pkt = {
        .type      = PKT_INPUT,
        .player_id = c->my_id,
        .buttons   = buttons,
        .yaw       = yaw,
        .pitch     = pitch,
        .seq       = c->seq++,
        .x = x, .y = y, .z = z,
    };
    send(c->sock, &pkt, sizeof(pkt), 0);
}

void net_client_recv(NetClient *c)
{
    if (!c->connected) return;

    uint8_t  buf[256];
    PktWorld latest;
    bool     got_world = false;

    for (;;) {
        ssize_t n = recv(c->sock, buf, sizeof(buf), 0);
        if (n < 0) {
            /* EAGAIN / EWOULDBLOCK = nothing more to read */
            break;
        }
        if (n >= (ssize_t)sizeof(PktWorld) && buf[0] == PKT_WORLD) {
            memcpy(&latest, buf, sizeof(PktWorld));
            got_world = true;
        }
    }

    if (got_world) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            c->remote[i] = latest.players[i];
        }
    }
}

void net_client_disconnect(NetClient *c)
{
    if (!c->connected) return;
    PktDisconnect pkt = { .type = PKT_DISCONNECT, .player_id = c->my_id };
    send(c->sock, &pkt, sizeof(pkt), 0);
    close(c->sock);
    c->connected = false;
}
