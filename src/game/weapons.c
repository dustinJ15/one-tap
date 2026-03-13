#include "weapons.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Spray patterns
 *
 * Each entry is the CUMULATIVE offset (degrees) added to the aimed direction.
 * The player subtracts this with mouse movement to land shots on target.
 *
 * Rule: yaw and pitch offsets are never both non-zero in the same shot.
 *   Vertical phase  → yaw = 0, pitch grows.
 *   Horizontal phase → yaw alternates, pitch locked at vertical-phase peak.
 * --------------------------------------------------------------------------- */

/* Pistol (12): gentle vertical rise only, semi-auto so no horizontal needed */
const SprayPoint SPRAY_PISTOL[12] = {
    { 0.0f, 0.0f }, { 0.0f, 0.8f }, { 0.0f, 1.5f }, { 0.0f, 2.0f },
    { 0.0f, 2.3f }, { 0.0f, 2.5f }, { 0.0f, 2.5f }, { 0.0f, 2.5f },
    { 0.0f, 2.5f }, { 0.0f, 2.5f }, { 0.0f, 2.5f }, { 0.0f, 2.5f },
};

/* AK (30) — CS:GO inspired, pure-axis constraint:
 *   shots  0-1  — perfectly straight (0,0), two-tap stays centered
 *   shots  2-8  — steep front-loaded vertical rise to 8.5°
 *   shots  9-29 — horizontal reverse-J: sweeps LEFT to -3.2°, curves
 *                 back RIGHT settling at +1.8° (iconic CS:GO AK shape) */
const SprayPoint SPRAY_AK[30] = {
    /* first two shots: dead center */
    {  0.00f, 0.0f }, {  0.00f, 0.0f },
    /* vertical rise — front-loaded */
    {  0.00f, 1.5f }, {  0.00f, 3.2f }, {  0.00f, 5.0f },
    {  0.00f, 6.5f }, {  0.00f, 7.5f }, {  0.00f, 8.2f }, {  0.00f, 8.5f },
    /* horizontal phase — left sweep then right curve */
    {  0.00f, 8.5f }, { -0.50f, 8.5f }, { -1.20f, 8.5f }, { -2.00f, 8.5f },
    { -2.80f, 8.5f }, { -3.20f, 8.5f }, { -3.00f, 8.5f }, { -2.50f, 8.5f },
    { -1.80f, 8.5f }, { -1.00f, 8.5f }, { -0.30f, 8.5f }, {  0.30f, 8.5f },
    {  0.80f, 8.5f }, {  1.20f, 8.5f }, {  1.50f, 8.5f }, {  1.70f, 8.5f },
    {  1.80f, 8.5f }, {  1.80f, 8.5f }, {  1.80f, 8.5f }, {  1.80f, 8.5f },
    {  1.80f, 8.5f },
};

/* M4 (30) — CS:GO inspired, pure-axis constraint:
 *   shots  0-1  — perfectly straight (0,0), two-tap stays centered
 *   shots  2-8  — softer front-loaded vertical rise to 6.0°
 *   shots  9-29 — horizontal: drifts RIGHT to +1.8°, settles at +0.3° */
const SprayPoint SPRAY_M4[30] = {
    /* first two shots: dead center */
    {  0.00f, 0.0f }, {  0.00f, 0.0f },
    /* vertical rise — softer than AK */
    {  0.00f, 1.2f }, {  0.00f, 2.5f }, {  0.00f, 3.8f },
    {  0.00f, 5.0f }, {  0.00f, 5.7f }, {  0.00f, 6.0f }, {  0.00f, 6.0f },
    /* horizontal phase — right drift then partial return */
    {  0.00f, 6.0f }, {  0.40f, 6.0f }, {  0.90f, 6.0f }, {  1.40f, 6.0f },
    {  1.70f, 6.0f }, {  1.80f, 6.0f }, {  1.70f, 6.0f }, {  1.50f, 6.0f },
    {  1.20f, 6.0f }, {  0.90f, 6.0f }, {  0.60f, 6.0f }, {  0.40f, 6.0f },
    {  0.30f, 6.0f }, {  0.30f, 6.0f }, {  0.30f, 6.0f }, {  0.30f, 6.0f },
    {  0.30f, 6.0f }, {  0.30f, 6.0f }, {  0.30f, 6.0f }, {  0.30f, 6.0f },
    {  0.30f, 6.0f },
};

/* AWP (10): first shot perfect; subsequent shots have massive kick.
 * In practice the 1.5 s fire_rate > SPRAY_RESET_TIME (0.4 s), so the pattern
 * always resets between shots — giving perfect first-shot accuracy every time.
 * The large values here are a safety net and enforce the bolt-action feel if
 * somehow fired rapidly. */
const SprayPoint SPRAY_AWP[10] = {
    {  0.0f,  0.0f }, {  0.0f, 20.0f }, {  0.0f, 40.0f }, {  0.0f, 60.0f },
    {  0.0f, 80.0f }, {  0.0f, 80.0f }, {  0.0f, 80.0f }, {  0.0f, 80.0f },
    {  0.0f, 80.0f }, {  0.0f, 80.0f },
};

/* ---------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------------- */

static const SprayPoint *get_pattern(WeaponId id, int *out_len)
{
    switch (id) {
        case WEAPON_AK:  *out_len = 30; return SPRAY_AK;
        case WEAPON_M4:  *out_len = 30; return SPRAY_M4;
        case WEAPON_AWP: *out_len = 10; return SPRAY_AWP;
        default:         *out_len = 12; return SPRAY_PISTOL;
    }
}

static Sound make_gunshot_sound(void)
{
    const unsigned int RATE  = 44100;
    const float        DUR   = 0.22f;
    unsigned int       count = (unsigned int)(RATE * DUR);
    short *buf = RL_MALLOC(count * sizeof(short));
    for (unsigned int i = 0; i < count; i++) {
        float t     = (float)i / (float)count;
        float env   = expf(-t * 14.0f);
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        buf[i] = (short)(env * noise * 18000.0f);
    }
    Wave  w = { count, RATE, 16, 1, buf };
    Sound s = LoadSoundFromWave(w);
    RL_FREE(buf);
    SetSoundVolume(s, 0.6f);
    return s;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void weapon_init(WeaponState *w)
{
    memset(w, 0, sizeof(*w));
    w->sound = make_gunshot_sound();
    weapon_switch(w, WEAPON_PISTOL);
}

void weapon_free(WeaponState *w)
{
    UnloadSound(w->sound);
}

void weapon_switch(WeaponState *w, WeaponId id)
{
    const WeaponDef *def = &WEAPONS[id];
    w->weapon_id    = id;
    w->ammo_mag     = def->ammo_mag;
    w->ammo_reserve = def->ammo_reserve;
    w->shot_index   = 0.0f;
    w->reset_timer  = 0.0f;
    w->shot_timer   = 0.0f;
    w->reload_timer = 0.0f;
}

void weapon_round_reset(WeaponState *w)
{
    const WeaponDef *def = &WEAPONS[w->weapon_id];
    w->ammo_mag     = def->ammo_mag;
    w->ammo_reserve = def->ammo_reserve;
    w->shot_index   = 0.0f;
    w->reset_timer  = 0.0f;
    w->shot_timer   = 0.0f;
    w->reload_timer = 0.0f;
}

void weapon_reload(WeaponState *w)
{
    const WeaponDef *def = &WEAPONS[w->weapon_id];
    if (w->reload_timer > 0.0f)       return;
    if (w->ammo_mag >= def->ammo_mag)  return;
    if (w->ammo_reserve <= 0)          return;
    w->reload_timer = def->reload_time;
}

void weapon_tick(WeaponState *w, float dt)
{
    if (w->shot_timer > 0.0f) {
        w->shot_timer -= dt;
        if (w->shot_timer < 0.0f) w->shot_timer = 0.0f;
    }

    if (w->reload_timer > 0.0f) {
        w->reload_timer -= dt;
        if (w->reload_timer <= 0.0f) {
            const WeaponDef *def = &WEAPONS[w->weapon_id];
            int needed = def->ammo_mag - w->ammo_mag;
            if (needed > w->ammo_reserve) needed = w->ammo_reserve;
            w->ammo_mag     += needed;
            w->ammo_reserve -= needed;
            w->reload_timer  = 0.0f;
        }
    }

    /* Gradual spray recovery: after SPRAY_DECAY_DELAY of inactivity, decay
     * shot_index continuously at SPRAY_DECAY_RATE indices/sec toward 0.
     * The delay exceeds the AK fire interval (0.10s) so full-auto never
     * self-resets mid-spray. */
    if (w->shot_index > 0.0f) {
        w->reset_timer += dt;
        if (w->reset_timer > SPRAY_DECAY_DELAY) {
            w->shot_index -= SPRAY_DECAY_RATE * dt;
            if (w->shot_index < 0.0f) w->shot_index = 0.0f;
        }
    }
}

bool weapon_try_fire(WeaponState *w, bool trigger_pressed, bool trigger_held,
                     const PlayerState *p, Vector3 *out_dir)
{
    const WeaponDef *def = &WEAPONS[w->weapon_id];

    bool fire = def->semi_auto ? trigger_pressed : trigger_held;
    if (!fire)                  return false;
    if (w->shot_timer > 0.0f)  return false;
    if (w->reload_timer > 0.0f) return false;
    if (w->ammo_mag <= 0)       return false;

    /* Look up spray offset for this shot (floor float index) */
    int               pat_len;
    const SprayPoint *pat = get_pattern(w->weapon_id, &pat_len);
    int               idx = (int)w->shot_index;
    if (idx >= pat_len) idx = pat_len - 1;

    float yr = (p->yaw   + pat[idx].yaw)   * DEG2RAD;
    float pr = (p->pitch + pat[idx].pitch)  * DEG2RAD;

    out_dir->x =  sinf(yr) * cosf(pr);
    out_dir->y =  sinf(pr);
    out_dir->z = -cosf(yr) * cosf(pr);

    PlaySound(w->sound);
    w->ammo_mag--;
    w->shot_timer  = def->fire_rate;
    w->reset_timer = 0.0f;      /* restart inactivity clock */
    w->shot_index += 1.0f;

    /* Auto-reload when mag empties */
    if (w->ammo_mag == 0) weapon_reload(w);

    return true;
}
