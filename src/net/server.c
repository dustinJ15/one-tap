/*
 * server.c — headless authoritative game server.
 * Runs at SERVER_TICKRATE Hz using clock_gettime + nanosleep.
 * Start with:  ./one-tap --server [port]
 */

#include "net.h"
#include "../physics/movement.h"
#include "../render/map.h"
#include "../game/round.h"
#include "../game/economy.h"

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
    bool               shoot_pending;
    bool               active;
    bool               dead;
    Team               team;
    uint8_t            weapon;   /* WeaponId */
    int                money;
    double             last_seen;
} SrvClient;

/* CT spawns: +Z side of the room */
static const Vector3 CT_SPAWNS[5] = {
    { -300.0f, PLAYER_EYE_STAND,  300.0f },
    {  300.0f, PLAYER_EYE_STAND,  300.0f },
    {    0.0f, PLAYER_EYE_STAND,  300.0f },
    { -150.0f, PLAYER_EYE_STAND,  200.0f },
    {  150.0f, PLAYER_EYE_STAND,  200.0f },
};

/* T spawns: -Z side of the room */
static const Vector3 T_SPAWNS[5] = {
    { -300.0f, PLAYER_EYE_STAND, -300.0f },
    {  300.0f, PLAYER_EYE_STAND, -300.0f },
    {    0.0f, PLAYER_EYE_STAND, -300.0f },
    { -150.0f, PLAYER_EYE_STAND, -200.0f },
    {  150.0f, PLAYER_EYE_STAND, -200.0f },
};

static bool addr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port         == b->sin_port;
}

static Vector3 team_spawn(SrvClient *clients, int id)
{
    Team team = clients[id].team;
    const Vector3 *spawns = (team == TEAM_CT) ? CT_SPAWNS : T_SPAWNS;
    int idx = 0;
    for (int j = 0; j < MAX_PLAYERS; j++) {
        if (j == id || !clients[j].active) continue;
        if (clients[j].team == team) idx++;
    }
    return spawns[idx % 5];
}

static void server_distribute_round_money(SrvClient *clients, const RoundState *round)
{
    bool ct_won = (round->win_team == 1);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].active) continue;
        bool on_winner = (ct_won && clients[i].team == TEAM_CT) ||
                         (!ct_won && clients[i].team == TEAM_T);
        if (on_winner) {
            economy_add(&clients[i].money, MONEY_WIN);
        } else {
            int streak = (clients[i].team == TEAM_CT)
                         ? round->ct_loss_streak : round->t_loss_streak;
            economy_add(&clients[i].money, economy_loss_bonus(streak));
        }
    }
}

static void server_restart_round(SrvClient *clients)
{
    int ct_idx = 0, t_idx = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!clients[i].active) continue;
        clients[i].dead          = false;
        clients[i].player.health = 100;
        clients[i].player.velocity = (Vector3){ 0, 0, 0 };
        clients[i].shoot_pending = false;
        clients[i].last_buttons  = 0;
        clients[i].weapon        = WEAPON_PISTOL;  /* pistol always free at round start */
        if (clients[i].team == TEAM_CT) {
            clients[i].player.position = CT_SPAWNS[ct_idx % 5];
            ct_idx++;
        } else {
            clients[i].player.position = T_SPAWNS[t_idx % 5];
            t_idx++;
        }
    }
}

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
        if (i == shooter_id || !clients[i].active || clients[i].dead) continue;
        if (clients[i].team == s->team) continue;  /* no friendly fire */

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
                clients[i].player.health = 0;
                clients[i].dead = true;
                economy_add(&clients[shooter_id].money, MONEY_KILL);
                printf("[server] player %d (%s) killed player %d (%s)  +$%d\n",
                       shooter_id, s->team == TEAM_CT ? "CT" : "T",
                       i, clients[i].team == TEAM_CT ? "CT" : "T",
                       MONEY_KILL);
            }
        }
    }
}

static void server_recv_packets(int sock, SrvClient *clients, double now,
                                const RoundState *round)
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
            if (id < 0) continue;

            if (!clients[id].active) {
                memset(&clients[id], 0, sizeof(SrvClient));
                clients[id].addr   = addr;
                clients[id].active = true;
                clients[id].money  = MONEY_START;
                clients[id].weapon = WEAPON_PISTOL;

                int ct_count = 0, t_count = 0;
                for (int j = 0; j < MAX_PLAYERS; j++) {
                    if (!clients[j].active) continue;
                    if (clients[j].team == TEAM_CT) ct_count++;
                    else                             t_count++;
                }
                clients[id].team = (ct_count <= t_count) ? TEAM_CT : TEAM_T;

                player_init(&clients[id].player);
                clients[id].player.position = team_spawn(clients, id);
                clients[id].dead = (round->phase == PHASE_END);

                printf("[server] player %d connected as %s from %s:%d  $%d\n",
                       id,
                       clients[id].team == TEAM_CT ? "CT" : "T",
                       inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
                       clients[id].money);
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

            if (!clients[id].dead && round->phase == PHASE_LIVE) {
                clients[id].player.position.x = pkt.x;
                clients[id].player.position.y = pkt.y;
                clients[id].player.position.z = pkt.z;
                clients[id].player.yaw        = pkt.yaw;
                clients[id].player.pitch      = pkt.pitch;

                uint8_t prev = clients[id].last_buttons;
                uint8_t curr = pkt.buttons;
                if ((curr & BTN_SHOOT) && !(prev & BTN_SHOOT))
                    clients[id].shoot_pending = true;
                clients[id].last_buttons = curr;
            } else if (round->phase == PHASE_BUY) {
                /* Allow position updates during buy phase so player can move */
                clients[id].player.position.x = pkt.x;
                clients[id].player.position.y = pkt.y;
                clients[id].player.position.z = pkt.z;
                clients[id].player.yaw        = pkt.yaw;
                clients[id].player.pitch      = pkt.pitch;
            }

        } else if (type == PKT_BUY && n >= (ssize_t)sizeof(PktBuy)) {
            PktBuy pkt;
            memcpy(&pkt, buf, sizeof(PktBuy));
            int id = pkt.player_id;
            if (id < 0 || id >= MAX_PLAYERS || !clients[id].active) continue;
            if (!addr_eq(&clients[id].addr, &addr)) continue;
            if (round->phase != PHASE_BUY) continue;
            if (pkt.weapon_id >= WEAPON_COUNT) continue;
            if (clients[id].money < WEAPONS[pkt.weapon_id].price) continue;

            economy_add(&clients[id].money, -WEAPONS[pkt.weapon_id].price);
            clients[id].weapon = pkt.weapon_id;
            printf("[server] player %d bought %s  $%d remaining\n",
                   id, WEAPONS[pkt.weapon_id].name, clients[id].money);

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

static void server_tick(SrvClient *clients, const Map *map, RoundState *round, float dt)
{
    if (round->phase == PHASE_BUY) {
        for (int i = 0; i < MAX_PLAYERS; i++) clients[i].shoot_pending = false;
        round_tick_buy(round, dt);

    } else if (round->phase == PHASE_LIVE) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].active || clients[i].dead) continue;
            if (clients[i].shoot_pending) {
                server_apply_shoot(clients, i, map);
                clients[i].shoot_pending = false;
            }
        }

        int ct_alive = 0, t_alive = 0, ct_total = 0, t_total = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!clients[i].active) continue;
            if (clients[i].team == TEAM_CT) {
                ct_total++;
                if (!clients[i].dead) ct_alive++;
            } else {
                t_total++;
                if (!clients[i].dead) t_alive++;
            }
        }

        if (round_tick_live(round, dt, ct_alive, t_alive, ct_total, t_total)) {
            server_distribute_round_money(clients, round);
            printf("[server] round over — %s  (CT:%d T:%d)\n",
                   round->win_team == 1 ? "CT WIN" : "T WIN",
                   round->ct_score, round->t_score);
        }

    } else { /* PHASE_END */
        for (int i = 0; i < MAX_PLAYERS; i++) clients[i].shoot_pending = false;

        if (round_tick_end(round, dt)) {
            round_start(round);
            server_restart_round(clients);
            printf("[server] new round (buy phase)  (CT:%d T:%d)\n",
                   round->ct_score, round->t_score);
        }
    }
}

static void server_send_world(int sock, SrvClient *clients, const RoundState *round)
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
        pkt.players[i].team   = (uint8_t)clients[i].team;
        pkt.players[i].flags  = clients[i].dead ? 1 : 0;
        pkt.players[i].money  = (uint16_t)(clients[i].money > 0 ? clients[i].money : 0);
    }

    /* Timer shows buy countdown during buy phase, round countdown otherwise */
    float timer_val = (round->phase == PHASE_BUY) ? round->buy_timer : round->timer;
    pkt.phase       = (uint8_t)round->phase;
    pkt.round_ticks = (uint16_t)(timer_val * 10.0f);
    pkt.ct_score    = (uint8_t)round->ct_score;
    pkt.t_score     = (uint8_t)round->t_score;
    pkt.win_team    = (uint8_t)round->win_team;

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

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    SrvClient clients[MAX_PLAYERS];
    memset(clients, 0, sizeof(clients));

    Map map;
    map_build(&map);

    RoundState round;
    round_init(&round);

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

        server_recv_packets(sock, clients, now, &round);
        server_tick(clients, &map, &round, (float)SERVER_DT);
        server_send_world(sock, clients, &round);

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
