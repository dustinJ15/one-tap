#include "raylib.h"
#include "rlgl.h"
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

#define MAX_TRACERS  64
#define TRACER_LIFE  0.15f
typedef struct { Vector3 origin; Vector3 endpoint; float born; } Tracer;

/*
 * Draw a blocky gun model in local gun-space.
 * Origin is the grip/handle position. -Z = forward (muzzle), +Y = up.
 * Muzzle tip distances: Pistol≈20  AK≈44  M4≈44  AWP≈57
 */
static void draw_gun(WeaponId wid, Vector3 origin, float yaw, float pitch, const WeaponState *ws)
{
    /* Material palette — desaturated but distinct enough to read materials */
    Color c_dark    = { 20, 20, 22, 255 };  /* near-black: bores, deep shadow, serrations */
    Color c_steel   = { 40, 41, 45, 255 };  /* dark steel: slides, barrels, receivers */
    Color c_metal   = { 60, 62, 67, 255 };  /* gunmetal mid: dust covers, guards */
    Color c_light   = { 88, 91, 96, 255 };  /* polished highlight: rails, flats, blade face */
    Color c_polymer = { 28, 29, 32, 255 };  /* matte black polymer: grips, stocks */
    Color c_wood    = { 66, 44, 20, 255 };  /* walnut: AK handguard, stock */

    rlPushMatrix();
    rlTranslatef(origin.x, origin.y, origin.z);

    /* Melee swiping animation (procedural) */
    if (wid == WEAPON_KNIFE && ws->swing_timer > 0.0f) {
        float t = ws->swing_timer;
        if (ws->swing_type == 1) { /* Light M1: CS-style right-to-left arm sweep */
            float phase = 1.0f - (t / 0.35f);
            float bell  = sinf(phase * PI);
            rlTranslatef(-bell * 20.0f, bell * 3.0f, -bell * 10.0f);
            rlRotatef(-bell * 12.0f, 0.0f, 0.0f, 1.0f);
        } else { /* Heavy M2: same sweep as M1 but slower */
            float phase = 1.0f - (t / 0.65f);
            float bell  = sinf(phase * PI);
            rlTranslatef(-bell * 20.0f, bell * 3.0f, -bell * 10.0f);
            rlRotatef(-bell * 12.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    rlRotatef(-yaw,   0.0f, 1.0f, 0.0f);
    rlRotatef( pitch, 1.0f, 0.0f, 0.0f);

    switch (wid) {

        /* ============================================================
         * PISTOL — Glock 18
         * Slide runs from Z≈+5 (rear) to Z≈-16 (front).
         * Grip drops from Y≈0 down to Y≈-14.
         * ============================================================ */
        case WEAPON_PISTOL:
            /* Slide — main steel upper */
            DrawCubeV((Vector3){ 0,  2.6f, -6.5f}, (Vector3){3.0f, 3.8f, 14.0f}, c_steel);  /* slide body */
            DrawCubeV((Vector3){ 0,  2.6f,-14.5f}, (Vector3){2.7f, 3.5f,  4.0f}, c_steel);  /* slide nose (tapers) */
            DrawCubeV((Vector3){ 0,  2.6f,  3.5f}, (Vector3){3.2f, 3.8f,  3.5f}, c_steel);  /* slide rear / serration block */
            DrawCubeV((Vector3){ 0,  2.6f,  3.5f}, (Vector3){1.6f, 4.0f,  3.8f}, c_dark);   /* rear serration cuts */
            DrawCubeV((Vector3){ 0,  4.3f, -6.5f}, (Vector3){2.0f, 0.6f, 16.0f}, c_light);  /* slide top flat */
            DrawCubeV((Vector3){ 0,  2.6f, -2.5f}, (Vector3){2.2f, 2.2f,  6.5f}, c_dark);   /* ejection port cutout */
            /* Sights */
            DrawCubeV((Vector3){ 0,  5.0f,-13.5f}, (Vector3){0.9f, 1.4f,  1.3f}, c_dark);   /* front sight blade */
            DrawCubeV((Vector3){ 0,  5.1f,-13.5f}, (Vector3){0.3f, 0.5f,  0.3f}, (Color){200,200,200,255}); /* front sight dot */
            DrawCubeV((Vector3){ 0,  5.0f,  2.8f}, (Vector3){2.2f, 1.4f,  1.0f}, c_dark);   /* rear sight */
            /* Frame — polymer lower */
            DrawCubeV((Vector3){ 0,  0.7f, -5.0f}, (Vector3){3.2f, 2.5f,  8.5f}, c_metal);  /* frame body */
            DrawCubeV((Vector3){ 0,  0.7f,-11.5f}, (Vector3){3.0f, 2.2f,  3.5f}, c_metal);  /* frame nose */
            DrawCubeV((Vector3){ 0,  0.2f, -9.8f}, (Vector3){2.5f, 1.0f,  2.2f}, c_dark);   /* accessory rail */
            DrawCubeV((Vector3){ 0, -1.0f, -8.5f}, (Vector3){3.2f, 1.8f,  1.2f}, c_metal);  /* trigger guard front bar */
            DrawCubeV((Vector3){ 0, -1.0f, -1.0f}, (Vector3){3.2f, 1.8f,  7.5f}, c_metal);  /* trigger guard sweep */
            DrawCubeV((Vector3){ 0, -2.3f, -4.5f}, (Vector3){1.5f, 1.0f,  2.2f}, c_dark);   /* trigger blade */
            /* Grip */
            DrawCubeV((Vector3){ 0, -7.5f,  1.8f}, (Vector3){3.0f,11.5f,  5.5f}, c_polymer);/* grip body */
            DrawCubeV((Vector3){ 0,-13.5f,  1.8f}, (Vector3){3.4f, 2.0f,  5.0f}, c_dark);   /* magazine baseplate */
            DrawCubeV((Vector3){ 0, -7.5f, -1.2f}, (Vector3){2.6f, 9.5f,  0.6f}, c_dark);   /* front strap */
            DrawCubeV((Vector3){ 0, -7.5f,  4.7f}, (Vector3){2.6f, 9.5f,  0.6f}, c_dark);   /* backstrap */
            DrawCubeV((Vector3){ 0, -7.5f,  1.8f}, (Vector3){0.4f,11.5f,  5.7f}, c_dark);   /* grip side panel line */
            /* Barrel stub visible at muzzle */
            DrawCubeV((Vector3){ 0,  2.6f,-18.5f}, (Vector3){1.1f, 1.1f,  3.5f}, c_dark);   /* barrel crown */
            break;

        /* ============================================================
         * AK-47
         * Barrel tip ≈ Z=-44. Stock rear ≈ Z=+24.
         * Most recognisable: curved mag, wood furniture, gas tube.
         * ============================================================ */
        case WEAPON_AK:
            /* Muzzle device */
            DrawCubeV((Vector3){ 0,  0.8f,-42.5f}, (Vector3){2.2f, 2.2f,  4.0f}, c_dark);   /* slant compensator */
            DrawCubeV((Vector3){ 0,  0.8f,-44.0f}, (Vector3){0.9f, 0.9f,  1.5f}, c_dark);   /* bore */
            /* Barrel */
            DrawCubeV((Vector3){ 0,  0.8f,-23.0f}, (Vector3){1.5f, 1.5f, 42.0f}, c_steel);  /* barrel */
            DrawCubeV((Vector3){ 0,  0.8f,-23.0f}, (Vector3){0.7f, 0.7f, 42.5f}, c_dark);   /* bore channel */
            /* Gas system */
            DrawCubeV((Vector3){ 0,  3.2f,-17.0f}, (Vector3){1.2f, 1.2f, 30.0f}, c_steel);  /* gas tube */
            DrawCubeV((Vector3){ 0,  2.0f,-31.0f}, (Vector3){2.0f, 4.5f,  3.0f}, c_dark);   /* gas block */
            DrawCubeV((Vector3){ 0,  3.2f,-13.5f}, (Vector3){1.8f, 2.5f,  2.0f}, c_dark);   /* gas tube bracket */
            /* Front sight tower — very recognisable AK feature */
            DrawCubeV((Vector3){ 0,  0.8f,-40.0f}, (Vector3){3.0f, 1.5f,  2.5f}, c_dark);   /* sight base */
            DrawCubeV((Vector3){ 0,  4.2f,-40.0f}, (Vector3){0.7f, 5.5f,  1.0f}, c_dark);   /* sight post */
            DrawCubeV((Vector3){ 0,  6.5f,-40.0f}, (Vector3){2.5f, 1.5f,  1.8f}, c_dark);   /* sight hood */
            /* Receiver */
            DrawCubeV((Vector3){ 0,  0.0f, -2.5f}, (Vector3){4.8f, 5.8f, 25.0f}, c_steel);  /* lower receiver */
            DrawCubeV((Vector3){ 0,  3.8f, -2.5f}, (Vector3){3.6f, 2.5f, 25.0f}, c_metal);  /* dust cover (rounds over top) */
            DrawCubeV((Vector3){ 0,  0.5f, -3.0f}, (Vector3){2.8f, 4.5f,  7.0f}, c_dark);   /* ejection port */
            DrawCubeV((Vector3){ 0,  2.2f,  3.5f}, (Vector3){6.2f, 1.5f,  2.5f}, c_steel);  /* bolt carrier handle */
            DrawCubeV((Vector3){ 0,  2.2f,  3.5f}, (Vector3){5.0f, 1.2f,  2.0f}, c_dark);   /* handle shadow groove */
            DrawCubeV((Vector3){ 0,  0.0f,-14.5f}, (Vector3){4.8f, 0.8f,  2.0f}, c_dark);   /* receiver step / handguard cap */
            /* Handguard — wood */
            DrawCubeV((Vector3){ 0, -0.5f,-12.5f}, (Vector3){4.5f, 4.5f, 12.0f}, c_wood);   /* lower handguard */
            DrawCubeV((Vector3){ 0,  2.8f,-12.5f}, (Vector3){3.8f, 2.2f, 11.0f}, c_wood);   /* upper handguard */
            DrawCubeV((Vector3){ 0,  2.8f,-12.5f}, (Vector3){3.2f, 0.6f, 11.5f}, c_dark);   /* upper handguard groove */
            /* Grip — bakelite */
            DrawCubeV((Vector3){ 0, -8.5f,  1.5f}, (Vector3){3.5f,12.0f,  5.2f}, c_polymer);/* grip */
            DrawCubeV((Vector3){ 0,-14.2f,  1.5f}, (Vector3){3.0f, 2.0f,  4.8f}, c_dark);   /* grip cap */
            DrawCubeV((Vector3){ 0, -8.5f, -1.4f}, (Vector3){2.8f,10.0f,  0.7f}, c_dark);   /* grip front strap */
            /* Banana magazine — the most iconic AK feature */
            DrawCubeV((Vector3){ 0,-10.5f, -7.0f}, (Vector3){3.2f,13.0f,  4.8f}, c_dark);   /* mag straight section */
            DrawCubeV((Vector3){ 0,-17.0f,-11.0f}, (Vector3){3.2f, 7.5f,  4.8f}, c_dark);   /* mag curve */
            DrawCubeV((Vector3){ 0,-22.5f,-13.5f}, (Vector3){3.2f, 4.0f,  4.5f}, c_dark);   /* mag tip */
            DrawCubeV((Vector3){ 0,-10.5f, -7.0f}, (Vector3){1.6f,13.5f,  3.5f}, c_metal);  /* mag body highlight */
            DrawCubeV((Vector3){ 0,-24.5f,-13.5f}, (Vector3){3.4f, 2.0f,  4.2f}, c_metal);  /* mag baseplate */
            /* Stock — straight wood */
            DrawCubeV((Vector3){ 0, -0.5f, 14.5f}, (Vector3){4.2f, 5.5f, 18.0f}, c_wood);   /* stock body */
            DrawCubeV((Vector3){ 0,  2.3f, 14.5f}, (Vector3){3.8f, 1.2f, 17.5f}, c_dark);   /* stock top groove */
            DrawCubeV((Vector3){ 0, -0.5f, 24.0f}, (Vector3){4.4f, 6.5f,  2.5f}, c_dark);   /* butt plate */
            break;

        /* ============================================================
         * M4A1-S  (suppressed)
         * Suppressor tip ≈ Z=-44. Stock ≈ Z=+26.
         * Key reads: fat suppressor, flat-top rail, collapsible stock.
         * ============================================================ */
        case WEAPON_M4:
            /* Suppressor — large, defines the silhouette */
            DrawCubeV((Vector3){ 0,  0.5f,-36.5f}, (Vector3){3.2f, 3.2f, 12.0f}, c_steel);  /* suppressor body */
            DrawCubeV((Vector3){ 0,  0.5f,-43.5f}, (Vector3){2.8f, 2.8f,  3.0f}, c_steel);  /* suppressor end cap */
            DrawCubeV((Vector3){ 0,  0.5f,-44.5f}, (Vector3){1.0f, 1.0f,  1.0f}, c_dark);   /* bore */
            DrawCubeV((Vector3){ 0,  0.5f,-33.5f}, (Vector3){3.8f, 3.8f,  2.5f}, c_metal);  /* suppressor mount collar */
            DrawCubeV((Vector3){ 0,  0.5f,-36.5f}, (Vector3){3.4f, 0.7f, 12.5f}, c_dark);   /* suppressor seam line */
            DrawCubeV((Vector3){ 0,  0.5f,-36.5f}, (Vector3){0.7f, 3.4f, 12.5f}, c_dark);   /* suppressor side seam */
            /* Barrel (hidden inside suppressor, stub visible) */
            DrawCubeV((Vector3){ 0,  0.5f,-22.0f}, (Vector3){1.7f, 1.7f, 20.0f}, c_steel);  /* barrel */
            DrawCubeV((Vector3){ 0,  0.5f,-22.0f}, (Vector3){0.8f, 0.8f, 20.5f}, c_dark);   /* bore */
            /* Upper receiver */
            DrawCubeV((Vector3){ 0,  0.5f, -2.0f}, (Vector3){4.4f, 6.4f, 22.0f}, c_steel);  /* upper receiver */
            DrawCubeV((Vector3){ 0,  3.8f, -2.0f}, (Vector3){3.6f, 1.2f, 22.0f}, c_light);  /* flat-top Picatinny rail */
            DrawCubeV((Vector3){ 0,  4.5f,  1.5f}, (Vector3){3.0f, 0.8f,  3.0f}, c_dark);   /* rail slot 1 */
            DrawCubeV((Vector3){ 0,  4.5f, -3.5f}, (Vector3){3.0f, 0.8f,  3.0f}, c_dark);   /* rail slot 2 */
            DrawCubeV((Vector3){ 0,  4.5f, -8.5f}, (Vector3){3.0f, 0.8f,  3.0f}, c_dark);   /* rail slot 3 */
            DrawCubeV((Vector3){ 0,  2.0f,  5.0f}, (Vector3){4.0f, 1.8f,  2.5f}, c_metal);  /* charging handle */
            DrawCubeV((Vector3){ 0,  0.5f, -4.0f}, (Vector3){2.5f, 4.5f,  4.5f}, c_dark);   /* ejection port */
            DrawCubeV((Vector3){ 0,  0.5f,-10.5f}, (Vector3){4.0f, 1.2f,  1.8f}, c_metal);  /* forward assist */
            /* Lower receiver */
            DrawCubeV((Vector3){ 0, -2.0f, -1.0f}, (Vector3){4.4f, 3.5f, 17.0f}, c_metal);  /* lower receiver */
            DrawCubeV((Vector3){ 0, -2.8f, -6.5f}, (Vector3){3.5f, 1.5f,  1.5f}, c_dark);   /* trigger guard front */
            DrawCubeV((Vector3){ 0, -2.8f,  4.0f}, (Vector3){3.5f, 1.5f,  4.0f}, c_dark);   /* trigger guard rear */
            DrawCubeV((Vector3){ 0, -3.8f, -1.5f}, (Vector3){1.8f, 1.3f,  2.8f}, c_dark);   /* trigger */
            /* M-LOK handguard */
            DrawCubeV((Vector3){ 0,  0.5f,-17.0f}, (Vector3){4.8f, 5.2f, 13.0f}, c_steel);  /* handguard body */
            DrawCubeV((Vector3){ 0,  3.0f,-17.0f}, (Vector3){4.0f, 0.8f, 13.5f}, c_dark);   /* M-LOK top slot */
            DrawCubeV((Vector3){ 0, -2.0f,-17.0f}, (Vector3){4.0f, 0.8f, 12.5f}, c_dark);   /* M-LOK bottom slot */
            DrawCubeV((Vector3){ 0,  0.5f,-22.5f}, (Vector3){5.0f, 5.8f,  2.5f}, c_dark);   /* handguard-barrel junction */
            /* Grip */
            DrawCubeV((Vector3){ 0, -8.5f,  0.0f}, (Vector3){3.4f,12.0f,  5.0f}, c_polymer);/* grip */
            DrawCubeV((Vector3){ 0,-14.2f,  0.0f}, (Vector3){3.0f, 2.0f,  4.8f}, c_dark);   /* grip cap */
            DrawCubeV((Vector3){ 0, -8.5f, -2.5f}, (Vector3){2.8f,10.0f,  0.7f}, c_dark);   /* grip front strap */
            /* Magazine — straight STANAG */
            DrawCubeV((Vector3){ 0, -8.5f, -5.5f}, (Vector3){3.0f,12.0f,  5.0f}, c_dark);   /* mag body */
            DrawCubeV((Vector3){ 0,-14.0f, -5.5f}, (Vector3){3.2f, 2.5f,  4.8f}, c_metal);  /* mag baseplate */
            DrawCubeV((Vector3){ 0, -8.5f, -5.5f}, (Vector3){1.5f,12.5f,  3.5f}, c_metal);  /* mag side highlight */
            /* Collapsible stock */
            DrawCubeV((Vector3){ 0,  0.5f, 13.5f}, (Vector3){3.8f, 3.8f, 14.0f}, c_steel);  /* buffer tube */
            DrawCubeV((Vector3){ 0,  0.5f, 13.5f}, (Vector3){4.5f, 0.8f, 14.5f}, c_dark);   /* tube seam */
            DrawCubeV((Vector3){ 0,  0.5f, 21.0f}, (Vector3){5.0f, 6.5f,  8.5f}, c_polymer);/* stock body */
            DrawCubeV((Vector3){ 0,  3.8f, 24.0f}, (Vector3){4.5f, 2.5f,  5.0f}, c_dark);   /* cheek weld */
            DrawCubeV((Vector3){ 0, -1.5f, 25.0f}, (Vector3){4.5f, 2.0f,  3.0f}, c_dark);   /* stock toe */
            DrawCubeV((Vector3){ 0,  0.5f, 26.0f}, (Vector3){5.2f, 7.5f,  2.5f}, c_dark);   /* butt pad */
            break;

        /* ============================================================
         * AWP — Arctic Warfare Police
         * Barrel+brake tip ≈ Z=-57. Stock ≈ Z=+27.
         * Key reads: massive scope, heavy barrel, thumbhole stock.
         * ============================================================ */
        case WEAPON_AWP: {
            /* Muzzle brake */
            DrawCubeV((Vector3){ 0,  0.5f,-53.5f}, (Vector3){4.0f, 4.0f,  7.0f}, c_dark);   /* brake body */
            DrawCubeV((Vector3){ 0,  0.5f,-53.5f}, (Vector3){1.4f, 1.4f,  8.0f}, c_dark);   /* bore */
            DrawCubeV((Vector3){ 0,  2.4f,-53.5f}, (Vector3){3.4f, 1.2f,  7.5f}, c_metal);  /* top port */
            DrawCubeV((Vector3){ 0, -1.4f,-53.5f}, (Vector3){3.4f, 1.2f,  7.5f}, c_metal);  /* bottom port */
            /* Heavy barrel */
            DrawCubeV((Vector3){ 0,  0.5f,-28.5f}, (Vector3){3.0f, 3.0f, 48.0f}, c_steel);  /* barrel body */
            DrawCubeV((Vector3){ 0,  0.5f,-28.5f}, (Vector3){1.2f, 1.2f, 49.0f}, c_dark);   /* bore channel */
            DrawCubeV((Vector3){ 0,  0.5f,-28.5f}, (Vector3){3.2f, 0.7f, 46.0f}, c_light);  /* barrel top flat */
            /* Action */
            DrawCubeV((Vector3){ 0,  1.0f, -2.0f}, (Vector3){5.5f, 6.5f, 24.0f}, c_steel);  /* action body */
            DrawCubeV((Vector3){ 0,  4.5f, -2.0f}, (Vector3){4.2f, 1.5f, 24.0f}, c_metal);  /* action top */
            DrawCubeV((Vector3){ 0,  1.0f, -8.0f}, (Vector3){3.5f, 4.5f,  5.5f}, c_dark);   /* ejection port */
            DrawCubeV((Vector3){ 0,  3.5f,  5.0f}, (Vector3){8.0f, 2.5f,  3.5f}, c_steel);  /* bolt handle shaft */
            DrawCubeV((Vector3){ 0,  3.5f,  5.0f}, (Vector3){9.5f, 3.0f,  2.2f}, c_metal);  /* bolt knob */
            /* Scope rail */
            DrawCubeV((Vector3){ 0,  6.5f, -3.0f}, (Vector3){3.5f, 1.5f, 22.0f}, c_metal);  /* rail base */
            DrawCubeV((Vector3){ 0,  7.2f,  1.5f}, (Vector3){3.0f, 0.8f,  3.5f}, c_dark);   /* rail slot 1 */
            DrawCubeV((Vector3){ 0,  7.2f, -5.0f}, (Vector3){3.0f, 0.8f,  3.5f}, c_dark);   /* rail slot 2 */
            /* Scope — the defining AWP feature */
            DrawCubeV((Vector3){ 0, 10.5f, -0.5f}, (Vector3){3.8f, 3.8f, 26.0f}, c_dark);   /* scope main tube */
            DrawCubeV((Vector3){ 0, 10.5f,-14.5f}, (Vector3){6.0f, 6.0f,  7.5f}, c_dark);   /* objective bell */
            DrawCubeV((Vector3){ 0, 10.5f, 12.5f}, (Vector3){5.2f, 5.2f,  7.0f}, c_dark);   /* eyepiece bell */
            DrawCubeV((Vector3){ 0, 14.0f,  1.5f}, (Vector3){3.0f, 3.5f,  3.5f}, c_metal);  /* elevation turret */
            DrawCubeV((Vector3){ 0, 10.5f, -4.5f}, (Vector3){6.0f, 2.5f,  3.5f}, c_metal);  /* windage turret */
            DrawCubeV((Vector3){ 0, 10.5f, -1.0f}, (Vector3){4.2f, 4.2f,  2.8f}, c_metal);  /* scope ring front */
            DrawCubeV((Vector3){ 0, 10.5f,  7.0f}, (Vector3){4.2f, 4.2f,  2.8f}, c_metal);  /* scope ring rear */
            /* Stock — thumbhole polymer */
            DrawCubeV((Vector3){ 0,  1.0f, 14.0f}, (Vector3){5.2f, 7.0f, 22.0f}, c_polymer);/* stock main */
            DrawCubeV((Vector3){ 0,  5.2f, 22.5f}, (Vector3){4.8f, 3.5f,  9.0f}, c_dark);   /* cheek piece */
            DrawCubeV((Vector3){ 0, -1.5f, 15.0f}, (Vector3){3.8f, 3.5f, 10.0f}, c_dark);   /* thumbhole cutout */
            DrawCubeV((Vector3){ 0,  1.0f, 26.5f}, (Vector3){5.8f, 8.5f,  2.5f}, c_dark);   /* butt plate */
            /* Grip */
            DrawCubeV((Vector3){ 0, -8.5f,  1.5f}, (Vector3){4.0f,12.5f,  5.5f}, c_polymer);/* grip */
            DrawCubeV((Vector3){ 0,-14.5f,  1.5f}, (Vector3){3.8f, 2.0f,  5.2f}, c_dark);   /* grip cap */
            DrawCubeV((Vector3){ 0, -8.5f, -1.8f}, (Vector3){3.2f,10.5f,  0.8f}, c_dark);   /* grip front strap */
            /* Box magazine */
            DrawCubeV((Vector3){ 0, -5.5f, -5.5f}, (Vector3){3.4f, 8.5f,  5.2f}, c_dark);   /* mag body */
            DrawCubeV((Vector3){ 0,-10.0f, -5.5f}, (Vector3){3.4f, 2.5f,  5.0f}, c_metal);  /* mag baseplate */
            DrawCubeV((Vector3){ 0, -5.5f, -5.5f}, (Vector3){1.8f, 8.5f,  3.5f}, c_metal);  /* mag highlight */
            break;
        }

        /* ============================================================
         * KNIFE — CS default knife style
         * Posed: blade pointing up and in, handle lower-right.
         * ============================================================ */
        case WEAPON_KNIFE: {
            rlRotatef(25.0f, 0.0f, 0.0f, 1.0f);  /* slant toward center */
            rlRotatef(45.0f, 1.0f, 0.0f, 0.0f);  /* point UP */
            /* Handle */
            DrawCubeV((Vector3){ 0,  0.0f,  6.5f}, (Vector3){2.8f, 3.5f, 12.0f}, c_polymer);/* handle body */
            DrawCubeV((Vector3){ 0,  0.0f,  6.5f}, (Vector3){0.5f, 3.7f, 12.5f}, c_metal);  /* spine strip */
            DrawCubeV((Vector3){ 0,  0.0f,  6.5f}, (Vector3){2.9f, 0.6f, 11.5f}, c_dark);   /* bottom groove */
            DrawCubeV((Vector3){ 0,  0.0f,  6.5f}, (Vector3){0.4f, 3.5f,  2.0f}, c_dark);   /* grip checkering slot 1 */
            DrawCubeV((Vector3){ 0,  0.0f,  9.5f}, (Vector3){0.4f, 3.5f,  2.0f}, c_dark);   /* grip checkering slot 2 */
            DrawCubeV((Vector3){ 0,  0.0f, 12.2f}, (Vector3){3.2f, 4.0f,  2.5f}, c_metal);  /* pommel */
            DrawCubeV((Vector3){ 0,  0.0f, 13.5f}, (Vector3){3.6f, 4.5f,  1.5f}, c_dark);   /* pommel cap */
            /* Guard */
            DrawCubeV((Vector3){ 0,  0.0f,  0.5f}, (Vector3){5.8f, 5.2f,  2.0f}, c_steel);  /* guard main */
            DrawCubeV((Vector3){ 0,  0.0f,  0.5f}, (Vector3){6.2f, 1.8f,  2.2f}, c_dark);   /* guard lower quillon */
            DrawCubeV((Vector3){ 0,  0.0f,  0.5f}, (Vector3){1.0f, 5.8f,  2.2f}, c_dark);   /* guard spine */
            /* Ricasso (unsharpened section near guard) */
            DrawCubeV((Vector3){ 0,  0.0f, -3.0f}, (Vector3){1.0f, 4.2f,  6.0f}, c_steel);  /* ricasso */
            DrawCubeV((Vector3){ 0, -0.8f, -3.0f}, (Vector3){0.6f, 2.8f,  6.2f}, c_light);  /* ricasso bevel highlight */
            /* Blade */
            DrawCubeV((Vector3){ 0,  0.3f,-10.0f}, (Vector3){0.9f, 3.8f, 13.0f}, c_steel);  /* blade spine */
            DrawCubeV((Vector3){ 0, -0.3f,-10.0f}, (Vector3){0.6f, 2.8f, 12.5f}, c_light);  /* blade face (bright) */
            DrawCubeV((Vector3){ 0, -1.5f,-10.5f}, (Vector3){0.5f, 1.2f, 11.5f}, c_dark);   /* blade edge bevel */
            DrawCubeV((Vector3){ 0,  0.8f,-15.5f}, (Vector3){0.7f, 2.2f,  5.0f}, c_steel);  /* false edge near tip */
            DrawCubeV((Vector3){ 0,  0.0f,-18.0f}, (Vector3){0.8f, 2.5f,  4.5f}, c_light);  /* blade tip */
            break;
        }

        default: break;
    }

    rlPopMatrix();
}

int main(int argc, char **argv)
{
    /* --- Arg parsing --- */
    const char *server_ip    = NULL;
    int         net_port     = NET_PORT;
    bool        is_server    = false;
    bool        server_testing = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0) {
            is_server = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
                net_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--testing") == 0) {
            server_testing = true;
        } else if (strcmp(argv[i], "--connect") == 0) {
            server_ip = "127.0.0.1";
            if (i + 1 < argc && argv[i + 1][0] != '-')
                server_ip = argv[++i];
        }
    }

    if (is_server) { server_run(net_port, server_testing); return 0; }

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

    WeaponState slots[3];
    weapon_init(&slots[0]);   /* primary - empty until bought */
    weapon_init(&slots[1]);   /* pistol - always have it */
    weapon_init(&slots[2]);   /* knife - always have it */
    weapon_switch(&slots[2], WEAPON_KNIFE);
    int  active_slot    = 1;  /* start on pistol */
    int  last_slot      = 1;  /* for Q quickswitch */
    bool has_primary    = false;
    bool awp_wants_zoom = false;

    BulletHole holes[MAX_HOLES];
    int     hole_count     = 0;
    Tracer  tracers[MAX_TRACERS];
    memset(tracers, 0, sizeof(tracers));
    int     tracer_head    = 0;
    int     shot_count     = 0;
    Vector3 gun_origin     = { 0 };
    Vector3 muzzle_pos     = { 0 };
    Vector3 gun_cam_fwd    = { 0, 0, -1 };
    Vector3 gun_cam_right  = { 1, 0,  0 };
    Vector3 gun_cam_up     = { 0, 1,  0 };
    float   gun_dev_yaw    = 0.0f;
    float   gun_dev_pitch  = 0.0f;

    uint8_t prev_shot_seq[MAX_PLAYERS];
    memset(prev_shot_seq, 0, sizeof(prev_shot_seq));
    bool    buy_menu_open  = false;
    bool    paused          = false;
    bool    settings_open   = false;
    bool    controls_open   = false;
    float   sensitivity     = MOUSE_SENSITIVITY;
    uint8_t prev_phase      = 0xFF;  /* for detecting PHASE_END → PHASE_BUY transition */
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

        bool testing = net.testing;
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
                    has_primary = true;
                }
                /* Always sync ammo from server packet */
                slots[0].ammo_mag     = (int)net.remote[net.my_id].ammo_mag;
                slots[0].ammo_reserve = (int)net.remote[net.my_id].ammo_reserve;
            } else {
                if (has_primary) weapon_switch(&slots[0], (WeaponId)0xFF);
                has_primary = false;
            }

            if ((WeaponId)srv_pistol != slots[1].weapon_id)
                weapon_switch(&slots[1], (WeaponId)srv_pistol);
            slots[1].ammo_mag     = (int)net.remote[net.my_id].ammo2_mag;
            slots[1].ammo_reserve = (int)net.remote[net.my_id].ammo2_reserve;

            if (net.round_phase == PHASE_BUY && prev_phase == PHASE_END) {
                weapon_round_reset(&slots[0]);
                weapon_round_reset(&slots[1]);
                weapon_round_reset(&slots[2]);
                slots[0].ammo_mag     = (int)net.remote[net.my_id].ammo_mag;
                slots[0].ammo_reserve = (int)net.remote[net.my_id].ammo_reserve;
                slots[1].ammo_mag     = (int)net.remote[net.my_id].ammo2_mag;
                slots[1].ammo_reserve = (int)net.remote[net.my_id].ammo2_reserve;
            }
        }
        prev_phase = net.round_phase;

        /* --- Active weapon and move factor --- */
        WeaponState *active_ws = &slots[active_slot < 3 ? active_slot : 2];
        bool awp_active = (active_slot == 0 && has_primary && slots[0].weapon_id == WEAPON_AWP);
        bool awp_zoomed = awp_active && awp_wants_zoom && slots[0].shot_timer == 0.0f;
        if (!awp_active) awp_wants_zoom = false;
        float move_factor = (active_slot == 2 || (active_slot == 0 && !has_primary))
                            ? 1.0f : WEAPONS[active_ws->weapon_id].move_factor;

        /* --- Lock cursor every frame so it can't escape the window (only when not paused) --- */
        if (!paused && IsWindowFocused() && !IsCursorHidden()) DisableCursor();

        /* --- Build PlayerInput --- */
        /* When zoomed, scale sens so physical mouse movement covers the same
         * world angle as unzoomed: factor = tan(half_zoom_fov) / tan(half_fov) */
        float sens = sensitivity * SOURCE_YAW;
        if (awp_zoomed)
            sens *= tanf(10.0f * DEG2RAD) / tanf(45.0f * DEG2RAD);  /* 20°/2 over 90°/2 */
        Vector2 md = paused ? (Vector2){0,0} : GetMouseDelta();
        PlayerInput input = {
            .yaw_delta   =  md.x * sens,
            .pitch_delta = -md.y * sens,
            .fwd    = IsKeyDown(KEY_W)          - IsKeyDown(KEY_S),
            .right  = IsKeyDown(KEY_D)          - IsKeyDown(KEY_A),
            .jump   = IsKeyPressed(KEY_SPACE) || (GetMouseWheelMove() < 0.0f),
            .crouch = IsKeyDown(KEY_LEFT_SHIFT),
            .max_speed = IsKeyDown(KEY_LEFT_CONTROL) ? 135.0f
                       : (move_factor < 1.0f) ? (PLAYER_SPEED_GROUND * move_factor) : 0.0f,
        };

        if (!am_dead && !round_end) {
            player_update(&player, &input, dt);
        }
        map_collide(&map, &player);
        Camera3D cam = player_camera(&player);
        if (awp_zoomed) cam.fovy = 20.0f;

        /* --- Gun viewmodel basis (screen-fixed: uses full camera basis so pitch doesn't shift it) --- */
        {
            float vm_yr = player.yaw   * DEG2RAD;
            float vm_pr = player.pitch * DEG2RAD;
            /* Full camera basis — also saved to outer vars for spray deviation */
            gun_cam_fwd   = (Vector3){  sinf(vm_yr)*cosf(vm_pr),  sinf(vm_pr), -cosf(vm_yr)*cosf(vm_pr) };
            gun_cam_right = (Vector3){  cosf(vm_yr),               0.0f,         sinf(vm_yr)             };
            gun_cam_up    = (Vector3){ -sinf(vm_yr)*sinf(vm_pr),   cosf(vm_pr),  cosf(vm_yr)*sinf(vm_pr) };
            /* Fixed camera-space offset: 30 forward, 32 right, 18 down */
            gun_origin = (Vector3){
                player.position.x + gun_cam_fwd.x*30.0f + gun_cam_right.x*32.0f - gun_cam_up.x*18.0f,
                player.position.y + gun_cam_fwd.y*30.0f + gun_cam_right.y*32.0f - gun_cam_up.y*18.0f,
                player.position.z + gun_cam_fwd.z*30.0f + gun_cam_right.z*32.0f - gun_cam_up.z*18.0f,
            };
        }

        /* --- Misc keys --- */
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        /* ESC: close buy menu first, then submenu, then toggle pause */
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (buy_menu_open) {
                buy_menu_open = false;
            } else if (controls_open) {
                controls_open = false;
            } else if (settings_open) {
                settings_open = false;
            } else {
                paused = !paused;
                if (paused) { EnableCursor(); }
                else { DisableCursor(); settings_open = false; controls_open = false; }
            }
        }

        /* --- Network: send input, receive world state --- */
        if (net.connected) {
            uint8_t buttons = 0;
            if (!paused && !am_dead) {
                if (IsKeyDown(KEY_W))            buttons |= BTN_FORWARD;
                if (IsKeyDown(KEY_S))            buttons |= BTN_BACK;
                if (IsKeyDown(KEY_A))            buttons |= BTN_LEFT;
                if (IsKeyDown(KEY_D))            buttons |= BTN_RIGHT;
                if (IsKeyDown(KEY_SPACE))        buttons |= BTN_JUMP;
                if (IsKeyDown(KEY_LEFT_SHIFT))   buttons |= BTN_CROUCH;
            }
            net_client_send_input(&net, buttons, player.yaw, player.pitch,
                                  player.position.x, player.position.y, player.position.z);
            net_client_recv(&net);

            am_dead   = net.remote[net.my_id].flags & 1;
            round_end = net.round_phase == PHASE_END;
            in_buy    = net.round_phase == PHASE_BUY;

            /* Spawn tracers for remote players when shot_seq changes */
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!net.remote[i].active || i == net.my_id) continue;
                if (net.remote[i].shot_seq == prev_shot_seq[i]) continue;
                prev_shot_seq[i] = net.remote[i].shot_seq;
                Vector3 origin = { net.remote[i].x, net.remote[i].y, net.remote[i].z };
                Vector3 dir    = { net.remote[i].shot_dx, net.remote[i].shot_dy, net.remote[i].shot_dz };
                RayHit  rh     = map_raycast(&map, origin, dir);
                Vector3 ep     = rh.hit
                    ? rh.point
                    : (Vector3){ origin.x + dir.x * 2000.0f,
                                 origin.y + dir.y * 2000.0f,
                                 origin.z + dir.z * 2000.0f };
                tracers[tracer_head % MAX_TRACERS] = (Tracer){ origin, ep, (float)GetTime() };
                tracer_head++;
            }
        }

        if (paused) goto draw;

        if (IsKeyPressed(KEY_R) && !am_dead && active_slot < 2)
            weapon_reload(active_ws);

        /* AWP zoom toggle */
        if (awp_active && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !am_dead)
            awp_wants_zoom = !awp_wants_zoom;

        /* Slot switching (only when buy menu is closed) */
        if (!buy_menu_open && !am_dead) {
            int new_slot = -1;
            if (IsKeyPressed(KEY_ONE) && has_primary)  new_slot = 0;
            else if (IsKeyPressed(KEY_TWO))             new_slot = 1;
            else if (IsKeyPressed(KEY_THREE))           new_slot = 2;
            else if (IsKeyPressed(KEY_Q)) {
                /* quickswitch: go to last slot if it differs, else toggle 0↔1 */
                new_slot = (last_slot != active_slot) ? last_slot
                         : (active_slot == 0 ? 1 : (has_primary ? 0 : 1));
            }
            if (new_slot >= 0 && new_slot != active_slot) {
                if (new_slot == 0 && !has_primary) new_slot = -1; /* no primary */
                if (new_slot >= 0) {
                    last_slot  = active_slot;
                    active_slot = new_slot;
                    if (new_slot != 0) awp_wants_zoom = false;
                    if (net.connected) net_client_equip(&net, (uint8_t)new_slot);
                }
            }
        }

        if (testing && IsKeyPressed(KEY_F5) && net.connected)
            net_client_end_round(&net);

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
        /* In testing mode, keep mags full so the client never auto-reloads */
        if (testing) {
            for (int s = 0; s < 3; s++) {
                if (slots[s].weapon_id < WEAPON_COUNT)
                    slots[s].ammo_reserve = WEAPONS[slots[s].weapon_id].ammo_reserve;
            }
        }
        weapon_tick(&slots[0], dt);
        weapon_tick(&slots[1], dt);
        weapon_tick(&slots[2], dt);

        /* --- Shoot --- */
        bool trigger_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool trigger_held    = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        Vector3 shot_dir;
        bool can_fire = active_slot < 3 && !(active_slot == 0 && !has_primary);
        float prev_shot_idx = active_ws->shot_index;
        bool shot_fired = !am_dead && !round_end && can_fire &&
            weapon_try_fire(active_ws, trigger_pressed, trigger_held, &player, &shot_dir);

        /* Viewmodel/Recoil tracking (updated after shooting so index is current) */
        {
            /* Target spray based on interpolated shot_index */
            int pat_len = 0;
            const SprayPoint *pat = NULL;
            switch (active_ws->weapon_id) {
                case WEAPON_AK:  pat = SPRAY_AK; pat_len = 30; break;
                case WEAPON_M4:  pat = SPRAY_M4; pat_len = 30; break;
                case WEAPON_AWP: pat = SPRAY_AWP; pat_len = 10; break;
                case WEAPON_PISTOL: pat = SPRAY_PISTOL; pat_len = 12; break;
                default: break;
            }

            float target_yaw = 0.0f, target_pitch = 0.0f;
            if (pat) {
                float sidx = active_ws->shot_index;
                int i0 = (int)sidx;
                int i1 = i0 + 1;
                if (i1 >= pat_len) i1 = pat_len - 1;
                if (i0 >= pat_len) i0 = pat_len - 1;
                float f = sidx - (float)i0;
                target_yaw   = pat[i0].yaw   * (1.0f - f) + pat[i1].yaw   * f;
                target_pitch = pat[i0].pitch * (1.0f - f) + pat[i1].pitch * f;
            }

            /* Snappy follow (100 Hz lerp speed) */
            float lerp_speed = 100.0f;
            gun_dev_yaw   += (target_yaw   - gun_dev_yaw)   * (1.0f - expf(-dt * lerp_speed));
            gun_dev_pitch += (target_pitch - gun_dev_pitch) * (1.0f - expf(-dt * lerp_speed));

            /* Muzzle position follows smooth gun deviation */
            static const float MUZZLE_DIST[WEAPON_COUNT] = { 18.0f, 44.0f, 42.0f, 57.0f, 18.0f };
            WeaponId cur_wid = active_ws->weapon_id;
            float mdist = (cur_wid < WEAPON_COUNT) ? MUZZLE_DIST[cur_wid] : 18.0f;

            float vm_yr = (player.yaw   + gun_dev_yaw)   * DEG2RAD;
            float vm_pr = (player.pitch + gun_dev_pitch) * DEG2RAD;
            Vector3 dev_fwd = { sinf(vm_yr)*cosf(vm_pr), sinf(vm_pr), -cosf(vm_yr)*cosf(vm_pr) };

            muzzle_pos = (Vector3){
                gun_origin.x + dev_fwd.x * mdist,
                gun_origin.y + dev_fwd.y * mdist,
                gun_origin.z + dev_fwd.z * mdist,
            };
        }

        if (shot_fired)
        {
            /* Movement inaccuracy — random cone spread based on speed */
            const WeaponDef *wdef = &WEAPONS[active_ws->weapon_id];
            float hspeed   = sqrtf(player.velocity.x * player.velocity.x +
                                   player.velocity.z * player.velocity.z);
            float spd_frac = fminf(hspeed / PLAYER_SPEED_GROUND, 1.0f);
            
            float inac;
            if (hspeed < 1.0f && player.on_ground) {
                /* Only the unscoped AWP has inaccuracy while standing still. 
                 * Everything else (and scoped AWP) is perfectly laser-accurate. */
                if (active_ws->weapon_id == WEAPON_AWP && !awp_zoomed) inac = wdef->inaccuracy_stand;
                else inac = 0.0f;
            } else {
                inac = wdef->inaccuracy_stand
                     + wdef->inaccuracy_move * spd_frac
                     + (!player.on_ground ? wdef->inaccuracy_air : 0.0f);
            }

            if (inac > 0.0f) {
                float phi = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
                float r   = sqrtf((float)rand() / (float)RAND_MAX) * inac * DEG2RAD;
                /* Recover angles from spray-modified direction, add inaccuracy */
                float yr = atan2f(shot_dir.x, -shot_dir.z) + r * cosf(phi);
                float pr = asinf(fmaxf(-1.0f, fminf(1.0f, shot_dir.y))) + r * sinf(phi);
                shot_dir.x =  sinf(yr) * cosf(pr);
                shot_dir.y =  sinf(pr);
                shot_dir.z = -cosf(yr) * cosf(pr);
            }

            /* Local visual: bullet hole + tracer */
            RayHit hit = map_raycast(&map, player.position, shot_dir);
            if (hit.hit && hole_count < MAX_HOLES) {
                holes[hole_count].pos    = hit.point;
                holes[hole_count].normal = hit.normal;
                hole_count++;
            }
            {
                Vector3 ep = hit.hit
                    ? hit.point
                    : (Vector3){ player.position.x + shot_dir.x * 2000.0f,
                                 player.position.y + shot_dir.y * 2000.0f,
                                 player.position.z + shot_dir.z * 2000.0f };
                {
                    WeaponId wid = active_ws->weapon_id;
                    bool first_shot = (prev_shot_idx < 0.5f);
                    bool spawn_tracer = false;
                    if (wid == WEAPON_PISTOL) {
                        spawn_tracer = true;
                    } else if (wid != WEAPON_AWP) {
                        /* Rifles: first shot of spray always, then every 3rd */
                        shot_count++;
                        spawn_tracer = first_shot || (shot_count % 3 == 0);
                    }
                    if (spawn_tracer) {
                        tracers[tracer_head % MAX_TRACERS] = (Tracer){ muzzle_pos, ep, (float)GetTime() };
                        tracer_head++;
                    }
                }
            }
            /* Network: send shot direction to server */
            if (net.connected)
                net_client_shoot(&net, shot_dir.x, shot_dir.y, shot_dir.z);
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
                    if      (fabsf(n.y) > 0.5f) size = (Vector3){ 2.0f, 0.2f, 2.0f };
                    else if (fabsf(n.x) > 0.5f) size = (Vector3){ 0.2f, 2.0f, 2.0f };
                    else                         size = (Vector3){ 2.0f, 2.0f, 0.2f };
                    DrawCubeV(p, size, (Color){ 12, 12, 12, 255 });
                }

                /* Tracers */
                {
                    float now3d = (float)GetTime();
                    for (int i = 0; i < MAX_TRACERS; i++) {
                        float age = now3d - tracers[i].born;
                        if (age < 0.0f || age >= TRACER_LIFE) continue;
                        unsigned char a = (unsigned char)((1.0f - age / TRACER_LIFE) * 200.0f);
                        DrawLine3D(tracers[i].origin, tracers[i].endpoint, (Color){220, 210, 170, a});
                    }
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

            /* Fixed viewmodel pass — separate camera that never moves, so gun is
             * glued to the screen corner regardless of pitch/yaw.              */
            if (!am_dead && !paused && !awp_zoomed &&
                active_slot < 3 && !(active_slot == 0 && !has_primary)) {
                Camera3D hud_cam = { 0 };
                hud_cam.position   = (Vector3){ 0.0f, 0.0f,  0.0f };
                hud_cam.target     = (Vector3){ 0.0f, 0.0f, -1.0f };
                hud_cam.up         = (Vector3){ 0.0f, 1.0f,  0.0f };
                hud_cam.fovy       = CAMERA_FOV;
                hud_cam.projection = CAMERA_PERSPECTIVE;
                BeginMode3D(hud_cam);
                    rlDrawRenderBatchActive();
                    rlDisableDepthTest();
                    /* Gun at fixed camera-space position: 32 right, 18 down, 30 forward */
                    draw_gun(active_ws->weapon_id, (Vector3){ 32.0f, -18.0f, -30.0f },
                             gun_dev_yaw, gun_dev_pitch, active_ws);
                    rlDrawRenderBatchActive();
                    rlEnableDepthTest();
                EndMode3D();
            }

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
            if (!am_dead && !paused && !awp_zoomed) {
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
                const char *s = "BUY PHASE  [B] open menu";
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
                    DrawText(player.crouching ? "CROUCH" : IsKeyDown(KEY_LEFT_CONTROL) ? "WALK" : "", 10, 52, 16, (Color){130,130,130,200});
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
                int bw = 340, bh = 70 + (WEAPON_COUNT - 1) * 36 + 28;
                int bx = cx - bw / 2, by = cy - bh / 2;
                DrawRectangle(bx, by, bw, bh, (Color){ 0, 0, 0, 220 });
                DrawRectangleLines(bx, by, bw, bh, GRAY);

                DrawText("BUY MENU", bx + 12, by + 12, 22, YELLOW);
                DrawText(TextFormat("Money: $%d", my_money), bx + 12, by + 36, 18, GREEN);

                for (int i = 0; i < WEAPON_COUNT - 1; i++) {  /* skip knife */
                    int wy = by + 68 + i * 36;
                    bool owned = (cur_weapon == (uint8_t)i);
                    bool affordable = !owned && (my_money >= WEAPONS[i].price);
                    Color wc = (owned || affordable) ? WHITE : DARKGRAY;

                    const char *action = owned ? "[equipped]"
                                       : (WEAPONS[i].price == 0 ? "FREE"
                                          : TextFormat("$%d", WEAPONS[i].price));

                    DrawText(TextFormat("[%d] %-8s  %d+%d  %s",
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

                if (controls_open) {
                    /* --- Controls page --- */
                    int pw = 380, ph = 380;
                    int px = cx - pw / 2, py = cy - ph / 2;
                    DrawRectangle(px, py, pw, ph, (Color){ 15, 15, 15, 245 });
                    DrawRectangleLines(px, py, pw, ph, (Color){ 80, 80, 80, 255 });

                    int tw = MeasureText("CONTROLS", 24);
                    DrawText("CONTROLS", cx - tw / 2, py + 14, 24, RAYWHITE);
                    DrawLine(px + 16, py + 46, px + pw - 16, py + 46, (Color){ 60, 60, 60, 255 });

                    /* Sensitivity row */
                    int ry = py + 60;
                    DrawText("Sensitivity", px + 20, ry, 18, LIGHTGRAY);

                    char sens_buf[16];
                    snprintf(sens_buf, sizeof(sens_buf), "%.2f", sensitivity);

                    /* Button layout: [<]  value  [>] right-aligned */
                    int btn_w = 36, btn_h = 22, val_w = 50;
                    int bx_right = px + pw - 20;
                    Rectangle btn_inc = { (float)(bx_right - btn_w),          (float)ry, (float)btn_w, (float)btn_h };
                    Rectangle val_box = { (float)(bx_right - btn_w - val_w),  (float)ry, (float)val_w, (float)btn_h };
                    Rectangle btn_dec = { (float)(bx_right - btn_w - val_w - btn_w), (float)ry, (float)btn_w, (float)btn_h };

                    Vector2 mouse = GetMousePosition();
                    bool dec_hover = CheckCollisionPointRec(mouse, btn_dec);
                    bool inc_hover = CheckCollisionPointRec(mouse, btn_inc);

                    DrawRectangleRec(btn_dec, dec_hover ? (Color){60,60,60,255} : (Color){35,35,35,255});
                    DrawRectangleRec(btn_inc, inc_hover ? (Color){60,60,60,255} : (Color){35,35,35,255});
                    DrawRectangleLinesEx(btn_dec, 1, (Color){80,80,80,255});
                    DrawRectangleLinesEx(btn_inc, 1, (Color){80,80,80,255});

                    int dw = MeasureText("<", 16); DrawText("<", (int)btn_dec.x + (btn_w - dw)/2, ry + 3, 16, LIGHTGRAY);
                    int iw = MeasureText(">", 16); DrawText(">", (int)btn_inc.x + (btn_w - iw)/2, ry + 3, 16, LIGHTGRAY);
                    int vw2 = MeasureText(sens_buf, 18);
                    DrawText(sens_buf, (int)val_box.x + (val_w - vw2)/2, ry + 2, 18, YELLOW);

                    bool clicked_dec = dec_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                    bool clicked_inc = inc_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                    if (IsKeyPressed(KEY_LEFT)  || clicked_dec)
                        sensitivity = fmaxf(0.1f, sensitivity - 0.1f);
                    if (IsKeyPressed(KEY_RIGHT) || clicked_inc)
                        sensitivity = fminf(10.0f, sensitivity + 0.1f);

                    /* Keybinds table */
                    DrawLine(px + 16, ry + 30, px + pw - 16, ry + 30, (Color){ 40, 40, 40, 255 });
                    static const char *binds[][2] = {
                        { "Move",        "W A S D"       },
                        { "Jump",        "Space / Scroll" },
                        { "Crouch",      "Shift"         },
                        { "Walk",        "Ctrl"          },
                        { "Shoot",       "LMB"           },
                        { "Scope (AWP)", "RMB"           },
                        { "Reload",      "R"             },
                        { "Quickswitch", "Q"             },
                        { "Slot 1/2/3",  "1 / 2 / 3"    },
                        { "Buy menu",    "B"             },
                        { "Fullscreen",  "F11"           },
                        { "Pause",       "ESC"           },
                    };
                    int num_binds = (int)(sizeof(binds) / sizeof(binds[0]));
                    for (int bi = 0; bi < num_binds; bi++) {
                        int by2 = ry + 42 + bi * 22;
                        DrawText(binds[bi][0], px + 20,       by2, 16, (Color){ 160, 160, 160, 255 });
                        int vw = MeasureText(binds[bi][1], 16);
                        DrawText(binds[bi][1], px + pw - 20 - vw, by2, 16, LIGHTGRAY);
                    }

                    DrawLine(px + 16, py + ph - 34, px + pw - 16, py + ph - 34, (Color){ 40, 40, 40, 255 });
                    tw = MeasureText("[ESC]  Back", 16);
                    DrawText("[ESC]  Back", cx - tw / 2, py + ph - 24, 16, (Color){ 100, 100, 100, 255 });

                } else if (settings_open) {
                    /* --- Settings page --- */
                    int pw = 300, ph = 160;
                    int px = cx - pw / 2, py = cy - ph / 2;
                    DrawRectangle(px, py, pw, ph, (Color){ 15, 15, 15, 245 });
                    DrawRectangleLines(px, py, pw, ph, (Color){ 80, 80, 80, 255 });

                    int tw = MeasureText("SETTINGS", 24);
                    DrawText("SETTINGS", cx - tw / 2, py + 14, 24, RAYWHITE);
                    DrawLine(px + 16, py + 46, px + pw - 16, py + 46, (Color){ 60, 60, 60, 255 });

                    Vector2 mouse2 = GetMousePosition();

                    /* Controls button */
                    Rectangle ctrl_r = { (float)(px + 20), (float)(py + 62), (float)(pw - 40), 30 };
                    bool ctrl_hov = CheckCollisionPointRec(mouse2, ctrl_r);
                    if (ctrl_hov) DrawRectangleRec(ctrl_r, (Color){40,40,40,200});
                    tw = MeasureText("Controls", 20);
                    DrawText("Controls", cx - tw / 2, py + 68, 20, ctrl_hov ? RAYWHITE : LIGHTGRAY);

                    /* Back button */
                    Rectangle back_r2 = { (float)(px + 20), (float)(py + 112), (float)(pw - 40), 26 };
                    bool back_hov2 = CheckCollisionPointRec(mouse2, back_r2);
                    if (back_hov2) DrawRectangleRec(back_r2, (Color){40,40,40,200});
                    tw = MeasureText("Back", 16);
                    DrawText("Back", cx - tw / 2, py + 118, 16, back_hov2 ? RAYWHITE : (Color){ 100, 100, 100, 255 });

                    if (IsKeyPressed(KEY_C) || (ctrl_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
                        controls_open = true;
                    if (back_hov2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                        settings_open = false;

                } else {
                    /* --- Main pause menu --- */
                    int pw = 300, ph = 200;
                    int px = cx - pw / 2, py = cy - ph / 2;
                    DrawRectangle(px, py, pw, ph, (Color){ 15, 15, 15, 240 });
                    DrawRectangleLines(px, py, pw, ph, (Color){ 80, 80, 80, 255 });

                    const char *title = "PAUSED";
                    int tw = MeasureText(title, 28);
                    DrawText(title, cx - tw / 2, py + 16, 28, RAYWHITE);
                    DrawLine(px + 16, py + 54, px + pw - 16, py + 54, (Color){ 60, 60, 60, 255 });

                    Vector2 mouse3 = GetMousePosition();

                    Rectangle res_r  = { (float)(px+20), (float)(py+62),  (float)(pw-40), 30 };
                    Rectangle set_r  = { (float)(px+20), (float)(py+98),  (float)(pw-40), 30 };
                    Rectangle quit_r = { (float)(px+20), (float)(py+148), (float)(pw-40), 30 };
                    bool res_hov  = CheckCollisionPointRec(mouse3, res_r);
                    bool set_hov  = CheckCollisionPointRec(mouse3, set_r);
                    bool quit_hov = CheckCollisionPointRec(mouse3, quit_r);

                    if (res_hov)  DrawRectangleRec(res_r,  (Color){40,40,40,200});
                    if (set_hov)  DrawRectangleRec(set_r,  (Color){40,40,40,200});
                    if (quit_hov) DrawRectangleRec(quit_r, (Color){60,20,20,200});

                    DrawText("Resume",   cx - MeasureText("Resume",   20)/2, py + 68,  20, res_hov  ? RAYWHITE : LIGHTGRAY);
                    DrawText("Settings", cx - MeasureText("Settings", 20)/2, py + 104, 20, set_hov  ? RAYWHITE : LIGHTGRAY);
                    DrawText("Quit",     cx - MeasureText("Quit",     20)/2, py + 154, 20, quit_hov ? (Color){255,100,100,255} : (Color){200,80,80,255});

                    bool lmb = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                    if (IsKeyPressed(KEY_S) || (set_hov  && lmb)) settings_open = true;
                    if (IsKeyPressed(KEY_Q) || (quit_hov && lmb)) { EndDrawing(); goto quit; }
                    if (res_hov && lmb) {
                        paused = false;
                        DisableCursor();
                        settings_open = false;
                        controls_open = false;
                    }
                }
            }

        EndDrawing();
    }

    quit:
    if (net.connected) net_client_disconnect(&net);
    weapon_free(&slots[0]);
    weapon_free(&slots[1]);
    weapon_free(&slots[2]);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
