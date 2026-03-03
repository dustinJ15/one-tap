#include "raylib.h"
#include "physics/movement.h"
#include "render/map.h"
#include "game/weapons.h"
#include "net/net.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "one-tap"
#define MAX_HOLES     512

typedef struct { Vector3 pos; Vector3 normal; } BulletHole;

int main(int argc, char **argv)
{
    /* --- Arg parsing --- */
    const char *server_ip = NULL;
    int         net_port  = NET_PORT;
    bool        is_server = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0) {
            is_server = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                net_port = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--connect") == 0) {
            server_ip = "127.0.0.1";
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                server_ip = argv[++i];
            }
        }
    }

    if (is_server) {
        server_run(net_port);
        return 0;
    }

    /* --- Window / audio init --- */
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(0);
    DisableCursor();
    InitAudioDevice();
    srand((unsigned int)time(NULL));

    PlayerState player;
    player_init(&player);

    Map map;
    map_build(&map);

    WeaponState weapon;
    weapon_init(&weapon);

    BulletHole holes[MAX_HOLES];
    int hole_count = 0;

    /* --- Networking --- */
    NetClient net;
    memset(&net, 0, sizeof(net));
    if (server_ip) {
        net_client_connect(&net, server_ip, net_port);
    }

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        /* --- Build PlayerInput from local devices --- */
        Vector2 md = GetMouseDelta();
        PlayerInput input = {
            .yaw_delta   =  md.x * MOUSE_SENSITIVITY,
            .pitch_delta = -md.y * MOUSE_SENSITIVITY,
            .fwd    = IsKeyDown(KEY_W)          - IsKeyDown(KEY_S),
            .right  = IsKeyDown(KEY_D)          - IsKeyDown(KEY_A),
            .jump   = IsKeyPressed(KEY_SPACE),
            .crouch = IsKeyDown(KEY_LEFT_CONTROL),
        };

        player_update(&player, &input, dt);
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);

        /* --- Local shoot (visual effect + sound) --- */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            RayHit hit = weapon_shoot(&weapon, &map, &player);
            if (hit.hit && hole_count < MAX_HOLES) {
                holes[hole_count].pos    = hit.point;
                holes[hole_count].normal = hit.normal;
                hole_count++;
            }
        }

        /* --- Network: send input, receive world state --- */
        if (net.connected) {
            uint8_t buttons = 0;
            if (IsKeyDown(KEY_W))             buttons |= BTN_FORWARD;
            if (IsKeyDown(KEY_S))             buttons |= BTN_BACK;
            if (IsKeyDown(KEY_A))             buttons |= BTN_LEFT;
            if (IsKeyDown(KEY_D))             buttons |= BTN_RIGHT;
            if (IsKeyDown(KEY_SPACE))         buttons |= BTN_JUMP;
            if (IsKeyDown(KEY_LEFT_CONTROL))  buttons |= BTN_CROUCH;
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) buttons |= BTN_SHOOT;

            net_client_send_input(&net, buttons, player.yaw, player.pitch,
                                  player.position.x, player.position.y, player.position.z);
            net_client_recv(&net);
        }

        /* --- Draw --- */
        BeginDrawing();
            ClearBackground((Color){ 20, 20, 20, 255 });

            BeginMode3D(cam);
                map_draw(&map);

                /* Bullet holes: thin dark slab flush to the hit surface */
                for (int i = 0; i < hole_count; i++) {
                    Vector3 n = holes[i].normal;
                    Vector3 p = { holes[i].pos.x + n.x * 0.3f,
                                  holes[i].pos.y + n.y * 0.3f,
                                  holes[i].pos.z + n.z * 0.3f };
                    Vector3 size;
                    if      (fabsf(n.y) > 0.5f) size = (Vector3){ 5.0f, 0.2f, 5.0f };
                    else if (fabsf(n.x) > 0.5f) size = (Vector3){ 0.2f, 5.0f, 5.0f };
                    else                         size = (Vector3){ 5.0f, 5.0f, 0.2f };
                    DrawCubeV(p, size, (Color){ 12, 12, 12, 255 });
                }

                /* Remote players: red box at their eye position */
                if (net.connected) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (!net.remote[i].active || i == net.my_id) continue;
                        Vector3 center = {
                            net.remote[i].x,
                            net.remote[i].y - PLAYER_EYE_STAND / 2.0f,
                            net.remote[i].z,
                        };
                        DrawCubeV(center, (Vector3){ 32.0f, 72.0f, 32.0f }, RED);
                    }
                }

            EndMode3D();

            /* Crosshair */
            int cx = WINDOW_WIDTH  / 2;
            int cy = WINDOW_HEIGHT / 2;
            DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
            DrawLine(cx, cy - 10, cx, cy + 10, WHITE);

            /* HUD */
            DrawFPS(10, 10);
            float hspeed = sqrtf(player.velocity.x * player.velocity.x +
                                 player.velocity.z * player.velocity.z);
            DrawText(TextFormat("speed: %.0f u/s", hspeed), 10, 34, 20, RAYWHITE);
            DrawText(player.crouching ? "CROUCH" : "", 10, 58, 20, RAYWHITE);

            int hp = net.connected ? (int)net.remote[net.my_id].health : 100;
            DrawText(TextFormat("HP: %d", hp), 10, 82, 20, RAYWHITE);

        EndDrawing();
    }

    if (net.connected) net_client_disconnect(&net);
    weapon_free(&weapon);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
