#include "weapons.h"
#include <math.h>
#include <stdlib.h>

/*
 * Generate a procedural gunshot: white noise with a fast exponential decay.
 * Sounds like a dry crack — good enough until real audio assets exist.
 */
static Sound make_gunshot_sound(void)
{
    const unsigned int RATE  = 44100;
    const float        DUR   = 0.22f;
    unsigned int       count = (unsigned int)(RATE * DUR);
    short *buf = RL_MALLOC(count * sizeof(short));

    for (unsigned int i = 0; i < count; i++) {
        float t     = (float)i / (float)count;
        float env   = expf(-t * 14.0f);   /* sharp crack, quick decay */
        float noise = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        buf[i] = (short)(env * noise * 18000.0f);
    }

    Wave  w = { count, RATE, 16, 1, buf };
    Sound s = LoadSoundFromWave(w);
    RL_FREE(buf);
    SetSoundVolume(s, 0.6f);
    return s;
}

void weapon_init(WeaponState *w)
{
    w->sound = make_gunshot_sound();
}

void weapon_free(WeaponState *w)
{
    UnloadSound(w->sound);
}

RayHit weapon_shoot(const WeaponState *w, const Map *m, const PlayerState *p)
{
    /* Apply random spread to yaw and pitch */
    float spread = WEAPON_SPREAD * DEG2RAD;
    float yr = p->yaw   * DEG2RAD + ((float)rand() / RAND_MAX * 2.0f - 1.0f) * spread;
    float pr = p->pitch * DEG2RAD + ((float)rand() / RAND_MAX * 2.0f - 1.0f) * spread;

    Vector3 dir = {
         sinf(yr) * cosf(pr),
         sinf(pr),
        -cosf(yr) * cosf(pr)
    };

    PlaySound(w->sound);
    return map_raycast(m, p->position, dir);
}
