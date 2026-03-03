#include "map.h"
#include <math.h>

#define WALL_THICK  32
#define ROOM_W     400   /* half-width in X */
#define ROOM_D     400   /* half-depth in Z */
#define ROOM_H     160   /* full height */

static bool boxes_overlap(Vector3 amin, Vector3 amax, Vector3 bmin, Vector3 bmax)
{
    return amin.x < bmax.x && amax.x > bmin.x &&
           amin.y < bmax.y && amax.y > bmin.y &&
           amin.z < bmax.z && amax.z > bmin.z;
}

void map_build(Map *m)
{
    m->count = 0;

    static const Color FLOOR  = { 108, 104,  96, 255 };
    static const Color WALL   = {  82,  79,  74, 255 };
    static const Color CEIL   = {  62,  60,  56, 255 };
    static const Color CRATE  = { 128, 106,  70, 255 };
    static const Color PILLAR = {  80,  78,  74, 255 };

#define BOX(x0,y0,z0, x1,y1,z1, col) \
    m->boxes[m->count++] = (MapBox){ {x0,y0,z0}, {x1,y1,z1}, col }

    /* Room shell */
    BOX(-ROOM_W-WALL_THICK, -WALL_THICK, -ROOM_D-WALL_THICK,
         ROOM_W+WALL_THICK,           0,  ROOM_D+WALL_THICK, FLOOR);
    BOX(-ROOM_W-WALL_THICK,  ROOM_H,    -ROOM_D-WALL_THICK,
         ROOM_W+WALL_THICK,  ROOM_H+WALL_THICK, ROOM_D+WALL_THICK, CEIL);
    BOX(-ROOM_W-WALL_THICK, 0, -ROOM_D-WALL_THICK,
         ROOM_W+WALL_THICK, ROOM_H,          -ROOM_D, WALL);  /* north */
    BOX(-ROOM_W-WALL_THICK, 0,  ROOM_D,
         ROOM_W+WALL_THICK, ROOM_H,  ROOM_D+WALL_THICK, WALL);  /* south */
    BOX(-ROOM_W-WALL_THICK, 0, -ROOM_D,
        -ROOM_W,            ROOM_H,  ROOM_D, WALL);  /* west */
    BOX( ROOM_W,            0, -ROOM_D,
         ROOM_W+WALL_THICK, ROOM_H,  ROOM_D, WALL);  /* east */

    /* Obstacles */
    BOX(-100, 0, -100,  -36, 64,  -36, CRATE);          /* crate A */
    BOX(  36, 0,   36,  100, 64,  100, CRATE);          /* crate B */
    BOX(-200, 0,  168, -168, ROOM_H, 200, PILLAR);      /* tall pillar */
    BOX( 150, 0, -300,  300,  32, -150, CRATE);         /* low platform */

#undef BOX
}

void map_draw(const Map *m)
{
    for (int i = 0; i < m->count; i++) {
        const MapBox *b = &m->boxes[i];
        Vector3 center = {
            (b->min.x + b->max.x) * 0.5f,
            (b->min.y + b->max.y) * 0.5f,
            (b->min.z + b->max.z) * 0.5f
        };
        Vector3 size = {
            b->max.x - b->min.x,
            b->max.y - b->min.y,
            b->max.z - b->min.z
        };
        DrawCubeV(center, size, b->color);
    }
}

void map_collide(const Map *m, PlayerState *p)
{
    float eye_h = p->crouching ? PLAYER_EYE_CROUCH : PLAYER_EYE_STAND;

    /*
     * Resolve penetrations. Three passes handles corner cases where
     * pushing out of one box puts you inside another.
     */
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < m->count; i++) {
            Vector3 pmin = { p->position.x - PLAYER_HALF_WIDTH,
                             p->position.y - eye_h,
                             p->position.z - PLAYER_HALF_WIDTH };
            Vector3 pmax = { p->position.x + PLAYER_HALF_WIDTH,
                             p->position.y,
                             p->position.z + PLAYER_HALF_WIDTH };

            if (!boxes_overlap(pmin, pmax, m->boxes[i].min, m->boxes[i].max))
                continue;

            float ox = fminf(pmax.x, m->boxes[i].max.x) - fmaxf(pmin.x, m->boxes[i].min.x);
            float oy = fminf(pmax.y, m->boxes[i].max.y) - fmaxf(pmin.y, m->boxes[i].min.y);
            float oz = fminf(pmax.z, m->boxes[i].max.z) - fmaxf(pmin.z, m->boxes[i].min.z);

            /* Player and box centers for push direction */
            float pcx = p->position.x;
            float pcy = p->position.y - eye_h * 0.5f;
            float pcz = p->position.z;
            float bcx = (m->boxes[i].min.x + m->boxes[i].max.x) * 0.5f;
            float bcy = (m->boxes[i].min.y + m->boxes[i].max.y) * 0.5f;
            float bcz = (m->boxes[i].min.z + m->boxes[i].max.z) * 0.5f;

            /* Push out along the axis with smallest overlap */
            if (ox <= oy && ox <= oz) {
                float s = pcx < bcx ? -1.0f : 1.0f;
                p->position.x += s * ox;
                p->velocity.x  = 0.0f;
            } else if (oz <= oy) {
                float s = pcz < bcz ? -1.0f : 1.0f;
                p->position.z += s * oz;
                p->velocity.z  = 0.0f;
            } else {
                float s = pcy < bcy ? -1.0f : 1.0f;
                p->position.y += s * oy;
                if (s > 0.0f && p->velocity.y < 0.0f) p->velocity.y = 0.0f; /* floor */
                if (s < 0.0f && p->velocity.y > 0.0f) p->velocity.y = 0.0f; /* ceiling */
            }
        }
    }

    /* Ground check: thin slice just below the player's feet */
    {
        float feet_y = p->position.y - eye_h;
        Vector3 gmin = { p->position.x - PLAYER_HALF_WIDTH, feet_y - 2.0f,  p->position.z - PLAYER_HALF_WIDTH };
        Vector3 gmax = { p->position.x + PLAYER_HALF_WIDTH, feet_y + 0.5f,  p->position.z + PLAYER_HALF_WIDTH };
        p->on_ground = false;
        for (int i = 0; i < m->count; i++) {
            if (boxes_overlap(gmin, gmax, m->boxes[i].min, m->boxes[i].max)) {
                p->on_ground = true;
                break;
            }
        }
    }
}
