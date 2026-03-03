#include "raylib.h"
#include "physics/movement.h"
#include "render/map.h"
#include "game/weapons.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "one-tap"
#define MAX_HOLES     512

typedef struct { Vector3 pos; Vector3 normal; } BulletHole;

int main(void)
{
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

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        player_update(&player, dt);
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);

        /* Shoot on left click */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            RayHit hit = weapon_shoot(&weapon, &map, &player);
            if (hit.hit && hole_count < MAX_HOLES) {
                holes[hole_count].pos    = hit.point;
                holes[hole_count].normal = hit.normal;
                hole_count++;
            }
        }

        BeginDrawing();
            ClearBackground((Color){ 20, 20, 20, 255 });

            BeginMode3D(cam);
                map_draw(&map);

                /* Bullet holes: thin dark slab flush to the hit surface */
                for (int i = 0; i < hole_count; i++) {
                    Vector3 n = holes[i].normal;
                    /* Offset slightly along normal to avoid z-fighting */
                    Vector3 p = { holes[i].pos.x + n.x * 0.3f,
                                  holes[i].pos.y + n.y * 0.3f,
                                  holes[i].pos.z + n.z * 0.3f };
                    Vector3 size;
                    if      (fabsf(n.y) > 0.5f) size = (Vector3){ 5.0f, 0.2f, 5.0f };
                    else if (fabsf(n.x) > 0.5f) size = (Vector3){ 0.2f, 5.0f, 5.0f };
                    else                         size = (Vector3){ 5.0f, 5.0f, 0.2f };
                    DrawCubeV(p, size, (Color){ 12, 12, 12, 255 });
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

        EndDrawing();
    }

    weapon_free(&weapon);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
