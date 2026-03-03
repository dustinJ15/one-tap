#include "raylib.h"
#include "physics/movement.h"
#include "render/map.h"
#include <math.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "one-tap"

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    SetTargetFPS(0);  /* uncapped */
    DisableCursor();

    PlayerState player;
    player_init(&player);

    Map map;
    map_build(&map);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        player_update(&player, dt);
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);

        BeginDrawing();
            ClearBackground((Color){ 20, 20, 20, 255 });

            BeginMode3D(cam);
                map_draw(&map);
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

    CloseWindow();
    return 0;
}
