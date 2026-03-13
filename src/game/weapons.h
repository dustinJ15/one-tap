#ifndef WEAPONS_H
#define WEAPONS_H

#include "raylib.h"
#include "../render/map.h"
#include "../physics/movement.h"
#include "economy.h"

/* Delay after last shot before recovery begins. Must exceed the fastest
 * rifle fire_rate (AK=0.10s) so full-auto spray never self-resets. */
#define SPRAY_DECAY_DELAY  0.12f
/* Pattern indices recovered per second once decay starts (~2s for full 30-shot reset). */
#define SPRAY_DECAY_RATE   15.0f

/*
 * Spray pattern: cumulative (yaw, pitch) offset in degrees added to the shot
 * direction.  The player cancels these offsets with mouse movement to land
 * shots on target.
 *
 * Design rule: vertical and horizontal phases never overlap.
 *   Phase 1 — pure vertical rise (yaw offset = 0 throughout).
 *   Phase 2 — pure horizontal zigzag (pitch offset fixed at phase-1 peak).
 * The transition is a clean right angle; tracing it is the learnable skill.
 */
typedef struct { float yaw; float pitch; } SprayPoint;

extern const SprayPoint SPRAY_PISTOL[12];
extern const SprayPoint SPRAY_AK[30];
extern const SprayPoint SPRAY_M4[30];
extern const SprayPoint SPRAY_AWP[10];

typedef struct {
    Sound    sound;
    WeaponId weapon_id;
    int      ammo_mag;
    int      ammo_reserve;
    float    shot_timer;    /* time until next shot allowed */
    float    reset_timer;   /* time since last shot (for pattern reset) */
    float    reload_timer;  /* > 0 while reloading */
    float    shot_index;    /* position in spray pattern (float for smooth decay) */
} WeaponState;

void   weapon_init(WeaponState *w);
void   weapon_free(WeaponState *w);
void   weapon_switch(WeaponState *w, WeaponId id);   /* change weapon, reset ammo+state */
void   weapon_round_reset(WeaponState *w);            /* refill ammo, reset spray state */
void   weapon_tick(WeaponState *w, float dt);         /* update all timers */
void   weapon_reload(WeaponState *w);                 /* start reload (if applicable) */

/*
 * Try to fire.  trigger_pressed = rising edge (for semi-auto),
 * trigger_held = button held (for full-auto).
 * Returns true and writes the shot direction into *out_dir on success.
 */
bool   weapon_try_fire(WeaponState *w, bool trigger_pressed, bool trigger_held,
                       const PlayerState *p, Vector3 *out_dir);

#endif
