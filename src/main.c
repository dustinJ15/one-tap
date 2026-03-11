#include "raylib.h"
#include "physics/movement.h"
#include "render/map.h"
#include "game/weapons.h"
#include "game/round.h"
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

        /* --- Derive state from server --- */
        bool am_dead   = net.connected && (net.remote[net.my_id].flags & 1);
        bool round_end = net.connected && (net.round_phase == PHASE_END);

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

        /* Suppress movement and shooting when dead or round over */
        if (!am_dead && !round_end) {
            player_update(&player, &input, dt);
        }
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);

        /* --- Fullscreen Toggle --- */
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        /* --- Local shoot (visual + sound) — only when alive and round live --- */
        if (!am_dead && !round_end && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
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
            if (!am_dead && !round_end) {
                if (IsKeyDown(KEY_W))             buttons |= BTN_FORWARD;
                if (IsKeyDown(KEY_S))             buttons |= BTN_BACK;
                if (IsKeyDown(KEY_A))             buttons |= BTN_LEFT;
                if (IsKeyDown(KEY_D))             buttons |= BTN_RIGHT;
                if (IsKeyDown(KEY_SPACE))         buttons |= BTN_JUMP;
                if (IsKeyDown(KEY_LEFT_CONTROL))  buttons |= BTN_CROUCH;
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) buttons |= BTN_SHOOT;
            }

            net_client_send_input(&net, buttons, player.yaw, player.pitch,
                                  player.position.x, player.position.y, player.position.z);
            net_client_recv(&net);

            /* Re-derive after recv in case state changed */
            am_dead   = net.remote[net.my_id].flags & 1;
            round_end = net.round_phase == PHASE_END;
        }

        /* --- Draw --- */
        BeginDrawing();
            ClearBackground((Color){ 20, 20, 20, 255 });

            BeginMode3D(cam);
                map_draw(&map);

                /* Bullet holes */
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

                /* Remote players: CT=blue box, T=red box */
                if (net.connected) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (!net.remote[i].active || i == net.my_id) continue;
                        if (net.remote[i].flags & 1) continue; /* dead: don't render */
                        Color c = (net.remote[i].team == TEAM_CT) ? BLUE : RED;
                        Vector3 center = {
                            net.remote[i].x,
                            net.remote[i].y - PLAYER_EYE_STAND / 2.0f,
                            net.remote[i].z,
                        };
                        DrawCubeV(center, (Vector3){ 32.0f, 72.0f, 32.0f }, c);
                    }
                }

            EndMode3D();

            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            int cx = sw / 2;
            int cy = sh / 2;

            /* Crosshair — only when alive */
            if (!am_dead) {
                DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
                DrawLine(cx, cy - 10, cx, cy + 10, WHITE);
            }

            /* Round timer (top center) */
            if (net.connected && !round_end) {
                int total_secs = (int)net.round_timer;
                int mins = total_secs / 60;
                int secs = total_secs % 60;
                const char *timer_str = TextFormat("%d:%02d", mins, secs);
                int tw = MeasureText(timer_str, 28);
                Color tc = (total_secs <= 10) ? RED : RAYWHITE;
                DrawText(timer_str, cx - tw / 2, 10, 28, tc);
            }

            /* Team label (top center, below timer) */
            if (net.connected) {
                bool my_team_ct = (net.remote[net.my_id].team == TEAM_CT);
                const char *team_str = my_team_ct ? "CT" : "T";
                Color team_col = my_team_ct ? BLUE : RED;
                int tw = MeasureText(team_str, 20);
                DrawText(team_str, cx - tw / 2, 44, 20, team_col);
            }

            /* HUD (bottom-left) */
            DrawFPS(10, 10);
            float hspeed = sqrtf(player.velocity.x * player.velocity.x +
                                 player.velocity.z * player.velocity.z);
            DrawText(TextFormat("speed: %.0f u/s", hspeed), 10, 34, 20, RAYWHITE);
            DrawText(player.crouching ? "CROUCH" : "", 10, 58, 20, RAYWHITE);

            int hp = net.connected ? (int)net.remote[net.my_id].health : 100;
            DrawText(TextFormat("HP: %d", hp), 10, sh - 36, 24,
                     hp <= 25 ? RED : RAYWHITE);

            /* Score (top-left) */
            if (net.connected) {
                DrawText(TextFormat("CT %d  |  %d T", net.ct_score, net.t_score),
                         10, sh - 64, 20, LIGHTGRAY);
            }

            /* Spectator overlay */
            if (am_dead) {
                DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 80 });
                const char *spec_str = "SPECTATING";
                int tw = MeasureText(spec_str, 32);
                DrawText(spec_str, cx - tw / 2, cy - 60, 32, (Color){ 220, 220, 220, 200 });
            }

            /* Round-end scoreboard overlay */
            if (round_end) {
                int bw = 360, bh = 160;
                int bx = cx - bw / 2, by = cy - bh / 2;
                DrawRectangle(bx, by, bw, bh, (Color){ 0, 0, 0, 200 });
                DrawRectangleLines(bx, by, bw, bh, GRAY);

                const char *winner;
                Color wc;
                if (net.win_team == 1)      { winner = "CT WIN";   wc = BLUE; }
                else if (net.win_team == 2) { winner = "T WIN";    wc = RED;  }
                else                        { winner = "TIME OUT"; wc = GRAY; }

                int tw = MeasureText(winner, 36);
                DrawText(winner, cx - tw / 2, by + 20, 36, wc);

                const char *score_str = TextFormat("CT  %d  :  %d  T",
                                                   net.ct_score, net.t_score);
                tw = MeasureText(score_str, 26);
                DrawText(score_str, cx - tw / 2, by + 74, 26, WHITE);

                const char *next_str = "Next round starting...";
                tw = MeasureText(next_str, 18);
                DrawText(next_str, cx - tw / 2, by + 118, 18, DARKGRAY);
            }

        EndDrawing();
    }

    if (net.connected) net_client_disconnect(&net);
    weapon_free(&weapon);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
