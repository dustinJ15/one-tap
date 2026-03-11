#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define NET_PORT        27015
#define MAX_PLAYERS     10
#define SERVER_TICKRATE 64
#define SERVER_DT       (1.0f / (float)SERVER_TICKRATE)

/* Packet type bytes */
#define PKT_CONNECT    1
#define PKT_ACCEPT     2
#define PKT_INPUT      3
#define PKT_WORLD      4
#define PKT_DISCONNECT 5
#define PKT_BUY        6

/* Button bitmask bits */
#define BTN_FORWARD  (1 << 0)
#define BTN_BACK     (1 << 1)
#define BTN_LEFT     (1 << 2)
#define BTN_RIGHT    (1 << 3)
#define BTN_JUMP     (1 << 4)
#define BTN_CROUCH   (1 << 5)
#define BTN_SHOOT    (1 << 6)

#pragma pack(push, 1)

typedef struct { uint8_t type; }                     PktConnect;
typedef struct { uint8_t type; uint8_t player_id; }  PktAccept;

typedef struct {
    uint8_t  type;
    uint8_t  player_id;
    uint8_t  buttons;
    float    yaw;
    float    pitch;
    uint32_t seq;
    float    x, y, z;  /* client eye position */
} PktInput;   /* 27 bytes */

typedef struct {
    float    x, y, z;
    float    yaw;
    uint8_t  health;
    uint8_t  active;
    uint8_t  team;     /* 0=CT 1=T */
    uint8_t  flags;    /* bit0=dead/spectating */
    uint16_t money;
} PlayerInfo;   /* 22 bytes */

typedef struct {
    uint8_t    type;
    uint32_t   seq;
    PlayerInfo players[MAX_PLAYERS];
    uint8_t    phase;        /* 0=live 1=end 2=buy */
    uint16_t   round_ticks;  /* tenths of a second remaining */
    uint8_t    ct_score;
    uint8_t    t_score;
    uint8_t    win_team;     /* 0=none 1=CT 2=T */
} PktWorld;   /* 1 + 4 + 10*22 + 6 = 231 bytes */

typedef struct { uint8_t type; uint8_t player_id; }                    PktDisconnect;
typedef struct { uint8_t type; uint8_t player_id; uint8_t weapon_id; } PktBuy;

#pragma pack(pop)

typedef struct {
    int                sock;
    struct sockaddr_in server_addr;
    uint8_t            my_id;
    bool               connected;
    uint32_t           seq;
    PlayerInfo         remote[MAX_PLAYERS];
    uint8_t            round_phase;
    float              round_timer;  /* seconds remaining */
    uint8_t            ct_score;
    uint8_t            t_score;
    uint8_t            win_team;
} NetClient;

bool net_client_connect(NetClient *c, const char *server_ip, int port);
void net_client_send_input(NetClient *c, uint8_t buttons, float yaw, float pitch,
                           float x, float y, float z);
void net_client_buy(NetClient *c, uint8_t weapon_id);
void net_client_recv(NetClient *c);
void net_client_disconnect(NetClient *c);

void server_run(int port);

#endif
