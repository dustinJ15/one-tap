/*
 * server.c — headless authoritative game server.
 * Runs at SERVER_TICKRATE Hz using clock_gettime + nanosleep.
 * Start with:  ./one-tap --server [port]
 */

#include "net.h"
#include "../physics/movement.h"
#include "../render/map.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CLIENT_TIMEOUT  5.0   /* drop client if unseen for this many seconds */

static volatile int g_running = 1;
static void handle_sigint(int sig) { (void)sig; g_running = 0; }

typedef struct {
    struct sockaddr_in addr;
    PlayerState        player;
    uint8_t            last_buttons;
    bool               jump_pending;
    bool               shoot_pending;
    PlayerInput        input;
    bool               active;
    double             last_seen;
} SrvClient;

/* Ten spawn points spread across the room interior (room is 800×800, -400..+400) */
static const Vector3 SPAWN_POINTS[MAX_PLAYERS] = {
    { -300.0f, PLAYER_EYE_STAND,  300.0f },
    {  300.0f, PLAYER_EYE_STAND,  300.0f },
    { -300.0f, PLAYER_EYE_STAND, -300.0f },
    {  300.0f, PLAYER_EYE_STAND, -300.0f },
    {    0.0f, PLAYER_EYE_STAND,  300.0f },
    {    0.0f, PLAYER_EYE_STAND, -300.0f },
    { -300.0f, PLAYER_EYE_STAND,    0.0f },
    {  300.0f, PLAYER_EYE_STAND,    0.0f },
    { -150.0f, PLAYER_EYE_STAND,  150.0f },
    {  150.0f, PLAYER_EYE_STAND, -150.0f },
};

static bool addr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port         == b->sin_port;
}

/* Ray vs player box — deals damage, respawns on kill */
static void server_apply_shoot(SrvClient *clients, int shooter_id, const Map *map)
{
    SrvClient *s  = &clients[shooter_id];
    float yr = s->player.yaw   * DEG2RAD;
    float pr = s->player.pitch * DEG2RAD;
    Vector3 dir = {
         sinf(yr) * cosf(pr),
         sinf(pr),
        -cosf(yr) * cosf(pr)
    };
    Vector3 origin = s->player.position;

    RayHit wall   = map_raycast(map, origin, dir);
    float  wall_t = wall.hit ? wall.t : 1e30f;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == shooter_id || !clients[i].active) continue;

        PlayerState *p  = &clients[i].player;
        Vector3 bmin = { p->position.x - PLAYER_HALF_WIDTH,
                         p->position.y - PLAYER_EYE_STAND,
                         p->position.z - PLAYER_HALF_WIDTH };
        Vector3 bmax = { p->position.x + PLAYER_HALF_WIDTH,
                         p->position.y,
                         p->position.z + PLAYER_HALF_WIDTH };

        float pt = map_ray_hits_box(origin, dir, bmin, bmax);
        if (pt > 0.0f && pt < wall_t) {
            clients[i].player.health -= 34;
            if (clients[i].player.health <= 0) {
                clients[i].player.health   = 100;
                clients[i].player.position = SPAWN_POINTS[i % MAX_PLAYERS];
                clients[i].player.velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
                printf("[server] player %d killed player %d\n", shooter_id, i);
            }
        }
    }
}

static void server_recv_packets(int sock, SrvClient *clients, double now)
{
    uint8_t            buf[256];
    struct sockaddr_in addr;
    socklen_t          addrlen;

    for (;;) {
        addrlen = sizeof(addr);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0,
                             (struct sockaddr *)&addr, &addrlen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        if (n < 1) continue;

        uint8_t type = buf[0];

        if (type == PKT_CONNECT) {
            /* Find existing slot (reconnect) or first free slot */
            int id = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (clients[i].active && addr_eq(&clients[i].addr, &addr)) {
                    id = i; break;
                }
            }
            if (id < 0) {
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (!clients[i].active) { id = i; break; }
                }
            }
            if (id < 0) continue; /* server full */

            if (!clients[id].active) {
                memset(&clients[id], 0, sizeof(SrvClient));
                clients[id].addr   = addr;
                clients[id].active = true;
                player_init(&clients[id].player);
                clients[id].player.position = SPAWN_POINTS[id % MAX_PLAYERS];
                printf("[server] player %d connected from %s:%d\n",
                       id, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            }
            clients[id].last_seen = now;

            PktAccept pkt = { PKT_ACCEPT, (uint8_t)id };
            sendto(sock, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&addr, sizeof(addr));

        } else if (type == PKT_INPUT && n >= (ssize_t)sizeof(PktInput)) {
            PktInput pkt;
            memcpy(&pkt, buf, sizeof(PktInput));
            int id = pkt.player_id;
            if (id < 0 || id >= MAX_PLAYERS || !clients[id].active) continue;
            if (!addr_eq(&clients[id].addr, &addr)) continue;

            clients[id].last_seen = now;

            /* Trust the client's reported position — avoids parallel-sim divergence */
            clients[id].player.position.x = pkt.x;
            clients[id].player.position.y = pkt.y;
            clients[id].player.position.z = pkt.z;
            clients[id].player.yaw        = pkt.yaw;
            clients[id].player.pitch      = pkt.pitch;

            /* Rising-edge detection for shoot */
            uint8_t prev = clients[id].last_buttons;
            uint8_t curr = pkt.buttons;
            if ((curr & BTN_SHOOT) && !(prev & BTN_SHOOT)) clients[id].shoot_pending = true;
            clients[id].last_buttons = curr;

        } else if (type == PKT_DISCONNECT && n >= (ssize_t)sizeof(PktDisconnect)) {
            PktDisconnect pkt;
            memcpy(&pkt, buf, sizeof(PktDisconnect));
            int id = pkt.player_id;
            if (id < 0 || id >= MAX_PLAYERS) continue;
            if (!clients[id].active || !addr_eq(&clients[id].addr, &addr)) continue;
            clients[id].active = false;
            printf("[server] player %d disconnected\n", id);
        }
    }
}

static void server_tick(SrvClient *clients, const Map *map)
{
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].active) continue;
        if (clients[i].shoot_pending) {
            server_apply_shoot(clients, i, map);
            clients[i].shoot_pending = false;
        }
    }
}

static void server_send_world(int sock, SrvClient *clients)
{
    PktWorld pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_WORLD;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        pkt.players[i].x      = clients[i].player.position.x;
        pkt.players[i].y      = clients[i].player.position.y;
        pkt.players[i].z      = clients[i].player.position.z;
        pkt.players[i].yaw    = clients[i].player.yaw;
        pkt.players[i].health = (uint8_t)(clients[i].player.health > 0
                                          ? clients[i].player.health : 0);
        pkt.players[i].active = clients[i].active ? 1 : 0;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].active) continue;
        sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&clients[i].addr,
               sizeof(clients[i].addr));
    }
}

void server_run(int port)
{
    signal(SIGINT, handle_sigint);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in bind_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons((uint16_t)port),
    };
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind"); close(sock); return;
    }

    /* Non-blocking recv for the game loop */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    SrvClient clients[MAX_PLAYERS];
    memset(clients, 0, sizeof(clients));

    Map map;
    map_build(&map);

    printf("[server] listening on :%d\n", port);

    struct timespec last_tick;
    clock_gettime(CLOCK_MONOTONIC, &last_tick);

    while (g_running) {
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);

        double now     = (double)now_ts.tv_sec  + now_ts.tv_nsec  * 1e-9;
        double last    = (double)last_tick.tv_sec + last_tick.tv_nsec * 1e-9;
        double elapsed = now - last;

        if (elapsed < SERVER_DT) {
            double rem = SERVER_DT - elapsed;
            struct timespec sleep_ts = {
                .tv_sec  = (time_t)rem,
                .tv_nsec = (long)((rem - (time_t)rem) * 1e9),
            };
            nanosleep(&sleep_ts, NULL);
            continue;
        }
        last_tick = now_ts;

        server_recv_packets(sock, clients, now);
        server_tick(clients, &map);
        server_send_world(sock, clients);

        /* Drop clients that have gone quiet */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (clients[i].active && (now - clients[i].last_seen) > CLIENT_TIMEOUT) {
                printf("[server] player %d timed out\n", i);
                clients[i].active = false;
            }
        }
    }

    close(sock);
    printf("[server] shutdown\n");
}
