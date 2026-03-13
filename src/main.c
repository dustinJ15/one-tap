#include "raylib.h"
#include "physics/movement.h"
#include "render/map.h"
#include "game/weapons.h"
#include "game/round.h"
#include "game/economy.h"
#include "net/net.h"
#include <math.h>
#include <stdio.h>
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
    const char *server_ip    = NULL;
    int         net_port     = NET_PORT;
    bool        is_server    = false;
    bool        testing = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0) {
            is_server = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
                net_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--testing") == 0) {
            testing = true;
        } else if (strcmp(argv[i], "--connect") == 0) {
            server_ip = "127.0.0.1";
            if (i + 1 < argc && argv[i + 1][0] != '-')
                server_ip = argv[++i];
        }
    }

    if (is_server) { server_run(net_port, testing); return 0; }

    /* --- Window / audio init --- */
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    ToggleFullscreen();
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);   /* ESC is now the pause key, not a quit shortcut */
    DisableCursor();
    InitAudioDevice();
    srand((unsigned int)time(NULL));

    PlayerState player;
    player_init(&player);

    Map map;
    map_build(&map);

    WeaponState slots[2];
    weapon_init(&slots[0]);   /* primary - empty until bought */
    weapon_init(&slots[1]);   /* pistol - always have it */
    int  active_slot    = 1;  /* start on pistol */
    bool has_primary    = false;
    bool awp_wants_zoom = false;

    BulletHole holes[MAX_HOLES];
    int     hole_count     = 0;
    bool    buy_menu_open  = false;
    bool    paused         = false;
    uint8_t prev_phase     = 0xFF;  /* for detecting PHASE_END → PHASE_BUY transition */
    float   hint_timer     = 5.0f;  /* seconds to show F11 hint on startup */

    /* --- Networking --- */
    NetClient net;
    memset(&net, 0, sizeof(net));
    if (server_ip) net_client_connect(&net, server_ip, net_port);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        /* --- Derive round state --- */
        bool am_dead   = net.connected && (net.remote[net.my_id].flags & 1);
        bool round_end = net.connected && (net.round_phase == PHASE_END);
        bool in_buy    = net.connected && (net.round_phase == PHASE_BUY);

        if (!in_buy && !testing) buy_menu_open = false;

        /* --- Sync weapons from server --- */
        if (net.connected) {
            uint8_t srv_primary = net.remote[net.my_id].weapon;
            uint8_t srv_pistol  = net.remote[net.my_id].weapon2;
            int     srv_aslot   = (int)net.remote[net.my_id].active_slot;

            if (srv_aslot < 3) active_slot = srv_aslot;

            if (srv_primary != 0xFF) {
                if (!has_primary || (WeaponId)srv_primary != slots[0].weapon_id) {
                    weapon_switch(&slots[0], (WeaponId)srv_primary);
                    slots[0].ammo_mag     = (int)net.remote[net.my_id].ammo_mag;
                    slots[0].ammo_reserve = (int)net.remote[net.my_id].ammo_reserve;
                    has_primary = true;
                }
            } else {
                has_primary = false;
            }

            if ((WeaponId)srv_pistol != slots[1].weapon_id)
                weapon_switch(&slots[1], (WeaponId)srv_pistol);

            if (net.round_phase == PHASE_BUY && prev_phase == PHASE_END) {
                weapon_round_reset(&slots[0]);
                weapon_round_reset(&slots[1]);
                slots[0].ammo_mag     = (int)net.remote[net.my_id].ammo_mag;
                slots[0].ammo_reserve = (int)net.remote[net.my_id].ammo_reserve;
                slots[1].ammo_mag     = (int)net.remote[net.my_id].ammo2_mag;
                slots[1].ammo_reserve = (int)net.remote[net.my_id].ammo2_reserve;
            }
        }
        prev_phase = net.round_phase;

        /* --- Active weapon and move factor --- */
        WeaponState *active_ws = &slots[active_slot < 2 ? active_slot : 1];
        bool awp_active = (active_slot == 0 && has_primary && slots[0].weapon_id == WEAPON_AWP);
        bool awp_zoomed = awp_active && awp_wants_zoom && slots[0].shot_timer == 0.0f;
        if (!awp_active) awp_wants_zoom = false;
        float move_factor = (active_slot == 2 || (active_slot == 0 && !has_primary))
                            ? 1.0f : WEAPONS[active_ws->weapon_id].move_factor;

        /* --- Lock cursor every frame so it can't escape the window --- */
        if (IsWindowFocused() && IsCursorHidden() == false) DisableCursor();

        /* --- Build PlayerInput --- */
        /* When zoomed, scale sens so physical mouse movement covers the same
         * world angle as unzoomed: factor = tan(half_zoom_fov) / tan(half_fov) */
        float sens = MOUSE_SENSITIVITY;
        if (awp_zoomed)
            sens *= tanf(10.0f * DEG2RAD) / tanf(45.0f * DEG2RAD);  /* 20°/2 over 90°/2 */
        Vector2 md = GetMouseDelta();
        PlayerInput input = {
            .yaw_delta   =  md.x * sens,
            .pitch_delta = -md.y * sens,
            .fwd    = IsKeyDown(KEY_W)          - IsKeyDown(KEY_S),
            .right  = IsKeyDown(KEY_D)          - IsKeyDown(KEY_A),
            .jump   = IsKeyPressed(KEY_SPACE) || (GetMouseWheelMove() < 0.0f),
            .crouch = IsKeyDown(KEY_LEFT_CONTROL),
            .max_speed = (move_factor < 1.0f) ? (PLAYER_SPEED_GROUND * move_factor) : 0.0f,
        };

        if (!am_dead && !round_end) {
            player_update(&player, &input, dt);
        }
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);
        if (awp_zoomed) cam.fovy = 20.0f;

        /* --- Misc keys --- */
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        /* ESC: close buy menu first, then toggle pause */
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (buy_menu_open) {
                buy_menu_open = false;
            } else {
                paused = !paused;
                if (paused) EnableCursor();
                else        DisableCursor();
            }
        }

        if (paused) goto draw;

        if (IsKeyPressed(KEY_R) && !am_dead && !round_end && !in_buy && active_slot < 2)
            weapon_reload(active_ws);

        /* AWP zoom toggle */
        if (awp_active && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !am_dead && !in_buy)
            awp_wants_zoom = !awp_wants_zoom;

        /* Slot switching (only when buy menu is closed) */
        if (!buy_menu_open && !am_dead && !round_end) {
            if (IsKeyPressed(KEY_ONE) && has_primary) {
                active_slot = 0;
                if (net.connected) net_client_equip(&net, 0);
            }
            if (IsKeyPressed(KEY_TWO)) {
                active_slot = 1;
                awp_wants_zoom = false;
                if (net.connected) net_client_equip(&net, 1);
            }
            if (IsKeyPressed(KEY_THREE)) {
                active_slot = 2;
                awp_wants_zoom = false;
                if (net.connected) net_client_equip(&net, 2);
            }
        }

        if ((in_buy || testing) && IsKeyPressed(KEY_B)) buy_menu_open = !buy_menu_open;

        /* --- Buy menu selection --- */
        if (buy_menu_open && net.connected) {
            for (int i = 0; i < WEAPON_COUNT; i++) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    net_client_buy(&net, (uint8_t)i);
                    buy_menu_open = false;
                }
            }
        }

        /* --- Weapon tick (fire rate, reload, spray reset) --- */
        weapon_tick(&slots[0], dt);
        weapon_tick(&slots[1], dt);

        /* --- Shoot --- */
        bool trigger_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool trigger_held    = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        Vector3 shot_dir;
        bool can_fire = active_slot < 2 && !(active_slot == 0 && !has_primary);
        if (!am_dead && !round_end && !in_buy && can_fire &&
            weapon_try_fire(active_ws, trigger_pressed, trigger_held, &player, &shot_dir))
        {
            /* Local visual: bullet hole */
            RayHit hit = map_raycast(&map, player.position, shot_dir);
            if (hit.hit && hole_count < MAX_HOLES) {
                holes[hole_count].pos    = hit.point;
                holes[hole_count].normal = hit.normal;
                hole_count++;
            }
            /* Network: send shot direction to server */
            if (net.connected)
                net_client_shoot(&net, shot_dir.x, shot_dir.y, shot_dir.z);
        }

        /* --- Network: send input, receive world state --- */
        if (net.connected) {
            uint8_t buttons = 0;
            if (!am_dead && !round_end) {
                if (IsKeyDown(KEY_W))            buttons |= BTN_FORWARD;
                if (IsKeyDown(KEY_S))            buttons |= BTN_BACK;
                if (IsKeyDown(KEY_A))            buttons |= BTN_LEFT;
                if (IsKeyDown(KEY_D))            buttons |= BTN_RIGHT;
                if (IsKeyDown(KEY_SPACE))        buttons |= BTN_JUMP;
                if (IsKeyDown(KEY_LEFT_CONTROL)) buttons |= BTN_CROUCH;
            }
            net_client_send_input(&net, buttons, player.yaw, player.pitch,
                                  player.position.x, player.position.y, player.position.z);
            net_client_recv(&net);

            am_dead   = net.remote[net.my_id].flags & 1;
            round_end = net.round_phase == PHASE_END;
            in_buy    = net.round_phase == PHASE_BUY;
        }

        /* --- Draw --- */
        draw:
        BeginDrawing();
            ClearBackground((Color){ 20, 20, 20, 255 });

            BeginMode3D(cam);
                map_draw(&map);

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

                if (net.connected) {
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (!net.remote[i].active || i == net.my_id) continue;
                        if (net.remote[i].flags & 1) continue;
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

            /* --- Low HP vignette --- */
            {
                int hp = net.connected ? (int)net.remote[net.my_id].health : 100;
                if (hp > 0 && hp <= 25 && !am_dead) {
                    float t = 1.0f - (float)hp / 25.0f;
                    float thick = t * 38.0f + 6.0f;
                    DrawRectangleLinesEx((Rectangle){0, 0, (float)sw, (float)sh},
                                        thick, (Color){200, 0, 0, (unsigned char)(t * 150)});
                }
            }

            /* --- Crosshair (4-line gap style) — hidden when AWP scoped --- */
            if (!am_dead && !in_buy && !paused && !awp_zoomed) {
                int gap = 5, len = 9;
                DrawLine(cx - gap - len, cy, cx - gap,       cy, (Color){0,0,0,160});
                DrawLine(cx + gap,       cy, cx + gap + len, cy, (Color){0,0,0,160});
                DrawLine(cx, cy - gap - len, cx, cy - gap,       (Color){0,0,0,160});
                DrawLine(cx, cy + gap,       cx, cy + gap + len, (Color){0,0,0,160});
                DrawLine(cx - gap - len + 1, cy, cx - gap,           cy, WHITE);
                DrawLine(cx + gap,           cy, cx + gap + len - 1, cy, WHITE);
                DrawLine(cx, cy - gap - len + 1, cx, cy - gap,           WHITE);
                DrawLine(cx, cy + gap,           cx, cy + gap + len - 1, WHITE);
            }

            /* --- AWP scope overlay --- */
            if (awp_zoomed && !am_dead && !paused) {
                int scope_r = sh * 42 / 100;
                Vector2 ctr = { (float)cx, (float)cy };
                float outer = (float)(sw + sh);  /* large enough to fill screen */
                /* Black ring — masks everything outside the lens */
                DrawRing(ctr, (float)scope_r, outer, 0, 360, 128, (Color){0,0,0,255});
                /* Clean cross — two lines, nothing else */
                DrawLine(cx - scope_r, cy, cx + scope_r, cy, (Color){0,0,0,210});
                DrawLine(cx, cy - scope_r, cx, cy + scope_r, (Color){0,0,0,210});
            }

            /* --- Top center: timer, round dots, team --- */
            if (net.connected && !round_end) {
                int total_secs = (int)net.round_timer;
                const char *timer_str = TextFormat("%d:%02d", total_secs / 60, total_secs % 60);
                int tw = MeasureText(timer_str, 34);
                Color tc = (!in_buy && total_secs <= 10) ? (Color){255,60,60,255} : RAYWHITE;
                DrawText(timer_str, cx - tw / 2, 8, 34, tc);
            }

            if (net.connected) {
                /* Round win dots — 16 per side */
                int dr = 4, dp = 3, dy = 52, nd = 16;
                for (int i = 0; i < nd; i++) {
                    int dx = cx - 6 - (nd - 1 - i) * (dr*2 + dp) - dr;
                    Color dc = i < (int)net.ct_score ? (Color){80,130,255,255} : (Color){45,45,50,220};
                    DrawCircle(dx, dy, dr, dc);
                }
                for (int i = 0; i < nd; i++) {
                    int dx = cx + 6 + i * (dr*2 + dp) + dr;
                    Color dc = i < (int)net.t_score ? (Color){255,80,80,255} : (Color){45,45,50,220};
                    DrawCircle(dx, dy, dr, dc);
                }

                bool my_ct = (net.remote[net.my_id].team == TEAM_CT);
                const char *team_str = my_ct ? "CT" : "T";
                Color team_col = my_ct ? (Color){80,130,255,255} : (Color){255,80,80,255};
                int tlw = MeasureText(team_str, 16);
                DrawText(team_str, cx - tlw/2, 62, 16, team_col);
            }

            if (in_buy) {
                const char *s = "BUY PHASE  [B] open menu  [R] reload";
                int tw = MeasureText(s, 18);
                DrawText(s, cx - tw/2, 84, 18, YELLOW);
            }

            /* --- Dev stats: top left, muted --- */
            DrawFPS(10, 10);
            {
                float hspeed = sqrtf(player.velocity.x * player.velocity.x +
                                     player.velocity.z * player.velocity.z);
                DrawText(TextFormat("%.0f u/s", hspeed), 10, 34, 16, (Color){130,130,130,200});
                if (player.crouching)
                    DrawText("CROUCH", 10, 52, 16, (Color){130,130,130,200});
            }

            /* --- Bottom HUD --- */
            if (!paused) {
                int bh = 54, by = sh - bh - 12;

                /* Left box: HP + money */
                {
                    int hp = net.connected ? (int)net.remote[net.my_id].health : 100;
                    const char *hp_str  = TextFormat("%d", hp);
                    const char *mon_str = net.connected
                        ? TextFormat("$%d", (int)net.remote[net.my_id].money) : "";
                    int hw = MeasureText(hp_str, 30);
                    int mw = MeasureText(mon_str, 16);
                    int lbw = (hw > mw ? hw : mw) + 44;
                    DrawRectangleRounded((Rectangle){12, (float)by, (float)lbw, (float)bh},
                                        0.18f, 6, (Color){0,0,0,90});
                    Color hpc = hp <= 25 ? (Color){255,60,60,255} : RAYWHITE;
                    DrawText(hp_str, 20, by + 6, 30, hpc);
                    DrawText("HP", 24 + hw, by + 18, 13, (Color){110,110,110,200});
                    DrawText(mon_str, 20, by + 37, 16, (Color){65,195,65,255});
                }

                /* Three weapon slot boxes (right side) */
                {
                    int sbw = 108, sgap = 5;
                    int total_sw = 3 * sbw + 2 * sgap;
                    int sbx0 = sw - 12 - total_sw;

                    for (int slot = 0; slot < 3; slot++) {
                        int sbx = sbx0 + slot * (sbw + sgap);
                        bool is_active = (slot == active_slot);
                        Color bg = is_active ? (Color){0,0,0,130} : (Color){0,0,0,65};
                        DrawRectangleRounded((Rectangle){(float)sbx,(float)by,(float)sbw,(float)bh},
                                            0.18f, 6, bg);
                        if (is_active)
                            DrawRectangleRoundedLinesEx(
                                (Rectangle){(float)sbx,(float)by,(float)sbw,(float)bh},
                                0.18f, 6, 1.5f, (Color){160,160,160,180});

                        Color tc = is_active ? RAYWHITE : (Color){100,100,100,200};
                        char slot_key[2] = { '1' + slot, 0 };
                        DrawText(slot_key, sbx + 6, by + 4, 11, (Color){90,90,90,200});

                        if (slot == 2) {
                            DrawText("KNIFE", sbx + 6, by + 20, 15, tc);
                        } else if (slot == 0 && !has_primary) {
                            DrawText("--", sbx + 6, by + 20, 15, (Color){70,70,70,200});
                        } else {
                            WeaponState *ws2 = &slots[slot];
                            DrawText(WEAPONS[ws2->weapon_id].name, sbx + 6, by + 18, 15, tc);
                            if (ws2->reload_timer > 0.0f) {
                                /* Reload progress bar */
                                float prog = 1.0f - ws2->reload_timer /
                                             WEAPONS[ws2->weapon_id].reload_time;
                                if (prog < 0.0f) prog = 0.0f;
                                DrawRectangle(sbx+6, by+bh-10, sbw-12, 5, (Color){50,50,50,180});
                                DrawRectangle(sbx+6, by+bh-10, (int)((sbw-12)*prog), 5,
                                              (Color){200,180,60,220});
                            } else {
                                char mag_buf[8], res_buf[12];
                                snprintf(mag_buf, sizeof(mag_buf), "%d", ws2->ammo_mag);
                                snprintf(res_buf, sizeof(res_buf), "/%d", ws2->ammo_reserve);
                                Color mc = ws2->ammo_mag == 0 ? RED : tc;
                                DrawText(mag_buf, sbx + 6,  by + 34, 14, mc);
                                DrawText(res_buf, sbx + 34, by + 36, 12, (Color){90,90,90,200});
                            }
                        }
                    }
                }
            }

            /* --- Spectator overlay --- */
            if (am_dead) {
                DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 80});
                const char *s = "SPECTATING";
                int tw = MeasureText(s, 32);
                DrawText(s, cx - tw/2, cy - 60, 32, (Color){220, 220, 220, 200});
            }

            /* Buy menu overlay */
            if (buy_menu_open) {
                int my_money = net.connected ? (int)net.remote[net.my_id].money : 0;
                uint8_t cur_weapon = net.connected ? net.remote[net.my_id].weapon
                                                   : active_ws->weapon_id;
                int bw = 380, bh = 70 + WEAPON_COUNT * 36 + 28;
                int bx = cx - bw / 2, by = cy - bh / 2;
                DrawRectangle(bx, by, bw, bh, (Color){ 0, 0, 0, 220 });
                DrawRectangleLines(bx, by, bw, bh, GRAY);

                DrawText("BUY MENU", bx + 12, by + 12, 22, YELLOW);
                DrawText(TextFormat("Money: $%d", my_money), bx + 12, by + 36, 18, GREEN);

                for (int i = 0; i < WEAPON_COUNT; i++) {
                    int wy = by + 68 + i * 36;
                    bool owned = (cur_weapon == (uint8_t)i);
                    bool affordable = owned
                        ? (my_money >= WEAPONS[i].ammo_price)
                        : (my_money >= WEAPONS[i].price);
                    Color wc = affordable ? WHITE : DARKGRAY;

                    const char *action;
                    if (owned)
                        action = (WEAPONS[i].ammo_price == 0)
                                 ? "FREE  [equipped]"
                                 : TextFormat("REFILL  $%d", WEAPONS[i].ammo_price);
                    else
                        action = (WEAPONS[i].price == 0)
                                 ? "FREE"
                                 : TextFormat("$%d", WEAPONS[i].price);

                    DrawText(TextFormat("[%d] %-8s  %d+%d dmg  %s",
                             i + 1, WEAPONS[i].name,
                             WEAPONS[i].ammo_mag, WEAPONS[i].ammo_reserve,
                             action),
                             bx + 12, wy, 20, wc);
                }
                DrawText("[B/ESC] Close", bx + 12, by + bh - 22, 16, DARKGRAY);
            }

            /* F11 hint (fades out after 5 s) */
            if (hint_timer > 0.0f) {
                hint_timer -= dt;
                float alpha = hint_timer > 1.0f ? 1.0f : hint_timer;
                unsigned char a = (unsigned char)(alpha * 180);
                const char *hint = "F11  —  toggle windowed";
                int hw = MeasureText(hint, 18);
                DrawText(hint, cx - hw / 2, sh - 68, 15, (Color){ 160, 160, 160, a });
            }

            /* Round-end scoreboard */
            if (round_end) {
                int bw = 360, bh = 160;
                int bx = cx - bw / 2, by = cy - bh / 2;
                DrawRectangle(bx, by, bw, bh, (Color){ 0, 0, 0, 200 });
                DrawRectangleLines(bx, by, bw, bh, GRAY);

                const char *winner;
                Color wc;
                if (net.win_team == 1)      { winner = "CT WIN";   wc = (Color){80,130,255,255}; }
                else if (net.win_team == 2) { winner = "T WIN";    wc = (Color){255,80,80,255};  }
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

            /* Pause menu */
            if (paused) {
                DrawRectangle(0, 0, sw, sh, (Color){ 0, 0, 0, 160 });
                int pw = 300, ph = 160;
                int px = cx - pw / 2, py = cy - ph / 2;
                DrawRectangle(px, py, pw, ph, (Color){ 15, 15, 15, 240 });
                DrawRectangleLines(px, py, pw, ph, (Color){ 80, 80, 80, 255 });

                const char *title = "PAUSED";
                int tw = MeasureText(title, 28);
                DrawText(title, cx - tw / 2, py + 18, 28, RAYWHITE);

                DrawLine(px + 16, py + 54, px + pw - 16, py + 54, (Color){ 60, 60, 60, 255 });

                const char *resume_str = "[ESC]  Resume";
                tw = MeasureText(resume_str, 20);
                DrawText(resume_str, cx - tw / 2, py + 68, 20, LIGHTGRAY);

                const char *quit_str = "[Q]  Quit";
                tw = MeasureText(quit_str, 20);
                DrawText(quit_str, cx - tw / 2, py + 104, 20, (Color){ 200, 80, 80, 255 });

                if (IsKeyPressed(KEY_Q)) {
                    EndDrawing();
                    goto quit;
                }
            }

        EndDrawing();
    }

    quit:
    if (net.connected) net_client_disconnect(&net);
    weapon_free(&slots[0]);
    weapon_free(&slots[1]);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
